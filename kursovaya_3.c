#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>    // для ioctl
#include <net/if.h>       // для struct ifreq

// Функция для проверки, является ли адрес loopback
int is_loopback_address(const struct sockaddr *addr) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        return (ntohl(addr_in->sin_addr.s_addr) & 0xFF000000) == 0x7F000000;
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        return IN6_IS_ADDR_LOOPBACK(&addr_in6->sin6_addr);
    }
    return 0;
}

// Функция для получения MAC-адреса по имени интерфейса
int get_mac_address(const char *iface_name, unsigned char mac[6]) {
    struct ifreq ifr;
    int sock;

    // Создаем сокет для выполнения ioctl 
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // Копируем имя интерфейса в структуру
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // Запрашиваем MAC-адрес через ioctl
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return -1;
    }

    // Копируем MAC-адрес из результата
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(sock);
    return 0;
}

// Функция для вывода MAC-адреса в формате xx:xx:xx:xx:xx:xx
void print_mac_address(const unsigned char mac[6]) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int main() {
    char hostname[256];
    struct ifaddrs *ifaddrs_ptr, *ifa;
    char ip_str[INET6_ADDRSTRLEN];
    unsigned char mac[6];
    int found = 0;
    const char *selected_iface = NULL;
    int ip_family = 0; // AF_INET или AF_INET6

    // Получаем сетевое имя компьютера
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("Ошибка при получении имени хоста");
        return EXIT_FAILURE;
    }

    // Получаем список сетевых интерфейсов
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        perror("Ошибка при получении адресов интерфейсов");
        return EXIT_FAILURE;
    }

    // Проходим по списку интерфейсов
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        // Проверяем, что это IPv4 или IPv6
        if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) {
            if (is_loopback_address(ifa->ifa_addr)) {
                continue;
            }

            void *addr_ptr;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                addr_ptr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            } else { // AF_INET6
                addr_ptr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
            }
            // Конвертируем бинарный адрес в строку
            if (inet_ntop(ifa->ifa_addr->sa_family, addr_ptr, ip_str, sizeof(ip_str)) != NULL) {
                selected_iface = ifa->ifa_name; // Сохраняем имя интерфейса для получения MAC-адреса
                ip_family = ifa->ifa_addr->sa_family; // Сохраняем тип адреса
                found = 1;
                break; // Берем первый подходящий адрес
            }
        }
    }

    freeifaddrs(ifaddrs_ptr);

    if (!found) {
        fprintf(stderr, "Не удалось найти подходящий IP-адрес (не-loopback).\n");
        return EXIT_FAILURE;
    }

    // Пытаемся получить MAC-адрес для выбранного интерфейса
    int mac_success = (get_mac_address(selected_iface, mac) == 0);

    // Выводим результаты
    printf("Сетевое имя: %s\n", hostname);
    
    const char *ip_type = (ip_family == AF_INET) ? "IPv4" : "IPv6";
    printf("IP-адрес (%s): %s\n", ip_type, ip_str);
    
    if (mac_success) {
        printf("MAC-адрес: ");
        print_mac_address(mac);
        printf("\n");
    } else {
        printf("MAC-адрес: недоступен\n");
    }

    return EXIT_SUCCESS;
}

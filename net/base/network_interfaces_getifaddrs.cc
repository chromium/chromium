// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces_getifaddrs.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>

#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces_posix.h"

#if BUILDFLAG(IS_MAC)
#include <net/if_media.h>
#include <netinet/in_var.h>
#include <sys/ioctl.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "net/base/network_interfaces_getifaddrs_android.h"
// Declare getifaddrs() and freeifaddrs() weakly as they're only available
// on Android N+.
extern "C" {
int getifaddrs(struct ifaddrs** __list_ptr) __attribute__((weak_import));
void freeifaddrs(struct ifaddrs* __ptr) __attribute__((weak_import));
}
#endif  // BUILDFLAG(IS_ANDROID)

namespace net {
namespace internal {

#if BUILDFLAG(IS_MAC)

// MacOSX implementation of IPAttributesGetter which calls ioctl() on socket to
// retrieve IP attributes.
class IPAttributesGetterMac : public internal::IPAttributesGetter {
 public:
  IPAttributesGetterMac();
  ~IPAttributesGetterMac() override;
  bool IsInitialized() const override;
  bool GetAddressAttributes(const ifaddrs* if_addr, int* attributes) override;
  NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(
      const ifaddrs* if_addr) override;

 private:
  int ioctl_socket_;
};

IPAttributesGetterMac::IPAttributesGetterMac()
    : ioctl_socket_(socket(AF_INET6, SOCK_DGRAM, 0)) {
  DCHECK_GE(ioctl_socket_, 0);
}

IPAttributesGetterMac::~IPAttributesGetterMac() {
  if (IsInitialized()) {
    PCHECK(IGNORE_EINTR(close(ioctl_socket_)) == 0);
  }
}

bool IPAttributesGetterMac::IsInitialized() const {
  return ioctl_socket_ >= 0;
}

int AddressFlagsToNetAddressAttributes(int flags) {
  int result = 0;
  if (flags & IN6_IFF_TEMPORARY) {
    result |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
  }
  if (flags & IN6_IFF_DEPRECATED) {
    result |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
  }
  if (flags & IN6_IFF_ANYCAST) {
    result |= IP_ADDRESS_ATTRIBUTE_ANYCAST;
  }
  if (flags & IN6_IFF_TENTATIVE) {
    result |= IP_ADDRESS_ATTRIBUTE_TENTATIVE;
  }
  if (flags & IN6_IFF_DUPLICATED) {
    result |= IP_ADDRESS_ATTRIBUTE_DUPLICATED;
  }
  if (flags & IN6_IFF_DETACHED) {
    result |= IP_ADDRESS_ATTRIBUTE_DETACHED;
  }
  return result;
}

bool IPAttributesGetterMac::GetAddressAttributes(const ifaddrs* if_addr,
                                                 int* attributes) {
  struct in6_ifreq ifr = {};
  strncpy(ifr.ifr_name, if_addr->ifa_name, sizeof(ifr.ifr_name) - 1);
  memcpy(&ifr.ifr_ifru.ifru_addr, if_addr->ifa_addr, if_addr->ifa_addr->sa_len);
  int rv = ioctl(ioctl_socket_, SIOCGIFAFLAG_IN6, &ifr);
  if (rv >= 0) {
    *attributes = AddressFlagsToNetAddressAttributes(ifr.ifr_ifru.ifru_flags);
  }
  return (rv >= 0);
}

NetworkChangeNotifier::ConnectionType
IPAttributesGetterMac::GetNetworkInterfaceType(const ifaddrs* if_addr) {
  if (!IsInitialized())
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;

  struct ifmediareq ifmr = {};
  strncpy(ifmr.ifm_name, if_addr->ifa_name, sizeof(ifmr.ifm_name) - 1);

  if (ioctl(ioctl_socket_, SIOCGIFMEDIA, &ifmr) != -1) {
    if (ifmr.ifm_current & IFM_IEEE80211) {
      return NetworkChangeNotifier::CONNECTION_WIFI;
    }
    if (ifmr.ifm_current & IFM_ETHER) {
      return NetworkChangeNotifier::CONNECTION_ETHERNET;
    }
  }

  return NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

#endif  // BUILDFLAG(IS_MAC)

bool IfaddrsToNetworkInterfaceList(int policy,
                                   const ifaddrs* interfaces,
                                   IPAttributesGetter* ip_attributes_getter,
                                   NetworkInterfaceList* networks) {
  // Enumerate the addresses assigned to network interfaces which are up.
  for (const ifaddrs* interface = interfaces; interface != nullptr;
       interface = interface->ifa_next) {
    // Skip loopback interfaces, and ones which are down.
    if (!(IFF_UP & interface->ifa_flags)) {
      continue;
    }
    if (!(IFF_RUNNING & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    // Skip interfaces with no address configured.
    struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    // configured on non-loopback interfaces.
    if (IsLoopbackOrUnspecifiedAddress(addr))
      continue;

    std::string name = interface->ifa_name;
    // Filter out VMware interfaces, typically named vmnet1 and vmnet8.
    if (ShouldIgnoreInterface(name, policy)) {
      continue;
    }

    NetworkChangeNotifier::ConnectionType connection_type =
        NetworkChangeNotifier::CONNECTION_UNKNOWN;

    int ip_attributes = IP_ADDRESS_ATTRIBUTE_NONE;

    // Retrieve native ip attributes and convert to net version if a getter is
    // given.
    if (ip_attributes_getter && ip_attributes_getter->IsInitialized()) {
      if (addr->sa_family == AF_INET6 &&
          ip_attributes_getter->GetAddressAttributes(interface,
                                                     &ip_attributes)) {
        // Disallow addresses with attributes ANYCASE, DUPLICATED, TENTATIVE,
        // and DETACHED as these are still progressing through duplicated
        // address detection (DAD) or are not suitable to be used in an
        // one-to-one communication and shouldn't be used by the application
        // layer.
        if (ip_attributes &
            (IP_ADDRESS_ATTRIBUTE_ANYCAST | IP_ADDRESS_ATTRIBUTE_DUPLICATED |
             IP_ADDRESS_ATTRIBUTE_TENTATIVE | IP_ADDRESS_ATTRIBUTE_DETACHED)) {
          continue;
        }
      }

      connection_type =
          ip_attributes_getter->GetNetworkInterfaceType(interface);
    }

    IPEndPoint address;

    int addr_size = 0;
    if (addr->sa_family == AF_INET6) {
      addr_size = sizeof(sockaddr_in6);
    } else if (addr->sa_family == AF_INET) {
      addr_size = sizeof(sockaddr_in);
    }

    if (address.FromSockAddr(addr, addr_size)) {
      uint8_t prefix_length = 0;
      if (interface->ifa_netmask) {
        // If not otherwise set, assume the same sa_family as ifa_addr.
        if (interface->ifa_netmask->sa_family == 0) {
          interface->ifa_netmask->sa_family = addr->sa_family;
        }
        IPEndPoint netmask;
        if (netmask.FromSockAddr(interface->ifa_netmask, addr_size)) {
          prefix_length = MaskPrefixLength(netmask.address());
        }
      }
      networks->push_back(NetworkInterface(
          name, name, if_nametoindex(name.c_str()), connection_type,
          address.address(), prefix_length, ip_attributes));
    }
  }

  return true;
}

}  // namespace internal

// This version of GetNetworkList() can only be called on Android N+, so give it
// a different and internal name so it isn't invoked mistakenly.
#if BUILDFLAG(IS_ANDROID)
namespace internal {
bool GetNetworkListUsingGetifaddrs(NetworkInterfaceList* networks,
                                   int policy,
                                   bool use_alternative_getifaddrs) {
  DCHECK_GE(base::android::BuildInfo::GetInstance()->sdk_int(),
            base::android::SDK_VERSION_NOUGAT);
  DCHECK(getifaddrs);
  DCHECK(freeifaddrs);
#else
bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  constexpr bool use_alternative_getifaddrs = false;
#endif
  if (networks == nullptr)
    return false;

  // getifaddrs() may require IO operations.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ifaddrs* interfaces;
  int getifaddrs_result;
  if (use_alternative_getifaddrs) {
#if BUILDFLAG(IS_ANDROID)
    // Chromium ships its own implementation of getifaddrs()
    // under the name Getifaddrs.
    getifaddrs_result = Getifaddrs(&interfaces);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else {
    getifaddrs_result = getifaddrs(&interfaces);
  }
  if (getifaddrs_result < 0) {
    PLOG(ERROR) << "getifaddrs";
    return false;
  }

  std::unique_ptr<internal::IPAttributesGetter> ip_attributes_getter;

#if BUILDFLAG(IS_MAC)
  ip_attributes_getter = std::make_unique<internal::IPAttributesGetterMac>();
#endif

  bool result = internal::IfaddrsToNetworkInterfaceList(
      policy, interfaces, ip_attributes_getter.get(), networks);

  if (use_alternative_getifaddrs) {
#if BUILDFLAG(IS_ANDROID)
    Freeifaddrs(interfaces);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else {
    freeifaddrs(interfaces);
  }
  return result;
}

#if BUILDFLAG(IS_ANDROID)
}  // namespace internal
// For Android use GetWifiSSID() impl in network_interfaces_linux.cc.
#else
std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return std::string();
}
#endif

}  // namespace net

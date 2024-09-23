// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/address_sorter_posix.h"

#include <netinet/in.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_BSD)
#include <sys/socket.h>  // Must be included before ifaddrs.h.
#include <ifaddrs.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#if BUILDFLAG(IS_IOS)
// The code in the following header file is copied from [1]. This file has the
// minimum definitions needed to retrieve the IP attributes, since iOS SDK
// doesn't include a necessary header <netinet/in_var.h>.
// [1] https://chromium.googlesource.com/external/webrtc/+/master/rtc_base/mac_ifaddrs_converter.cc
#include "net/dns/netinet_in_var_ios.h"
#else
#include <netinet/in_var.h>
#endif  // BUILDFLAG(IS_IOS)
#endif
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "net/base/address_tracker_linux.h"
#endif

namespace net {
namespace {
// Address sorting is performed according to RFC3484 with revisions.
// http://tools.ietf.org/html/draft-ietf-6man-rfc3484bis-06
// Precedence and label are separate to support override through
// /etc/gai.conf.

// Returns true if |p1| should precede |p2| in the table.
// Sorts table by decreasing prefix size to allow longest prefix matching.
bool ComparePolicy(const AddressSorterPosix::PolicyEntry& p1,
                   const AddressSorterPosix::PolicyEntry& p2) {
  return p1.prefix_length > p2.prefix_length;
}

// Creates sorted PolicyTable from |table| with |size| entries.
AddressSorterPosix::PolicyTable LoadPolicy(
    const AddressSorterPosix::PolicyEntry* table,
    size_t size) {
  AddressSorterPosix::PolicyTable result(table, table + size);
  std::sort(result.begin(), result.end(), ComparePolicy);
  return result;
}

// Search |table| for matching prefix of |address|. |table| must be sorted by
// descending prefix (prefix of another prefix must be later in table).
unsigned GetPolicyValue(const AddressSorterPosix::PolicyTable& table,
                        const IPAddress& address) {
  if (address.IsIPv4())
    return GetPolicyValue(table, ConvertIPv4ToIPv4MappedIPv6(address));
  for (const auto& entry : table) {
    IPAddress prefix(entry.prefix);
    if (IPAddressMatchesPrefix(address, prefix, entry.prefix_length))
      return entry.value;
  }
  NOTREACHED_IN_MIGRATION();
  // The last entry is the least restrictive, so assume it's default.
  return table.back().value;
}

bool IsIPv6Multicast(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  return address.bytes()[0] == 0xFF;
}

AddressSorterPosix::AddressScope GetIPv6MulticastScope(
    const IPAddress& address) {
  DCHECK(address.IsIPv6());
  return static_cast<AddressSorterPosix::AddressScope>(address.bytes()[1] &
                                                       0x0F);
}

bool IsIPv6Loopback(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  return address == IPAddress::IPv6Localhost();
}

bool IsIPv6LinkLocal(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  // IN6_IS_ADDR_LINKLOCAL
  return (address.bytes()[0] == 0xFE) && ((address.bytes()[1] & 0xC0) == 0x80);
}

bool IsIPv6SiteLocal(const IPAddress& address) {
  DCHECK(address.IsIPv6());
  // IN6_IS_ADDR_SITELOCAL
  return (address.bytes()[0] == 0xFE) && ((address.bytes()[1] & 0xC0) == 0xC0);
}

AddressSorterPosix::AddressScope GetScope(
    const AddressSorterPosix::PolicyTable& ipv4_scope_table,
    const IPAddress& address) {
  if (address.IsIPv6()) {
    if (IsIPv6Multicast(address)) {
      return GetIPv6MulticastScope(address);
    } else if (IsIPv6Loopback(address) || IsIPv6LinkLocal(address)) {
      return AddressSorterPosix::SCOPE_LINKLOCAL;
    } else if (IsIPv6SiteLocal(address)) {
      return AddressSorterPosix::SCOPE_SITELOCAL;
    } else {
      return AddressSorterPosix::SCOPE_GLOBAL;
    }
  } else if (address.IsIPv4()) {
    return static_cast<AddressSorterPosix::AddressScope>(
        GetPolicyValue(ipv4_scope_table, address));
  } else {
    NOTREACHED_IN_MIGRATION();
    return AddressSorterPosix::SCOPE_NODELOCAL;
  }
}

// Default policy table. RFC 3484, Section 2.1.
const AddressSorterPosix::PolicyEntry kDefaultPrecedenceTable[] = {
    // ::1/128 -- loopback
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 128, 50},
    // ::/0 -- any
    {{}, 0, 40},
    // ::ffff:0:0/96 -- IPv4 mapped
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF}, 96, 35},
    // 2002::/16 -- 6to4
    {{
         0x20,
         0x02,
     },
     16,
     30},
    // 2001::/32 -- Teredo
    {{0x20, 0x01, 0, 0}, 32, 5},
    // fc00::/7 -- unique local address
    {{0xFC}, 7, 3},
    // ::/96 -- IPv4 compatible
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 96, 1},
    // fec0::/10 -- site-local expanded scope
    {{0xFE, 0xC0}, 10, 1},
    // 3ffe::/16 -- 6bone
    {{0x3F, 0xFE}, 16, 1},
};

const AddressSorterPosix::PolicyEntry kDefaultLabelTable[] = {
    // ::1/128 -- loopback
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 128, 0},
    // ::/0 -- any
    {{}, 0, 1},
    // ::ffff:0:0/96 -- IPv4 mapped
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF}, 96, 4},
    // 2002::/16 -- 6to4
    {{
         0x20,
         0x02,
     },
     16,
     2},
    // 2001::/32 -- Teredo
    {{0x20, 0x01, 0, 0}, 32, 5},
    // fc00::/7 -- unique local address
    {{0xFC}, 7, 13},
    // ::/96 -- IPv4 compatible
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 96, 3},
    // fec0::/10 -- site-local expanded scope
    {{0xFE, 0xC0}, 10, 11},
    // 3ffe::/16 -- 6bone
    {{0x3F, 0xFE}, 16, 12},
};

// Default mapping of IPv4 addresses to scope.
const AddressSorterPosix::PolicyEntry kDefaultIPv4ScopeTable[] = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0x7F},
     104,
     AddressSorterPosix::SCOPE_LINKLOCAL},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xA9, 0xFE},
     112,
     AddressSorterPosix::SCOPE_LINKLOCAL},
    {{}, 0, AddressSorterPosix::SCOPE_GLOBAL},
};

struct DestinationInfo {
  IPEndPoint endpoint;
  AddressSorterPosix::AddressScope scope;
  unsigned precedence;
  unsigned label;
  AddressSorterPosix::SourceAddressInfo src;
  std::unique_ptr<DatagramClientSocket> socket;
  size_t common_prefix_length;
  bool failed = false;
};

// Returns true iff |dst_a| should precede |dst_b| in the address list.
// RFC 3484, section 6.
bool CompareDestinations(const DestinationInfo& dst_a,
                         const DestinationInfo& dst_b) {
  // Rule 1: Avoid unusable destinations.
  // Nothing to do here because unusable destinations are already filtered out.

  // Rule 2: Prefer matching scope.
  bool scope_match1 = (dst_a.src.scope == dst_a.scope);
  bool scope_match2 = (dst_b.src.scope == dst_b.scope);
  if (scope_match1 != scope_match2)
    return scope_match1;

  // Rule 3: Avoid deprecated addresses.
  if (dst_a.src.deprecated != dst_b.src.deprecated) {
    return !dst_a.src.deprecated;
  }

  // Rule 4: Prefer home addresses.
  if (dst_a.src.home != dst_b.src.home) {
    return dst_a.src.home;
  }

  // Rule 5: Prefer matching label.
  bool label_match1 = (dst_a.src.label == dst_a.label);
  bool label_match2 = (dst_b.src.label == dst_b.label);
  if (label_match1 != label_match2)
    return label_match1;

  // Rule 6: Prefer higher precedence.
  if (dst_a.precedence != dst_b.precedence)
    return dst_a.precedence > dst_b.precedence;

  // Rule 7: Prefer native transport.
  if (dst_a.src.native != dst_b.src.native) {
    return dst_a.src.native;
  }

  // Rule 8: Prefer smaller scope.
  if (dst_a.scope != dst_b.scope)
    return dst_a.scope < dst_b.scope;

  // Rule 9: Use longest matching prefix. Only for matching address families.
  if (dst_a.endpoint.address().size() == dst_b.endpoint.address().size()) {
    if (dst_a.common_prefix_length != dst_b.common_prefix_length)
      return dst_a.common_prefix_length > dst_b.common_prefix_length;
  }

  // Rule 10: Leave the order unchanged.
  // stable_sort takes care of that.
  return false;
}

}  // namespace

class AddressSorterPosix::SortContext {
 public:
  SortContext(size_t in_num_endpoints,
              AddressSorter::CallbackType callback,
              const AddressSorterPosix* sorter)
      : num_endpoints_(in_num_endpoints),
        callback_(std::move(callback)),
        sorter_(sorter) {}
  ~SortContext() = default;
  void DidCompleteConnect(IPEndPoint dest, size_t info_index, int rv) {
    ++num_completed_;
    if (rv != OK) {
      VLOG(1) << "Could not connect to " << dest.ToStringWithoutPort()
              << " reason " << rv;
      sort_list_[info_index].failed = true;
    }

    MaybeFinishSort();
  }

  std::vector<DestinationInfo>& sort_list() { return sort_list_; }

 private:
  void MaybeFinishSort() {
    // Sort the list of endpoints only after each Connect call has been made.
    if (num_completed_ != num_endpoints_) {
      return;
    }
    for (auto& info : sort_list_) {
      if (info.failed) {
        continue;
      }

      IPEndPoint src;
      // Filter out unusable destinations.
      int rv = info.socket->GetLocalAddress(&src);
      if (rv != OK) {
        LOG(WARNING) << "Could not get local address for "
                     << info.endpoint.ToStringWithoutPort() << " reason " << rv;
        info.failed = true;
        continue;
      }

      auto iter = sorter_->source_map_.find(src.address());
      if (iter == sorter_->source_map_.end()) {
        //  |src.address| may not be in the map if |source_info_| has not been
        //  updated from the OS yet. It will be updated and HostCache cleared
        //  soon, but we still want to sort, so fill in an empty
        info.src = AddressSorterPosix::SourceAddressInfo();
      } else {
        info.src = iter->second;
      }

      if (info.src.scope == AddressSorterPosix::SCOPE_UNDEFINED) {
        sorter_->FillPolicy(src.address(), &info.src);
      }

      if (info.endpoint.address().size() == src.address().size()) {
        info.common_prefix_length =
            std::min(CommonPrefixLength(info.endpoint.address(), src.address()),
                     info.src.prefix_length);
      }
    }
    std::erase_if(sort_list_, [](auto& element) { return element.failed; });
    std::stable_sort(sort_list_.begin(), sort_list_.end(), CompareDestinations);

    std::vector<IPEndPoint> sorted_result;
    for (const auto& info : sort_list_)
      sorted_result.push_back(info.endpoint);

    CallbackType callback = std::move(callback_);
    sorter_->FinishedSort(this);  // deletes this
    std::move(callback).Run(true, std::move(sorted_result));
  }

  const size_t num_endpoints_;
  size_t num_completed_ = 0;
  std::vector<DestinationInfo> sort_list_;
  AddressSorter::CallbackType callback_;

  raw_ptr<const AddressSorterPosix> sorter_;
};

AddressSorterPosix::AddressSorterPosix(ClientSocketFactory* socket_factory)
    : socket_factory_(socket_factory),
      precedence_table_(LoadPolicy(kDefaultPrecedenceTable,
                                   std::size(kDefaultPrecedenceTable))),
      label_table_(
          LoadPolicy(kDefaultLabelTable, std::size(kDefaultLabelTable))),
      ipv4_scope_table_(LoadPolicy(kDefaultIPv4ScopeTable,
                                   std::size(kDefaultIPv4ScopeTable))) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  OnIPAddressChanged();
}

AddressSorterPosix::~AddressSorterPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void AddressSorterPosix::Sort(const std::vector<IPEndPoint>& endpoints,
                              CallbackType callback) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sort_contexts_.insert(std::make_unique<SortContext>(
      endpoints.size(), std::move(callback), this));
  auto* sort_context = sort_contexts_.rbegin()->get();
  for (const IPEndPoint& endpoint : endpoints) {
    DestinationInfo info;
    info.endpoint = endpoint;
    info.scope = GetScope(ipv4_scope_table_, info.endpoint.address());
    info.precedence =
        GetPolicyValue(precedence_table_, info.endpoint.address());
    info.label = GetPolicyValue(label_table_, info.endpoint.address());

    // Each socket can only be bound once.
    info.socket = socket_factory_->CreateDatagramClientSocket(
        DatagramSocket::DEFAULT_BIND, nullptr /* NetLog */, NetLogSource());
    IPEndPoint dest = info.endpoint;
    // Even though no packets are sent, cannot use port 0 in Connect.
    if (dest.port() == 0) {
      dest = IPEndPoint(dest.address(), /*port=*/80);
    }
    sort_context->sort_list().push_back(std::move(info));
    size_t info_index = sort_context->sort_list().size() - 1;
    // Destroying a SortContext destroys the underlying socket.
    int rv = sort_context->sort_list().back().socket->ConnectAsync(
        dest,
        base::BindOnce(&AddressSorterPosix::SortContext::DidCompleteConnect,
                       base::Unretained(sort_context), dest, info_index));
    if (rv != ERR_IO_PENDING) {
      sort_context->DidCompleteConnect(dest, info_index, rv);
    }
  }
}

void AddressSorterPosix::OnIPAddressChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_map_.clear();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40263501): This always returns nullptr on ChromeOS.
  const AddressMapOwnerLinux* address_map_owner =
      NetworkChangeNotifier::GetAddressMapOwner();
  if (!address_map_owner) {
    return;
  }
  AddressMapOwnerLinux::AddressMap map = address_map_owner->GetAddressMap();
  for (const auto& [address, msg] : map) {
    SourceAddressInfo& info = source_map_[address];
    info.native = false;  // TODO(szym): obtain this via netlink.
    info.deprecated = msg.ifa_flags & IFA_F_DEPRECATED;
    info.home = msg.ifa_flags & IFA_F_HOMEADDRESS;
    info.prefix_length = msg.ifa_prefixlen;
    FillPolicy(address, &info);
  }
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_BSD)
  // It's not clear we will receive notification when deprecated flag changes.
  // Socket for ioctl.
  int ioctl_socket = socket(AF_INET6, SOCK_DGRAM, 0);
  if (ioctl_socket < 0)
    return;

  struct ifaddrs* addrs;
  int rv = getifaddrs(&addrs);
  if (rv < 0) {
    LOG(WARNING) << "getifaddrs failed " << rv;
    close(ioctl_socket);
    return;
  }

  for (struct ifaddrs* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
    IPEndPoint src;
    if (!src.FromSockAddr(ifa->ifa_addr, ifa->ifa_addr->sa_len))
      continue;
    SourceAddressInfo& info = source_map_[src.address()];
    // Note: no known way to fill in |native| and |home|.
    info.native = info.home = info.deprecated = false;
    if (ifa->ifa_addr->sa_family == AF_INET6) {
      struct in6_ifreq ifr = {};
      strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name) - 1);
      DCHECK_LE(ifa->ifa_addr->sa_len, sizeof(ifr.ifr_ifru.ifru_addr));
      memcpy(&ifr.ifr_ifru.ifru_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
      rv = ioctl(ioctl_socket, SIOCGIFAFLAG_IN6, &ifr);
      if (rv >= 0) {
        info.deprecated = ifr.ifr_ifru.ifru_flags & IN6_IFF_DEPRECATED;
      } else {
        LOG(WARNING) << "SIOCGIFAFLAG_IN6 failed " << rv;
      }
    }
    if (ifa->ifa_netmask) {
      IPEndPoint netmask;
      if (netmask.FromSockAddr(ifa->ifa_netmask, ifa->ifa_addr->sa_len)) {
        info.prefix_length = MaskPrefixLength(netmask.address());
      } else {
        LOG(WARNING) << "FromSockAddr failed on netmask";
      }
    }
    FillPolicy(src.address(), &info);
  }
  freeifaddrs(addrs);
  close(ioctl_socket);
#endif
}

void AddressSorterPosix::FillPolicy(const IPAddress& address,
                                    SourceAddressInfo* info) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  info->scope = GetScope(ipv4_scope_table_, address);
  info->label = GetPolicyValue(label_table_, address);
}

void AddressSorterPosix::FinishedSort(SortContext* sort_context) const {
  auto it = sort_contexts_.find(sort_context);
  sort_contexts_.erase(it);
}

// static
std::unique_ptr<AddressSorter> AddressSorter::CreateAddressSorter() {
  return std::make_unique<AddressSorterPosix>(
      ClientSocketFactory::GetDefaultFactory());
}

}  // namespace net

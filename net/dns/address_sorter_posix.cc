// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter_posix.h"

#include <netinet/in.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
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

// Creates sorted PolicyTable from |table|.
AddressSorterPosix::PolicyTable LoadPolicy(
    base::span<const AddressSorterPosix::PolicyEntry> table) {
  AddressSorterPosix::PolicyTable result(table.begin(), table.end());
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
  NOTREACHED();
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
    NOTREACHED();
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
  std::optional<IPAddress> source_address;
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

// Masks the host-specific bits of the given IP address to group addresses
// from the same subnet or prefix for connect caching.
//
// Specifically, this zero-masks the last octet of IPv4 and IPv4-mapped IPv6
// addresses (masking to a /24 subnet), and zero-masks the lower 64 bits of
// IPv6 addresses (masking to a /64 prefix). This aligns with standard internet
// routing boundaries where route reachability and source address selection are
// identical.
IPAddress MaskIPAddress(const IPAddress& address) {
  if (address.IsIPv4()) {
    IPAddressBytes bytes = address.bytes();
    bytes[3] = 0;
    return IPAddress(bytes);
  } else if (address.IsIPv6()) {
    IPAddressBytes bytes = address.bytes();
    if (address.IsIPv4MappedIPv6()) {
      // Only mask the last byte, to match the IPv4 behavior.
      bytes[15] = 0;
      return IPAddress(bytes);
    }
    for (size_t i = 8; i < 16; ++i) {
      bytes[i] = 0;
    }
    return IPAddress(bytes);
  }
  return address;
}

}  // namespace

class AddressSorterPosix::SortContext {
 public:
  SortContext(size_t in_num_endpoints,
              const NetworkAnonymizationKey& anonymization_key,
              AddressSorter::CallbackType callback,
              const AddressSorterPosix* sorter,
              base::TimeTicks start_time)
      : num_endpoints_(in_num_endpoints),
        anonymization_key_(anonymization_key),
        callback_(std::move(callback)),
        sorter_(sorter),
        start_time_(start_time) {}
  SortContext(const SortContext&) = delete;
  SortContext& operator=(const SortContext&) = delete;

  ~SortContext() = default;

  void ConnectWithInfo(DestinationInfo info) {
    // TODO(crbug.com/515502437): Pass in a net log.
    info.socket = sorter_->socket_factory_->CreateDatagramClientSocket(
        DatagramSocket::DEFAULT_BIND, /*net_log=*/nullptr, NetLogSource());
    IPEndPoint dest = info.endpoint;
    // Even though no packets are sent, cannot use port 0 in Connect.
    if (dest.port() == 0) {
      dest = IPEndPoint(dest.address(), /*port=*/80);
    }
    sort_list_.push_back(std::move(info));
    size_t info_index = sort_list_.size() - 1;
    // This use of base::Unretained() is safe because the socket is owned by
    // SortContext, and the callback won't be called after the socket is
    // destroyed.
    int rv = sort_list_.back().socket->ConnectAsync(
        dest,
        base::BindOnce(&AddressSorterPosix::SortContext::RecordConnectResult,
                       base::Unretained(this), dest, info_index));
    ++connect_calls_made_;
    if (rv != ERR_IO_PENDING) {
      RecordConnectResultWithInfo(dest, rv, sort_list_.back());
    }
  }

  void UseCacheResultWithInfo(DestinationInfo info,
                              const ConnectResult& result) {
    if (result.rv == OK) {
      info.source_address = result.source_address;
    } else {
      info.failed = true;
    }
    sort_list_.push_back(std::move(info));
    DestinationInfo& moved_info = sort_list_.back();
    RecordConnectResultWithInfo(moved_info.endpoint, result.rv, moved_info);
  }

 private:
  void RecordConnectResult(IPEndPoint dest, size_t info_index, int rv) {
    DestinationInfo& info = sort_list_[info_index];
    RecordConnectResultWithInfo(dest, rv, info);
  }

  void RecordConnectResultWithInfo(IPEndPoint dest,
                                   int rv,
                                   DestinationInfo& info) {
    ++num_completed_;
    if (rv == OK) {
      if (!info.source_address.has_value()) {
        IPEndPoint src;
        CHECK(info.socket);
        rv = info.socket->GetLocalAddress(&src);
        if (rv == OK) {
          info.source_address = src.address();
        } else {
          DLOG(WARNING) << "Could not get local address for "
                        << info.endpoint.ToStringWithoutPort() << " reason "
                        << rv;
          info.failed = true;
        }
      }
    } else {
      DVLOG(1) << "Could not connect to " << dest.ToStringWithoutPort()
               << " reason " << rv;
      info.failed = true;
    }

    if (sorter_->caching_enabled_) {
      IPAddress masked_address = MaskIPAddress(info.endpoint.address());
      CacheKey cache_key = std::pair(masked_address, anonymization_key_);
      sorter_->connect_cache_.Put(
          cache_key,
          ConnectResult{rv, info.source_address.value_or(IPAddress())});
    }

    MaybeFinishSort();
  }

  void MaybeFinishSort() {
    // Sort the list of endpoints only after each Connect call has been made.
    if (num_completed_ != num_endpoints_) {
      return;
    }

    base::UmaHistogramCounts100("Net.DNS.AddressSorterPosix.ConnectCalls",
                                connect_calls_made_);

    for (auto& info : sort_list_) {
      if (info.failed) {
        continue;
      }

      CHECK(info.source_address.has_value());
      IPAddress src_address = info.source_address.value();

      auto iter = sorter_->source_map_.find(src_address);
      if (iter == sorter_->source_map_.end()) {
        //  |src_address| may not be in the map if |source_info_| has not been
        //  updated from the OS yet. It will be updated and HostCache cleared
        //  soon, but we still want to sort, so fill in an empty
        info.src = AddressSorterPosix::SourceAddressInfo();
      } else {
        info.src = iter->second;
      }

      if (info.src.scope == AddressSorterPosix::SCOPE_UNDEFINED) {
        sorter_->FillPolicy(src_address, &info.src);
      }

      if (info.endpoint.address().size() == src_address.size()) {
        info.common_prefix_length =
            std::min(CommonPrefixLength(info.endpoint.address(), src_address),
                     info.src.prefix_length);
      }
    }
    std::erase_if(sort_list_, [](auto& element) { return element.failed; });
    std::stable_sort(sort_list_.begin(), sort_list_.end(), CompareDestinations);

    std::vector<IPEndPoint> sorted_result = base::ToVector(
        sort_list_, [](const DestinationInfo& info) { return info.endpoint; });

    const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
    base::UmaHistogramCustomMicrosecondsTimes(
        "Net.DNS.AddressSorterPosix.SortDuration", elapsed,
        base::Microseconds(1), base::Seconds(1), 50);

    CallbackType callback = std::move(callback_);
    sorter_->FinishedSort(this);  // deletes this
    std::move(callback).Run(true, std::move(sorted_result));
  }

  const size_t num_endpoints_;
  size_t num_completed_ = 0;
  size_t connect_calls_made_ = 0;
  std::vector<DestinationInfo> sort_list_;
  NetworkAnonymizationKey anonymization_key_;
  AddressSorter::CallbackType callback_;

  raw_ptr<const AddressSorterPosix> sorter_;
  const base::TimeTicks start_time_;
};

// Maximum size for the `connect_cache_`. Cache entries need to survive longer
// than the DNS TTL to be useful. Some sites connect to 200+ different hosts,
// and some hostnames resolve to 16 or more addresses, so this number needs to
// be reasonably large. 4096 corresponds to about 350KB of memory usage.
constexpr size_t kMaxCacheEntries = 4096;

AddressSorterPosix::AddressSorterPosix(ClientSocketFactory* socket_factory)
    : socket_factory_(socket_factory),
      precedence_table_(LoadPolicy(kDefaultPrecedenceTable)),
      label_table_(LoadPolicy(kDefaultLabelTable)),
      ipv4_scope_table_(LoadPolicy(kDefaultIPv4ScopeTable)),
      connect_cache_(kMaxCacheEntries),
      caching_enabled_(
          base::FeatureList::IsEnabled(features::kAddressSorterConnectCache)) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (caching_enabled_) {
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }
  OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL);
}

AddressSorterPosix::~AddressSorterPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_enabled_) {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void AddressSorterPosix::Sort(const std::vector<IPEndPoint>& endpoints,
                              const NetworkAnonymizationKey& anonymization_key,
                              CallbackType callback) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Calling base::TimeTicks::Now() before std::make_unique<>() permits us to
  // include the memory allocation overhead in the time measurement.
  auto [it, inserted] = sort_contexts_.insert(std::make_unique<SortContext>(
      endpoints.size(), anonymization_key, std::move(callback), this,
      base::TimeTicks::Now()));
  CHECK(inserted);
  auto* sort_context = it->get();
  for (const IPEndPoint& endpoint : endpoints) {
    DestinationInfo info;
    info.endpoint = endpoint;
    info.scope = GetScope(ipv4_scope_table_, info.endpoint.address());
    info.precedence =
        GetPolicyValue(precedence_table_, info.endpoint.address());
    info.label = GetPolicyValue(label_table_, info.endpoint.address());

    if (caching_enabled_) {
      IPAddress masked_address = MaskIPAddress(endpoint.address());
      CacheKey cache_key = std::pair(masked_address, anonymization_key);
      auto cache_it = connect_cache_.Get(cache_key);
      if (cache_it != connect_cache_.end()) {
        sort_context->UseCacheResultWithInfo(std::move(info), cache_it->second);
        continue;
      }
    }

    sort_context->ConnectWithInfo(std::move(info));
  }
}

void AddressSorterPosix::OnIPAddressChanged(
    NetworkChangeNotifier::IPAddressChangeType change_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_map_.clear();
  connect_cache_.Clear();
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
      UNSAFE_TODO(
          strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name) - 1));
      DCHECK_LE(ifa->ifa_addr->sa_len, sizeof(ifr.ifr_ifru.ifru_addr));
      UNSAFE_TODO(memcpy(&ifr.ifr_ifru.ifru_addr, ifa->ifa_addr,
                         ifa->ifa_addr->sa_len));
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

void AddressSorterPosix::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  connect_cache_.Clear();
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

bool AddressSorterPosix::IsConnectCacheEmptyForTesting() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return connect_cache_.empty();
}

// static
std::unique_ptr<AddressSorter> AddressSorter::CreateAddressSorter() {
  return std::make_unique<AddressSorterPosix>(
      ClientSocketFactory::GetDefaultFactory());
}

}  // namespace net

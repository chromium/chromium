// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/debug/debugger.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_isolation_key.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_reloader.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_mdns_listener_impl.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_results.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/record_parsed.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if defined(OS_WIN)
#include <Winsock2.h>
#include "net/base/winsock_init.h"
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <net/if.h>
#include "net/base/sys_addrinfo.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_library.h"
#else  // !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif  // defined(OS_ANDROID)
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

namespace net {

namespace {

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Default TTL for successful resolutions with ProcTask.
const unsigned kCacheEntryTTLSeconds = 60;

// Default TTL for unsuccessful resolutions with ProcTask.
const unsigned kNegativeCacheEntryTTLSeconds = 0;

// Minimum TTL for successful resolutions with DnsTask.
const unsigned kMinimumTTLSeconds = kCacheEntryTTLSeconds;

// Time between IPv6 probes, i.e. for how long results of each IPv6 probe are
// cached.
const int kIPv6ProbePeriodMs = 1000;

// Google DNS address used for IPv6 probes.
const uint8_t kIPv6ProbeAddress[] = {0x20, 0x01, 0x48, 0x60, 0x48, 0x60,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x88, 0x88};

enum DnsResolveStatus {
  RESOLVE_STATUS_DNS_SUCCESS = 0,
  RESOLVE_STATUS_PROC_SUCCESS,
  RESOLVE_STATUS_FAIL,
  RESOLVE_STATUS_SUSPECT_NETBIOS,
  RESOLVE_STATUS_MAX
};

// ICANN uses this localhost address to indicate a name collision.
//
// The policy in Chromium is to fail host resolving if it resolves to
// this special address.
//
// Not however that IP literals are exempt from this policy, so it is still
// possible to navigate to http://127.0.53.53/ directly.
//
// For more details: https://www.icann.org/news/announcement-2-2014-08-01-en
const uint8_t kIcanNameCollisionIp[] = {127, 0, 53, 53};

bool ContainsIcannNameCollisionIp(const AddressList& addr_list) {
  for (const auto& endpoint : addr_list) {
    const IPAddress& addr = endpoint.address();
    if (addr.IsIPv4() && IPAddressStartsWith(addr, kIcanNameCollisionIp)) {
      return true;
    }
  }
  return false;
}

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(base::StringPiece hostname) {
  const char kSuffix[] = ".local.";
  const size_t kSuffixLen = sizeof(kSuffix) - 1;
  const size_t kSuffixLenTrimmed = kSuffixLen - 1;
  if (!hostname.empty() && hostname.back() == '.') {
    return hostname.size() > kSuffixLen &&
           !hostname.compare(hostname.size() - kSuffixLen, kSuffixLen, kSuffix);
  }
  return hostname.size() > kSuffixLenTrimmed &&
         !hostname.compare(hostname.size() - kSuffixLenTrimmed,
                           kSuffixLenTrimmed, kSuffix, kSuffixLenTrimmed);
}

bool ConfigureAsyncDnsNoFallbackFieldTrial() {
  const bool kDefault = false;

  // Configure the AsyncDns field trial as follows:
  // groups AsyncDnsNoFallbackA and AsyncDnsNoFallbackB: return true,
  // groups AsyncDnsA and AsyncDnsB: return false,
  // groups SystemDnsA and SystemDnsB: return false,
  // otherwise (trial absent): return default.
  std::string group_name = base::FieldTrialList::FindFullName("AsyncDns");
  if (!group_name.empty()) {
    return base::StartsWith(group_name, "AsyncDnsNoFallback",
                            base::CompareCase::INSENSITIVE_ASCII);
  }
  return kDefault;
}

const base::FeatureParam<base::TaskPriority>::Option prio_modes[] = {
    {base::TaskPriority::USER_VISIBLE, "default"},
    {base::TaskPriority::USER_BLOCKING, "user_blocking"}};
const base::Feature kSystemResolverPriorityExperiment = {
    "SystemResolverPriorityExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<base::TaskPriority> priority_mode{
    &kSystemResolverPriorityExperiment, "mode",
    base::TaskPriority::USER_VISIBLE, &prio_modes};

//-----------------------------------------------------------------------------

// Returns true if |addresses| contains only IPv4 loopback addresses.
bool IsAllIPv4Loopback(const AddressList& addresses) {
  for (unsigned i = 0; i < addresses.size(); ++i) {
    const IPAddress& address = addresses[i].address();
    switch (addresses[i].GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        if (address.bytes()[0] != 127)
          return false;
        break;
      case ADDRESS_FAMILY_IPV6:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }
  return true;
}

// Returns true if it can determine that only loopback addresses are configured.
// i.e. if only 127.0.0.1 and ::1 are routable.
// Also returns false if it cannot determine this.
bool HaveOnlyLoopbackAddresses() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
#if defined(OS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_ANDROID)
  return android::HaveOnlyLoopbackAddresses();
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  struct ifaddrs* interface_addr = NULL;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVPLOG(1) << "getifaddrs() failed";
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    const struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;
    if (addr->sa_family == AF_INET6) {
      // Safe cast since this is AF_INET6.
      const struct sockaddr_in6* addr_in6 =
          reinterpret_cast<const struct sockaddr_in6*>(addr);
      const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
        continue;
    }
    if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET)
      continue;

    result = false;
    break;
  }
  freeifaddrs(interface_addr);
  return result;
#endif  // defined(various platforms)
}

// Creates NetLog parameters when the resolve failed.
base::Value NetLogProcTaskFailedParams(uint32_t attempt_number,
                                       int net_error,
                                       int os_error) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (attempt_number)
    dict.SetIntKey("attempt_number", attempt_number);

  dict.SetIntKey("net_error", net_error);

  if (os_error) {
    dict.SetIntKey("os_error", os_error);
#if defined(OS_WIN)
    // Map the error code to a human-readable string.
    LPWSTR error_string = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  nullptr,  // Use the internal message table.
                  os_error,
                  0,  // Use default language.
                  (LPWSTR)&error_string,
                  0,         // Buffer size.
                  nullptr);  // Arguments (unused).
    dict.SetStringKey("os_error_string", base::WideToUTF8(error_string));
    LocalFree(error_string);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    dict.SetStringKey("os_error_string", gai_strerror(os_error));
#endif
  }

  return dict;
}

base::Value NetLogDnsTaskCreationParams(
    bool secure,
    const base::circular_deque<DnsQueryType>& transactions_needed) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey("secure", secure);

  base::Value transactions_needed_value(base::Value::Type::LIST);
  for (DnsQueryType type : transactions_needed) {
    base::Value transaction_dict(base::Value::Type::DICTIONARY);
    transaction_dict.SetIntKey("dns_query_type", static_cast<int>(type));
    transactions_needed_value.Append(std::move(transaction_dict));
  }
  dict.SetKey("transactions_needed", std::move(transactions_needed_value));

  return dict;
}

// Creates NetLog parameters when the DnsTask failed.
base::Value NetLogDnsTaskFailedParams(
    int net_error,
    absl::optional<DnsQueryType> failed_transaction_type,
    absl::optional<base::TimeDelta> ttl,
    const HostCache::Entry* saved_results) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (failed_transaction_type) {
    dict.SetIntKey("dns_query_type",
                   static_cast<int>(failed_transaction_type.value()));
  }
  if (ttl)
    dict.SetIntKey("error_ttl_sec", ttl.value().InSeconds());
  dict.SetIntKey("net_error", net_error);
  if (saved_results)
    dict.SetKey("saved_results", saved_results->NetLogParams());
  return dict;
}

base::Value NetLogDnsTaskExtractionFailureParams(
    DnsResponseResultExtractor::ExtractionError extraction_error,
    DnsQueryType dns_query_type,
    const HostCache::Entry& results) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("extraction_error", static_cast<int>(extraction_error));
  dict.SetIntKey("dns_query_type", static_cast<int>(dns_query_type));
  dict.SetKey("results", results.NetLogParams());
  return dict;
}

base::Value NetLogDnsTaskTimeoutParams(
    const base::flat_map<std::unique_ptr<DnsTransaction>,
                         DnsQueryType,
                         base::UniquePtrComparator>& started_transactions,
    const base::circular_deque<DnsQueryType>& queued_transactions) {
  base::Value dict(base::Value::Type::DICTIONARY);

  if (!started_transactions.empty()) {
    base::Value list(base::Value::Type::LIST);
    for (const auto& transaction : started_transactions) {
      base::Value transaction_dict(base::Value::Type::DICTIONARY);
      transaction_dict.SetIntKey("dns_query_type",
                                 static_cast<int>(transaction.second));
      list.Append(std::move(transaction_dict));
    }
    dict.SetKey("started_transactions", std::move(list));
  }

  if (!queued_transactions.empty()) {
    base::Value list(base::Value::Type::LIST);
    for (DnsQueryType type : queued_transactions) {
      base::Value transaction_dict(base::Value::Type::DICTIONARY);
      transaction_dict.SetIntKey("dns_query_type", static_cast<int>(type));
      list.Append(std::move(transaction_dict));
    }
    dict.SetKey("queued_transactions", std::move(list));
  }

  return dict;
}

// Creates NetLog parameters for HOST_RESOLVER_MANAGER_JOB_ATTACH/DETACH events.
base::Value NetLogJobAttachParams(const NetLogSource& source,
                                  RequestPriority priority) {
  base::Value dict(base::Value::Type::DICTIONARY);
  source.AddToEventParameters(&dict);
  dict.SetStringKey("priority", RequestPriorityToString(priority));
  return dict;
}

base::Value NetLogIPv6AvailableParams(bool ipv6_available, bool cached) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey("ipv6_available", ipv6_available);
  dict.SetBoolKey("cached", cached);
  return dict;
}

// The logging routines are defined here because some requests are resolved
// without a Request object.

//-----------------------------------------------------------------------------

// Maximum of 64 concurrent resolver threads (excluding retries).
// Between 2010 and 2020, the limit was set to 6 because of a report of a broken
// home router that would fail in the presence of more simultaneous queries.
// In 2020, we conducted an experiment to see if this kind of router was still
// present on the Internet, and found no evidence of any remaining issues, so
// we increased the limit to 64 at that time.
const size_t kDefaultMaxProcTasks = 64u;

PrioritizedDispatcher::Limits GetDispatcherLimits(
    const HostResolver::ManagerOptions& options) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES,
                                       options.max_concurrent_resolves);

  // If not using default, do not use the field trial.
  if (limits.total_jobs != HostResolver::ManagerOptions::kDefaultParallelism)
    return limits;

  // Default, without trial is no reserved slots.
  limits.total_jobs = kDefaultMaxProcTasks;

  // Parallelism is determined by the field trial.
  std::string group =
      base::FieldTrialList::FindFullName("HostResolverDispatch");

  if (group.empty())
    return limits;

  // The format of the group name is a list of non-negative integers separated
  // by ':'. Each of the elements in the list corresponds to an element in
  // |reserved_slots|, except the last one which is the |total_jobs|.
  std::vector<base::StringPiece> group_parts = base::SplitStringPiece(
      group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (group_parts.size() != NUM_PRIORITIES + 1) {
    NOTREACHED();
    return limits;
  }

  std::vector<size_t> parsed(group_parts.size());
  size_t total_reserved_slots = 0;

  for (size_t i = 0; i < group_parts.size(); ++i) {
    if (!base::StringToSizeT(group_parts[i], &parsed[i])) {
      NOTREACHED();
      return limits;
    }
  }

  size_t total_jobs = parsed.back();
  parsed.pop_back();
  for (size_t i = 0; i < parsed.size(); ++i) {
    total_reserved_slots += parsed[i];
  }

  // There must be some unreserved slots available for the all priorities.
  if (total_reserved_slots > total_jobs ||
      (total_reserved_slots == total_jobs && parsed[MINIMUM_PRIORITY] == 0)) {
    NOTREACHED();
    return limits;
  }

  limits.total_jobs = total_jobs;
  limits.reserved_slots = parsed;
  return limits;
}

// Keeps track of the highest priority.
class PriorityTracker {
 public:
  explicit PriorityTracker(RequestPriority initial_priority)
      : highest_priority_(initial_priority), total_count_(0) {
    memset(counts_, 0, sizeof(counts_));
  }

  RequestPriority highest_priority() const { return highest_priority_; }

  size_t total_count() const { return total_count_; }

  void Add(RequestPriority req_priority) {
    ++total_count_;
    ++counts_[req_priority];
    if (highest_priority_ < req_priority)
      highest_priority_ = req_priority;
  }

  void Remove(RequestPriority req_priority) {
    DCHECK_GT(total_count_, 0u);
    DCHECK_GT(counts_[req_priority], 0u);
    --total_count_;
    --counts_[req_priority];
    size_t i;
    for (i = highest_priority_; i > MINIMUM_PRIORITY && !counts_[i]; --i) {
    }
    highest_priority_ = static_cast<RequestPriority>(i);

    // In absence of requests, default to MINIMUM_PRIORITY.
    if (total_count_ == 0)
      DCHECK_EQ(MINIMUM_PRIORITY, highest_priority_);
  }

 private:
  RequestPriority highest_priority_;
  size_t total_count_;
  size_t counts_[NUM_PRIORITIES];
};

base::Value NetLogResults(const HostCache::Entry& results) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("results", results.NetLogParams());
  return dict;
}

base::Value ToLogStringValue(
    const absl::variant<url::SchemeHostPort, HostPortPair>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host))
    return base::Value(absl::get<url::SchemeHostPort>(host).Serialize());

  return base::Value(absl::get<HostPortPair>(host).ToString());
}

base::Value ToLogStringValue(
    const absl::variant<url::SchemeHostPort, std::string>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    return base::Value(absl::get<url::SchemeHostPort>(host).Serialize());
  }

  return base::Value(absl::get<std::string>(host));
}

// Returns empty string if `host` has no known scheme.
base::StringPiece GetScheme(
    const absl::variant<url::SchemeHostPort, std::string>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host))
    return absl::get<url::SchemeHostPort>(host).scheme();

  return base::StringPiece();
}

base::StringPiece GetHostname(
    const absl::variant<url::SchemeHostPort, HostPortPair>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    base::StringPiece hostname = absl::get<url::SchemeHostPort>(host).host();
    if (hostname.size() >= 2 && hostname.front() == '[' &&
        hostname.back() == ']') {
      hostname = hostname.substr(1, hostname.size() - 2);
    }
    return hostname;
  }

  return absl::get<HostPortPair>(host).host();
}

base::StringPiece GetHostname(
    const absl::variant<url::SchemeHostPort, std::string>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    base::StringPiece hostname = absl::get<url::SchemeHostPort>(host).host();
    if (hostname.size() >= 2 && hostname.front() == '[' &&
        hostname.back() == ']') {
      hostname = hostname.substr(1, hostname.size() - 2);
    }
    return hostname;
  }

  return absl::get<std::string>(host);
}

uint16_t GetPort(const absl::variant<url::SchemeHostPort, HostPortPair>& host) {
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    return absl::get<url::SchemeHostPort>(host).port();
  }

  return absl::get<HostPortPair>(host).port();
}

// Only use scheme/port in JobKey if `features::kUseDnsHttpsSvcb` is enabled
// (or the query is explicitly for HTTPS). Otherwise DNS will not give different
// results for the same hostname.
absl::variant<url::SchemeHostPort, std::string> CreateHostForJobKey(
    const absl::variant<url::SchemeHostPort, HostPortPair>& input,
    DnsQueryType query_type) {
  if ((base::FeatureList::IsEnabled(features::kUseDnsHttpsSvcb) ||
       query_type == DnsQueryType::HTTPS) &&
      absl::holds_alternative<url::SchemeHostPort>(input)) {
    return absl::get<url::SchemeHostPort>(input);
  }

  return std::string(GetHostname(input));
}

DnsResponse CreateFakeEmptyResponse(base::StringPiece hostname,
                                    DnsQueryType query_type) {
  std::string qname;
  CHECK(DNSDomainFromDot(hostname, &qname));
  return DnsResponse::CreateEmptyNoDataResponse(
      /*id=*/0u, /*is_authoritative=*/true, qname,
      DnsQueryTypeToQtype(query_type));
}

}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(base::StringPiece host, AddressList* address_list) {
  address_list->clear();

  if (!IsLocalHostname(host))
    return false;

  address_list->push_back(IPEndPoint(IPAddress::IPv6Localhost(), 0));
  address_list->push_back(IPEndPoint(IPAddress::IPv4Localhost(), 0));

  return true;
}

// Holds the callback and request parameters for an outstanding request.
//
// The RequestImpl is owned by the end user of host resolution. Deletion prior
// to the request having completed means the request was cancelled by the
// caller.
//
// Both the RequestImpl and its associated Job hold non-owning pointers to each
// other. Care must be taken to clear the corresponding pointer when
// cancellation is initiated by the Job (OnJobCancelled) vs by the end user
// (~RequestImpl).
class HostResolverManager::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public base::LinkNode<HostResolverManager::RequestImpl> {
 public:
  RequestImpl(NetLogWithSource source_net_log,
              absl::variant<url::SchemeHostPort, HostPortPair> request_host,
              NetworkIsolationKey network_isolation_key,
              absl::optional<ResolveHostParameters> optional_parameters,
              base::WeakPtr<ResolveContext> resolve_context,
              HostCache* host_cache,
              base::WeakPtr<HostResolverManager> resolver,
              const base::TickClock* tick_clock)
      : source_net_log_(std::move(source_net_log)),
        request_host_(std::move(request_host)),
        network_isolation_key_(
            base::FeatureList::IsEnabled(
                net::features::kSplitHostCacheByNetworkIsolationKey)
                ? std::move(network_isolation_key)
                : NetworkIsolationKey()),
        parameters_(optional_parameters ? std::move(optional_parameters).value()
                                        : ResolveHostParameters()),
        resolve_context_(std::move(resolve_context)),
        host_cache_(host_cache),
        host_resolver_flags_(
            HostResolver::ParametersToHostResolverFlags(parameters_)),
        priority_(parameters_.initial_priority),
        job_(nullptr),
        resolver_(std::move(resolver)),
        complete_(false),
        tick_clock_(tick_clock) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override;

  int Start(CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK(!job_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    if (!resolve_context_) {
      complete_ = true;
      resolver_ = nullptr;
      set_error_info(ERR_CONTEXT_SHUT_DOWN, false);
      return ERR_NAME_NOT_RESOLVED;
    }

    LogStartRequest();
    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      DCHECK(job_);
      callback_ = std::move(callback);
    } else {
      DCHECK(!job_);
      complete_ = true;
      LogFinishRequest(rv, false /* async_completion */);
    }
    resolver_ = nullptr;

    return rv;
  }

  const absl::optional<AddressList>& GetAddressResults() const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<AddressList>> nullopt_result;
    return results_ ? results_.value().addresses() : *nullopt_result;
  }

  absl::optional<std::vector<HostResolverEndpointResult>> GetEndpointResults()
      const override {
    DCHECK(complete_);

    if (!results_.has_value() || !results_.value().addresses().has_value())
      return absl::nullopt;

    // TODO(crbug.com/1264933): Use HostResolverEndpointResult internally
    // instead of converting on output.
    return HostResolver::AddressListToEndpointResults(
        results_.value().addresses().value());
  }

  const absl::optional<std::vector<std::string>>& GetTextResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<std::vector<std::string>>>
        nullopt_result;
    return results_ ? results_.value().text_records() : *nullopt_result;
  }

  const absl::optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<std::vector<HostPortPair>>>
        nullopt_result;
    return results_ ? results_.value().hostnames() : *nullopt_result;
  }

  const absl::optional<std::vector<std::string>>& GetDnsAliasResults()
      const override {
    DCHECK(complete_);
    return sanitized_dns_alias_results_;
  }

  const absl::optional<std::vector<bool>>& GetExperimentalResultsForTesting()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<absl::optional<std::vector<bool>>>
        nullopt_result;
    return results_ ? results_.value().experimental_results() : *nullopt_result;
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    DCHECK(complete_);
    return error_info_;
  }

  const absl::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    DCHECK(complete_);
    return stale_info_;
  }

  void ChangeRequestPriority(RequestPriority priority) override;

  void set_results(HostCache::Entry results) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!results_);
    DCHECK(!parameters_.is_speculative);

    results_ = std::move(results);
  }

  void set_error_info(int error, bool is_secure_network_error) {
    error_info_ = ResolveErrorInfo(error, is_secure_network_error);
  }

  void set_stale_info(HostCache::EntryStaleness stale_info) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!stale_info_);
    DCHECK(!parameters_.is_speculative);

    stale_info_ = std::move(stale_info);
  }

  void AssignJob(Job* job) {
    DCHECK(job);
    DCHECK(!job_);

    job_ = job;
  }

  // Unassigns the Job without calling completion callback.
  void OnJobCancelled(Job* job) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;
    DCHECK(!complete_);
    DCHECK(callback_);
    callback_.Reset();

    // No results should be set.
    DCHECK(!results_);

    LogCancelRequest();
  }

  // Cleans up Job assignment, marks request completed, and calls the completion
  // callback. |is_secure_network_error| indicates whether |error| came from a
  // secure DNS lookup.
  void OnJobCompleted(Job* job, int error, bool is_secure_network_error) {
    set_error_info(error, is_secure_network_error);

    DCHECK_EQ(job_, job);
    job_ = nullptr;

    DCHECK(!complete_);
    complete_ = true;

    LogFinishRequest(error, true /* async_completion */);

    DCHECK(callback_);
    std::move(callback_).Run(HostResolver::SquashErrorCode(error));
  }

  Job* job() const { return job_; }

  // NetLog for the source, passed in HostResolver::Resolve.
  const NetLogWithSource& source_net_log() { return source_net_log_; }

  const absl::variant<url::SchemeHostPort, HostPortPair>& request_host() const {
    return request_host_;
  }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  ResolveContext* resolve_context() const { return resolve_context_.get(); }

  HostCache* host_cache() const { return host_cache_; }

  HostResolverFlags host_resolver_flags() const { return host_resolver_flags_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  bool complete() const { return complete_; }

  void SanitizeDnsAliasResults() {
    // If there are no address results, if there are no aliases, or if there
    // are already sanitized alias results, there is nothing to do.
    if (!results_ || !results_.value().addresses() ||
        results_.value().addresses()->dns_aliases().empty() ||
        sanitized_dns_alias_results_) {
      return;
    }

    sanitized_dns_alias_results_ = dns_alias_utility::SanitizeDnsAliases(
        results_.value().addresses()->dns_aliases());
  }

 private:
  // Logging and metrics for when a request has just been started.
  void LogStartRequest() {
    DCHECK(request_time_.is_null());
    request_time_ = tick_clock_->NowTicks();

    source_net_log_.BeginEvent(
        NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST, [this] {
          base::Value dict(base::Value::Type::DICTIONARY);
          dict.SetKey("host", ToLogStringValue(request_host_));
          dict.SetIntKey("dns_query_type",
                         static_cast<int>(parameters_.dns_query_type));
          dict.SetBoolKey("allow_cached_response",
                          parameters_.cache_usage !=
                              ResolveHostParameters::CacheUsage::DISALLOWED);
          dict.SetBoolKey("is_speculative", parameters_.is_speculative);
          dict.SetStringKey("network_isolation_key",
                            network_isolation_key_.ToDebugString());
          dict.SetIntKey("secure_dns_policy",
                         static_cast<int>(parameters_.secure_dns_policy));
          return dict;
        });
  }

  // Logging and metrics for when a request has just completed (before its
  // callback is run).
  void LogFinishRequest(int net_error, bool async_completion) {
    source_net_log_.EndEventWithNetErrorCode(
        NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST, net_error);

    if (!parameters_.is_speculative) {
      DCHECK(!request_time_.is_null());
      base::TimeDelta duration = tick_clock_->NowTicks() - request_time_;

      UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.Request.TotalTime", duration);
      if (async_completion)
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.Request.TotalTimeAsync", duration);
    }
  }

  // Logs when a request has been cancelled.
  void LogCancelRequest() {
    source_net_log_.AddEvent(NetLogEventType::CANCELLED);
    source_net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST);
  }

  const NetLogWithSource source_net_log_;

  const absl::variant<url::SchemeHostPort, HostPortPair> request_host_;
  const NetworkIsolationKey network_isolation_key_;
  ResolveHostParameters parameters_;
  base::WeakPtr<ResolveContext> resolve_context_;
  const raw_ptr<HostCache> host_cache_;
  const HostResolverFlags host_resolver_flags_;

  RequestPriority priority_;

  // The resolve job that this request is dependent on.
  raw_ptr<Job> job_;
  base::WeakPtr<HostResolverManager> resolver_;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_;
  absl::optional<HostCache::Entry> results_;
  absl::optional<HostCache::EntryStaleness> stale_info_;
  absl::optional<std::vector<std::string>> sanitized_dns_alias_results_;
  ResolveErrorInfo error_info_;

  const raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks request_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class HostResolverManager::ProbeRequestImpl
    : public HostResolver::ProbeRequest,
      public ResolveContext::DohStatusObserver {
 public:
  ProbeRequestImpl(base::WeakPtr<ResolveContext> context,
                   base::WeakPtr<HostResolverManager> resolver)
      : context_(std::move(context)), resolver_(std::move(resolver)) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override {
    // Ensure that observers are deregistered to avoid wasting memory.
    if (context_)
      context_->UnregisterDohStatusObserver(this);
  }

  int Start() override {
    DCHECK(resolver_);
    DCHECK(!runner_);

    if (!context_)
      return ERR_CONTEXT_SHUT_DOWN;

    context_->RegisterDohStatusObserver(this);

    StartRunner(false /* network_change */);
    return ERR_IO_PENDING;
  }

  // ResolveContext::DohStatusObserver
  void OnSessionChanged() override { CancelRunner(); }

  void OnDohServerUnavailable(bool network_change) override {
    // Start the runner asynchronously, as this may trigger reentrant calls into
    // HostResolverManager, which are not allowed during notification handling.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProbeRequestImpl::StartRunner,
                       weak_ptr_factory_.GetWeakPtr(), network_change));
  }

 private:
  void StartRunner(bool network_change) {
    DCHECK(resolver_);
    DCHECK(!resolver_->invalidation_in_progress_);

    if (!context_)
      return;  // Reachable if the context ends before a posted task runs.

    if (!runner_)
      runner_ = resolver_->CreateDohProbeRunner(context_.get());
    if (runner_)
      runner_->Start(network_change);
  }

  void CancelRunner() {
    runner_.reset();

    // Cancel any asynchronous StartRunner() calls.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  base::WeakPtr<ResolveContext> context_;

  std::unique_ptr<DnsProbeRunner> runner_;
  base::WeakPtr<HostResolverManager> resolver_;

  base::WeakPtrFactory<ProbeRequestImpl> weak_ptr_factory_{this};
};

//------------------------------------------------------------------------------

// Calls HostResolverProc in ThreadPool. Performs retries if necessary.
//
// In non-test code, the HostResolverProc is always SystemHostResolverProc,
// which calls a platform API that implements host resolution.
//
// Whenever we try to resolve the host, we post a delayed task to check if host
// resolution (OnLookupComplete) is completed or not. If the original attempt
// hasn't completed, then we start another attempt for host resolution. We take
// the results from the first attempt that finishes and ignore the results from
// all other attempts.
//
// TODO(szym): Move to separate source file for testing and mocking.
//
class HostResolverManager::ProcTask {
 public:
  typedef base::OnceCallback<void(int net_error, const AddressList& addr_list)>
      Callback;

  ProcTask(std::string hostname,
           AddressFamily address_family,
           HostResolverFlags flags,
           const ProcTaskParams& params,
           Callback callback,
           scoped_refptr<base::TaskRunner> proc_task_runner,
           const NetLogWithSource& job_net_log,
           const base::TickClock* tick_clock)
      : hostname_(std::move(hostname)),
        address_family_(address_family),
        flags_(flags),
        params_(params),
        callback_(std::move(callback)),
        network_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        proc_task_runner_(std::move(proc_task_runner)),
        attempt_number_(0),
        net_log_(job_net_log),
        tick_clock_(tick_clock) {
    DCHECK(callback_);
    if (!params_.resolver_proc.get())
      params_.resolver_proc = HostResolverProc::GetDefault();
    // If default is unset, use the system proc.
    if (!params_.resolver_proc.get())
      params_.resolver_proc = new SystemHostResolverProc();
  }

  ProcTask(const ProcTask&) = delete;
  ProcTask& operator=(const ProcTask&) = delete;

  // Cancels this ProcTask. Any outstanding resolve attempts running on worker
  // thread will continue running, but they will post back to the network thread
  // before checking their WeakPtrs to find that this task is cancelled.
  ~ProcTask() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());

    // If this is cancellation, log the EndEvent (otherwise this was logged in
    // OnLookupComplete()).
    if (!was_completed())
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_PROC_TASK);
  }

  void Start() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());
    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_MANAGER_PROC_TASK);
    StartLookupAttempt();
  }

  bool was_completed() const {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    return callback_.is_null();
  }

 private:
  using AttemptCompletionCallback = base::OnceCallback<
      void(const AddressList& results, int error, const int os_error)>;

  void StartLookupAttempt() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());
    base::TimeTicks start_time = tick_clock_->NowTicks();
    ++attempt_number_;
    // Dispatch the lookup attempt to a worker thread.
    AttemptCompletionCallback completion_callback = base::BindOnce(
        &ProcTask::OnLookupAttemptComplete, weak_ptr_factory_.GetWeakPtr(),
        start_time, attempt_number_, tick_clock_);
    proc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ProcTask::DoLookup, hostname_, address_family_, flags_,
                       params_.resolver_proc, network_task_runner_,
                       std::move(completion_callback)));

    net_log_.AddEventWithIntParams(
        NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_STARTED,
        "attempt_number", attempt_number_);

    // If the results aren't received within a given time, RetryIfNotComplete
    // will start a new attempt if none of the outstanding attempts have
    // completed yet.
    // Use a WeakPtr to avoid keeping the ProcTask alive after completion or
    // cancellation.
    if (attempt_number_ <= params_.max_retry_attempts) {
      network_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ProcTask::StartLookupAttempt,
                         weak_ptr_factory_.GetWeakPtr()),
          params_.unresponsive_delay *
              std::pow(params_.retry_factor, attempt_number_ - 1));
    }
  }

  // WARNING: This code runs in ThreadPool with CONTINUE_ON_SHUTDOWN. The
  // shutdown code cannot wait for it to finish, so this code must be very
  // careful about using other objects (like MessageLoops, Singletons, etc).
  // During shutdown these objects may no longer exist.
  static void DoLookup(
      std::string hostname,
      AddressFamily address_family,
      HostResolverFlags flags,
      scoped_refptr<HostResolverProc> resolver_proc,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      AttemptCompletionCallback completion_callback) {
    AddressList results;
    int os_error = 0;
    int error = resolver_proc->Resolve(hostname, address_family, flags,
                                       &results, &os_error);

    network_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), results,
                                  error, os_error));
  }

  // Callback for when DoLookup() completes (runs on task runner thread). Now
  // that we're back in the network thread, checks that |proc_task| is still
  // valid, and if so, passes back to the object.
  static void OnLookupAttemptComplete(base::WeakPtr<ProcTask> proc_task,
                                      const base::TimeTicks& start_time,
                                      const uint32_t attempt_number,
                                      const base::TickClock* tick_clock,
                                      const AddressList& results,
                                      int error,
                                      const int os_error) {
    TRACE_EVENT0(NetTracingCategory(), "ProcTask::OnLookupComplete");

    // If results are empty, we should return an error.
    bool empty_list_on_ok = (error == OK && results.empty());
    if (empty_list_on_ok)
      error = ERR_NAME_NOT_RESOLVED;

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker threads.
    // So do it here on the IO thread instead.
    if (error != OK && NetworkChangeNotifier::IsOffline())
      error = ERR_INTERNET_DISCONNECTED;

    if (!proc_task)
      return;

    proc_task->OnLookupComplete(results, start_time, attempt_number, error,
                                os_error);
  }

  void OnLookupComplete(const AddressList& results,
                        const base::TimeTicks& start_time,
                        const uint32_t attempt_number,
                        int error,
                        const int os_error) {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());

    // Invalidate WeakPtrs to cancel handling of all outstanding lookup attempts
    // and retries.
    weak_ptr_factory_.InvalidateWeakPtrs();

    if (error != OK) {
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_PROC_TASK, [&] {
        return NetLogProcTaskFailedParams(0, error, os_error);
      });
      net_log_.AddEvent(
          NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_FINISHED, [&] {
            return NetLogProcTaskFailedParams(attempt_number, error, os_error);
          });
    } else {
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_PROC_TASK,
                        [&] { return results.NetLogParams(); });
      net_log_.AddEventWithIntParams(
          NetLogEventType::HOST_RESOLVER_MANAGER_ATTEMPT_FINISHED,
          "attempt_number", attempt_number);
    }

    std::move(callback_).Run(error, results);
  }

  const std::string hostname_;
  const AddressFamily address_family_;
  const HostResolverFlags flags_;

  // Holds an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  ProcTaskParams params_;

  // The listener to the results of this ProcTask.
  Callback callback_;

  // Used to post events onto the network thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  // Used to post blocking HostResolverProc tasks.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  // Keeps track of the number of attempts we have made so far to resolve the
  // host. Whenever we start an attempt to resolve the host, we increase this
  // number.
  uint32_t attempt_number_;

  NetLogWithSource net_log_;

  raw_ptr<const base::TickClock> tick_clock_;

  // Used to loop back from the blocking lookup attempt tasks as well as from
  // delayed retry tasks. Invalidate WeakPtrs on completion and cancellation to
  // cancel handling of such posted tasks.
  base::WeakPtrFactory<ProcTask> weak_ptr_factory_{this};
};

//-----------------------------------------------------------------------------

// Resolves the hostname using DnsTransaction, which is a full implementation of
// a DNS stub resolver. One DnsTransaction is created for each resolution
// needed, which for AF_UNSPEC resolutions includes both A and AAAA. The
// transactions are scheduled separately and started separately.
//
// TODO(szym): This could be moved to separate source file as well.
class HostResolverManager::DnsTask : public base::SupportsWeakPtr<DnsTask> {
 public:
  class Delegate {
   public:
    virtual void OnDnsTaskComplete(base::TimeTicks start_time,
                                   HostCache::Entry results,
                                   bool secure) = 0;

    // Called when a job succeeds and there are more transactions needed.  If
    // the current completed transaction fails, this is not called.  Also not
    // called when the DnsTask only needs to run one transaction.
    virtual void OnIntermediateTransactionComplete() = 0;

    virtual RequestPriority priority() const = 0;

    virtual void AddTransactionTimeQueued(base::TimeDelta time_queued) = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  DnsTask(DnsClient* client,
          absl::variant<url::SchemeHostPort, std::string> host,
          DnsQueryType query_type,
          ResolveContext* resolve_context,
          bool secure,
          SecureDnsMode secure_dns_mode,
          Delegate* delegate,
          const NetLogWithSource& job_net_log,
          const base::TickClock* tick_clock,
          bool fallback_available)
      : client_(client),
        host_(std::move(host)),
        resolve_context_(resolve_context->AsSafeRef()),
        secure_(secure),
        secure_dns_mode_(secure_dns_mode),
        delegate_(delegate),
        net_log_(job_net_log),
        num_completed_transactions_(0),
        tick_clock_(tick_clock),
        task_start_time_(tick_clock_->NowTicks()),
        fallback_available_(fallback_available) {
    DCHECK(client_);
    if (secure_)
      DCHECK(client_->CanUseSecureDnsTransactions());
    else
      DCHECK(client_->CanUseInsecureDnsTransactions());

    if (query_type != DnsQueryType::UNSPECIFIED) {
      transactions_needed_.push_back(query_type);
    } else {
      transactions_needed_.push_back(DnsQueryType::A);
      transactions_needed_.push_back(DnsQueryType::AAAA);
      MaybeQueueHttpsTransaction();
    }
    num_needed_transactions_ = transactions_needed_.size();

    DCHECK(delegate_);
  }

  DnsTask(const DnsTask&) = delete;
  DnsTask& operator=(const DnsTask&) = delete;

  // The number of transactions required for the specified query type. Does not
  // change as transactions are completed.
  int num_needed_transactions() const { return num_needed_transactions_; }

  bool needs_another_transaction() const {
    return !transactions_needed_.empty();
  }

  bool secure() const { return secure_; }

  void StartNextTransaction() {
    DCHECK(needs_another_transaction());

    if (num_needed_transactions_ ==
        static_cast<int>(transactions_needed_.size())) {
      net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK, [&] {
        return NetLogDnsTaskCreationParams(secure(), transactions_needed_);
      });
    }

    DnsQueryType type = transactions_needed_.front();
    transactions_needed_.pop_front();

    DCHECK(IsAddressType(type) || secure_ ||
           client_->CanQueryAdditionalTypesViaInsecureDns());

    // Record how long this transaction has been waiting to be created.
    base::TimeDelta time_queued = tick_clock_->NowTicks() - task_start_time_;
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.PerTransaction",
                                 time_queued);
    delegate_->AddTransactionTimeQueued(time_queued);

    std::unique_ptr<DnsTransaction> transaction = CreateTransaction(type);
    transaction->Start();
    transactions_started_.emplace(std::move(transaction), type);
  }

 private:
  void MaybeQueueHttpsTransaction() {
    // `features::kUseDnsHttpsSvcb` has precedence, so if enabled, ignore any
    // other related features.
    if (base::FeatureList::IsEnabled(features::kUseDnsHttpsSvcb)) {
      base::StringPiece scheme = GetScheme(host_);
      if (scheme != url::kHttpScheme && scheme != url::kHttpsScheme &&
          scheme != url::kWsScheme && scheme != url::kWssScheme) {
        return;
      }

      if (!secure_ && (!features::kUseDnsHttpsSvcbEnableInsecure.Get() ||
                       !client_->CanQueryAdditionalTypesViaInsecureDns()))
        return;

      httpssvc_metrics_.emplace(/*expect_intact=*/false);
      transactions_needed_.push_back(DnsQueryType::HTTPS);
      return;
    }

    if (base::FeatureList::IsEnabled(features::kDnsHttpssvc)) {
      const bool is_httpssvc_experiment_domain =
          httpssvc_domain_cache_.IsExperimental(GetHostname(host_));
      const bool is_httpssvc_control_domain =
          httpssvc_domain_cache_.IsControl(GetHostname(host_));
      const bool can_query_via_insecure =
          !secure_ && features::kDnsHttpssvcEnableQueryOverInsecure.Get() &&
          client_->CanQueryAdditionalTypesViaInsecureDns();
      if (base::FeatureList::IsEnabled(features::kDnsHttpssvc) &&
          (secure_ || can_query_via_insecure) &&
          (is_httpssvc_experiment_domain || is_httpssvc_control_domain)) {
        httpssvc_metrics_.emplace(
            is_httpssvc_experiment_domain /* expect_intact */);

        if (features::kDnsHttpssvcUseIntegrity.Get())
          transactions_needed_.push_back(DnsQueryType::INTEGRITY);
        if (features::kDnsHttpssvcUseHttpssvc.Get())
          transactions_needed_.push_back(DnsQueryType::HTTPS_EXPERIMENTAL);
      }
    }
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(
      DnsQueryType dns_query_type) {
    DCHECK_NE(DnsQueryType::UNSPECIFIED, dns_query_type);

    std::string transaction_hostname(GetHostname(host_));

    // For HTTPS, prepend "_<port>._https." for any non-default port.
    if (dns_query_type == DnsQueryType::HTTPS &&
        absl::holds_alternative<url::SchemeHostPort>(host_)) {
      const auto& scheme_host_port = absl::get<url::SchemeHostPort>(host_);

      DCHECK(!scheme_host_port.host().empty() &&
             scheme_host_port.host().front() != '.');

      // Normalize ws/wss schemes to http/https.
      base::StringPiece normalized_scheme = scheme_host_port.scheme();
      if (normalized_scheme == url::kWsScheme) {
        normalized_scheme = url::kHttpScheme;
      } else if (normalized_scheme == url::kWssScheme) {
        normalized_scheme = url::kHttpsScheme;
      }

      // For http-schemed hosts, request the corresponding upgraded https host
      // per the rules in draft-ietf-dnsop-svcb-https-06, Section 8.5.
      uint16_t port = scheme_host_port.port();
      if (normalized_scheme == url::kHttpScheme) {
        normalized_scheme = url::kHttpsScheme;
        if (port == 80)
          port = 443;
      }

      // Scheme should always end up normalized to "https" to create HTTPS
      // transactions.
      DCHECK_EQ(normalized_scheme, url::kHttpsScheme);

      // Per the rules in draft-ietf-dnsop-svcb-https-06, Section 8.1 and 2.3,
      // encode scheme and port in the transaction hostname, unless the port is
      // the default 443.
      if (port != 443) {
        transaction_hostname = base::StrCat({"_", base::NumberToString(port),
                                             "._https.", transaction_hostname});
      }
    }

    std::unique_ptr<DnsTransaction> trans =
        client_->GetTransactionFactory()->CreateTransaction(
            std::move(transaction_hostname),
            DnsQueryTypeToQtype(dns_query_type),
            base::BindOnce(&DnsTask::OnTransactionComplete,
                           base::Unretained(this), tick_clock_->NowTicks(),
                           dns_query_type),
            net_log_, secure_, secure_dns_mode_, &*resolve_context_,
            fallback_available_ /* fast_timeout */);
    trans->SetRequestPriority(delegate_->priority());
    return trans;
  }

  void OnTimeout() {
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK_TIMEOUT,
                      [&] {
                        return NetLogDnsTaskTimeoutParams(transactions_started_,
                                                          transactions_needed_);
                      });

    for (auto& transaction : transactions_started_) {
      base::TimeDelta elapsed_time = tick_clock_->NowTicks() - task_start_time_;

      switch (transaction.second) {
        case DnsQueryType::INTEGRITY:
          DCHECK(httpssvc_metrics_);
          // Don't record provider ID for timeouts. It is not precisely known
          // at this level which provider is actually to blame for the
          // timeout, and breaking metrics out by provider is no longer
          // important for current experimentation goals.
          httpssvc_metrics_->SaveForIntegrity(
              /*doh_provider_id=*/absl::nullopt, HttpssvcDnsRcode::kTimedOut,
              {}, elapsed_time);
          break;
        case DnsQueryType::HTTPS:
          DCHECK(!secure_ ||
                 !features::kUseDnsHttpsSvcbEnforceSecureResponse.Get());
          FALLTHROUGH;
        case DnsQueryType::HTTPS_EXPERIMENTAL:
          if (httpssvc_metrics_) {
            // Don't record provider ID for timeouts. It is not precisely known
            // at this level which provider is actually to blame for the
            // timeout, and breaking metrics out by provider is no longer
            // important for current experimentation goals.
            httpssvc_metrics_->SaveForHttps(
                /*doh_provider_id=*/absl::nullopt, HttpssvcDnsRcode::kTimedOut,
                /*condensed_records=*/{}, elapsed_time);
          }
          break;
        default:
          // The timeout timer is only started when all other transactions have
          // completed.
          NOTREACHED();
      }
    }

    num_completed_transactions_ += transactions_needed_.size();
    transactions_needed_.clear();
    num_completed_transactions_ += transactions_started_.size();
    transactions_started_.clear();
    DCHECK(num_completed_transactions_ == num_needed_transactions());

    ProcessResultsOnCompletion();
  }

  void OnTransactionComplete(const base::TimeTicks& start_time,
                             DnsQueryType dns_query_type,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response,
                             absl::optional<std::string> doh_provider_id) {
    DCHECK(transaction);

    // Once control leaves OnTransactionComplete, there's no further
    // need for the transaction object. On the other hand, since it owns
    // |*response|, it should stay around while OnTransactionComplete
    // executes.
    std::unique_ptr<DnsTransaction> destroy_transaction_on_return;
    {
      auto it = transactions_started_.find(transaction);
      DCHECK(it != transactions_started_.end());

      destroy_transaction_on_return = std::move(it->first);
      transactions_started_.erase(it);
    }

    base::TimeDelta elapsed_time = tick_clock_->NowTicks() - task_start_time_;
    enum HttpssvcDnsRcode rcode_for_httpssvc = HttpssvcDnsRcode::kNoError;
    if (httpssvc_metrics_) {
      if (net_error == ERR_DNS_TIMED_OUT) {
        rcode_for_httpssvc = HttpssvcDnsRcode::kTimedOut;
      } else if (net_error == ERR_NAME_NOT_RESOLVED) {
        rcode_for_httpssvc = HttpssvcDnsRcode::kNoError;
      } else if (response == nullptr) {
        rcode_for_httpssvc = HttpssvcDnsRcode::kMissingDnsResponse;
      } else {
        rcode_for_httpssvc =
            TranslateDnsRcodeForHttpssvcExperiment(response->rcode());
      }
    }

    // Handle network errors. Note that for NXDOMAIN, DnsTransaction returns
    // ERR_NAME_NOT_RESOLVED, so that is not a network error if received with a
    // valid response.
    absl::optional<DnsResponse> fake_response;
    if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                             response->IsValid())) {
      if (dns_query_type == DnsQueryType::INTEGRITY ||
          dns_query_type == DnsQueryType::HTTPS_EXPERIMENTAL ||
          (dns_query_type == DnsQueryType::HTTPS &&
           !IsFatalHttpsTransactionFailure(net_error, response))) {
        // For non-fatal failures, synthesize an empty response.
        fake_response =
            CreateFakeEmptyResponse(GetHostname(host_), dns_query_type);
        response = &fake_response.value();
      } else {
        // Fail completely on network failure.
        OnFailure(net_error, /*ttl=*/absl::nullopt, dns_query_type);
        return;
      }
    } else if (dns_query_type == DnsQueryType::HTTPS) {
      // Just to record metrics about successful HTTPS transactions as a side
      // effect of the IsFatal...() call.
      CHECK(!IsFatalHttpsTransactionFailure(net_error, response));
    }

    HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    DnsResponseResultExtractor extractor(response);
    DnsResponseResultExtractor::ExtractionError extraction_error =
        extractor.ExtractDnsResults(dns_query_type, &results);
    DCHECK_NE(extraction_error,
              DnsResponseResultExtractor::ExtractionError::kUnexpected);

    if (results.error() != OK && results.error() != ERR_NAME_NOT_RESOLVED) {
      net_log_.AddEvent(
          NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK_EXTRACTION_FAILURE,
          [&] {
            return NetLogDnsTaskExtractionFailureParams(
                extraction_error, dns_query_type, results);
          });
      if (dns_query_type == DnsQueryType::INTEGRITY ||
          dns_query_type == DnsQueryType::HTTPS ||
          dns_query_type == DnsQueryType::HTTPS_EXPERIMENTAL) {
        // Ignore extraction errors of these types. In most cases treating them
        // as fatal would only result in fallback to resolution without querying
        // the type. Instead, synthesize empty results.
        results = DnsResponseResultExtractor::CreateEmptyResult(dns_query_type);
      } else {
        OnFailure(results.error(), results.GetOptionalTtl(), dns_query_type);
        return;
      }
    }

    if (httpssvc_metrics_) {
      if (dns_query_type == DnsQueryType::INTEGRITY) {
        const absl::optional<std::vector<bool>>& condensed =
            results.experimental_results();
        CHECK(condensed.has_value());
        // INTEGRITY queries can time out the normal way (here), or when the
        // experimental query timer runs out (OnExperimentalQueryTimeout).
        httpssvc_metrics_->SaveForIntegrity(doh_provider_id, rcode_for_httpssvc,
                                            *condensed, elapsed_time);
      } else if (dns_query_type == DnsQueryType::HTTPS ||
                 dns_query_type == DnsQueryType::HTTPS_EXPERIMENTAL) {
        const absl::optional<std::vector<bool>>& condensed =
            results.experimental_results();
        CHECK(condensed.has_value());
        httpssvc_metrics_->SaveForHttps(doh_provider_id, rcode_for_httpssvc,
                                        *condensed, elapsed_time);
      } else {
        httpssvc_metrics_->SaveForAddressQuery(doh_provider_id, elapsed_time,
                                               rcode_for_httpssvc);
      }
    }

    // Trigger HTTP->HTTPS upgrade if an HTTPS record is received for an "http"
    // or "ws" request.
    if (dns_query_type == DnsQueryType::HTTPS &&
        ShouldTriggerHttpToHttpsUpgrade(results)) {
      OnFailure(ERR_DNS_NAME_HTTPS_ONLY, results.GetOptionalTtl(),
                dns_query_type);
      return;
    }

    // Merge results with saved results from previous transactions.
    if (saved_results_) {
      DCHECK_LE(2, num_needed_transactions());
      DCHECK_LT(num_completed_transactions_, num_needed_transactions());

      switch (dns_query_type) {
        case DnsQueryType::A:
          // Canonical names from A results have lower priority than those
          // from AAAA results, so merge to the back.
          results = HostCache::Entry::MergeEntries(
              std::move(saved_results_).value(), std::move(results));
          break;
        case DnsQueryType::AAAA:
          // Canonical names from AAAA results take priority over those
          // from A results, so merge to the front.
          results = HostCache::Entry::MergeEntries(
              std::move(results), std::move(saved_results_).value());
          break;
        case DnsQueryType::INTEGRITY:
        case DnsQueryType::HTTPS:
        case DnsQueryType::HTTPS_EXPERIMENTAL:
          // No particular importance to order.
          results = HostCache::Entry::MergeEntries(
              std::move(results), std::move(saved_results_).value());
          break;
        default:
          // Only expect address query types with multiple transactions.
          NOTREACHED();
      }
    }

    saved_results_ = std::move(results);

    // If not all transactions are complete, the task cannot yet be completed
    // and the results so far must be saved to merge with additional results.
    ++num_completed_transactions_;
    if (num_completed_transactions_ < num_needed_transactions()) {
      delegate_->OnIntermediateTransactionComplete();
      MaybeStartTimeoutTimer();
      return;
    }

    // Since all transactions are complete, in particular, all experimental or
    // supplemental transactions are complete (if any were started).
    timeout_timer_.Stop();

    ProcessResultsOnCompletion();
  }

  bool IsFatalHttpsTransactionFailure(int transaction_error,
                                      const DnsResponse* response) {
    // These values are logged to UMA. Entries should not be renumbered and
    // numeric values should never be reused. Please keep in sync with
    // "DNS.SvcbHttpsTransactionError" in
    // src/tools/metrics/histograms/enums.xml.
    enum class HttpsTransactionError {
      kNoError = 0,
      kInsecureError = 1,
      kNonFatalError = 2,
      kFatalErrorDisabled = 3,
      kFatalErrorEnabled = 4,
      kMaxValue = kFatalErrorEnabled
    } error;

    if (transaction_error == OK ||
        (transaction_error == ERR_NAME_NOT_RESOLVED && response &&
         response->IsValid())) {
      error = HttpsTransactionError::kNoError;
    } else if (!secure_) {
      // HTTPS failures are never fatal via insecure DNS.
      error = HttpsTransactionError::kInsecureError;
    } else if (transaction_error == ERR_DNS_SERVER_FAILED && response &&
               response->rcode() != dns_protocol::kRcodeSERVFAIL) {
      // For server failures, only SERVFAIL is fatal.
      error = HttpsTransactionError::kNonFatalError;
    } else if (features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
      error = HttpsTransactionError::kFatalErrorEnabled;
    } else {
      error = HttpsTransactionError::kFatalErrorDisabled;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.SvcbHttpsTransactionError",
                              error);
    return error == HttpsTransactionError::kFatalErrorEnabled;
  }

  // Postprocesses the transactions' aggregated results after all
  // transactions have completed.
  void ProcessResultsOnCompletion() {
    DCHECK(saved_results_.has_value());
    HostCache::Entry results = std::move(*saved_results_);

    // If there are multiple addresses, and at least one is IPv6, need to
    // sort them.
    bool at_least_one_ipv6_address =
        results.addresses() && !results.addresses().value().empty() &&
        (results.addresses().value()[0].GetFamily() == ADDRESS_FAMILY_IPV6 ||
         std::any_of(results.addresses().value().begin(),
                     results.addresses().value().end(), [](auto& e) {
                       return e.GetFamily() == ADDRESS_FAMILY_IPV6;
                     }));

    if (at_least_one_ipv6_address) {
      // Sort addresses if needed.  Sort could complete synchronously.
      AddressList addresses = results.addresses().value();
      client_->GetAddressSorter()->Sort(
          addresses,
          base::BindOnce(&DnsTask::OnSortComplete, AsWeakPtr(),
                         tick_clock_->NowTicks(), std::move(results), secure_));
      return;
    }

    OnSuccess(std::move(results));
  }

  void OnSortComplete(base::TimeTicks sort_start_time,
                      HostCache::Entry results,
                      bool secure,
                      bool success,
                      const AddressList& addr_list) {
    results.set_addresses(addr_list);

    if (!success) {
      OnFailure(ERR_DNS_SORT_ERROR, results.GetOptionalTtl());
      return;
    }

    // AddressSorter prunes unusable destinations.
    if (addr_list.empty() &&
        results.text_records().value_or(std::vector<std::string>()).empty() &&
        results.hostnames().value_or(std::vector<HostPortPair>()).empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, results.GetOptionalTtl());
      return;
    }

    OnSuccess(std::move(results));
  }

  // TODO(crbug.com/1225776): Disallow fallback after a fatal HTTPS error.  Also
  // prevent A/AAAA errors from leading to immediate fallback if an HTTPS query
  // is still pending that may lead to a fatal HTTPS error.
  void OnFailure(
      int net_error,
      absl::optional<base::TimeDelta> ttl = absl::nullopt,
      absl::optional<DnsQueryType> failed_transaction_type = absl::nullopt) {
    if (httpssvc_metrics_ && failed_transaction_type &&
        IsAddressType(failed_transaction_type.value())) {
      httpssvc_metrics_->SaveAddressQueryFailure();
    }

    DCHECK_NE(OK, net_error);

    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK, [&] {
      return NetLogDnsTaskFailedParams(net_error, failed_transaction_type, ttl,
                                       base::OptionalOrNullptr(saved_results_));
    });

    HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN, ttl);
    delegate_->OnDnsTaskComplete(task_start_time_, std::move(results), secure_);
  }

  void OnSuccess(HostCache::Entry results) {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK,
                      [&] { return NetLogResults(results); });
    delegate_->OnDnsTaskComplete(task_start_time_, std::move(results), secure_);
  }

  // Returns whether any transactions left to finish are of a transaction type
  // in `types`. Used for logging and starting the timeout timer (see
  // MaybeStartTimeoutTimer()).
  bool AnyOfTypeTransactionsRemain(
      std::initializer_list<DnsQueryType> types) const {
    // Should only be called if some transactions are still running or waiting
    // to run.
    DCHECK(!transactions_needed_.empty() || !transactions_started_.empty());

    // Check running transactions.
    if (base::ranges::find_first_of(transactions_started_, types, /*pred=*/{},
                                    /*proj1=*/[](const auto& transaction) {
                                      return transaction.second;
                                    }) != transactions_started_.end()) {
      return true;
    }

    // Check queued transactions, in case it ever becomes possible to get here
    // without the transactions being started first.
    return base::ranges::find_first_of(transactions_needed_, types) !=
           transactions_needed_.end();
  }

  void MaybeStartTimeoutTimer() {
    // Should only be called if some transactions are still running or waiting
    // to run.
    DCHECK(!transactions_started_.empty() || !transactions_needed_.empty());

    // Timer already running.
    if (timeout_timer_.IsRunning())
      return;

    // Always wait for address transactions.
    if (AnyOfTypeTransactionsRemain({DnsQueryType::A, DnsQueryType::AAAA}))
      return;

    base::TimeDelta timeout;
    int extra_time_percent = 0;

    if (AnyOfTypeTransactionsRemain({DnsQueryType::HTTPS})) {
      DCHECK(base::FeatureList::IsEnabled(features::kUseDnsHttpsSvcb));

      // Skip timeout for secure requests if the timeout would be a fatal
      // failure.
      if (!secure_ || !features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
        timeout = features::kUseDnsHttpsSvcbExtraTimeAbsolute.Get();
        extra_time_percent = features::kUseDnsHttpsSvcbExtraTimePercent.Get();
      }
    } else if (AnyOfTypeTransactionsRemain(
                   {DnsQueryType::INTEGRITY,
                    DnsQueryType::HTTPS_EXPERIMENTAL})) {
      DCHECK(base::FeatureList::IsEnabled(features::kDnsHttpssvc));
      timeout = features::dns_httpssvc_experiment::GetExtraTimeAbsolute();
      extra_time_percent = features::kDnsHttpssvcExtraTimePercent.Get();
    } else {
      // Unhandled supplemental type.
      NOTREACHED();
    }

    if (extra_time_percent > 0) {
      base::TimeDelta total_time_for_other_transactions =
          tick_clock_->NowTicks() - task_start_time_;
      base::TimeDelta relative_timeout =
          total_time_for_other_transactions * extra_time_percent / 100;
      // Use at least 1ms to ensure timeout doesn't occur immediately in tests.
      relative_timeout = std::max(relative_timeout, base::Milliseconds(1));

      if (timeout.is_zero()) {
        timeout = relative_timeout;
      } else {
        timeout = std::min(timeout, relative_timeout);
      }
    }

    if (!timeout.is_zero())
      timeout_timer_.Start(
          FROM_HERE, timeout,
          base::BindOnce(&DnsTask::OnTimeout, base::Unretained(this)));
  }

  bool ShouldTriggerHttpToHttpsUpgrade(const HostCache::Entry& results) {
    // These values are logged to UMA. Entries should not be renumbered and
    // numeric values should never be reused. Please keep in sync with
    // "DNS.HttpUpgradeResult" in src/tools/metrics/histograms/enums.xml.
    enum class UpgradeResult {
      kUpgradeTriggered = 0,
      kNoHttpsRecord = 1,
      kHttpsScheme = 2,
      kOtherScheme = 3,
      kUpgradeDisabled = 4,
      kMaxValue = kUpgradeDisabled
    } upgrade_result;

    if (results.error() != OK) {
      upgrade_result = UpgradeResult::kNoHttpsRecord;
    } else if (GetScheme(host_) == url::kHttpsScheme ||
               GetScheme(host_) == url::kWssScheme) {
      upgrade_result = UpgradeResult::kHttpsScheme;
    } else if (GetScheme(host_) != url::kHttpScheme &&
               GetScheme(host_) != url::kWsScheme) {
      // This is an unusual case because HTTPS would normally not be requested
      // if the scheme is not http(s):// or ws(s)://.
      upgrade_result = UpgradeResult::kOtherScheme;
    } else if (!features::kUseDnsHttpsSvcbHttpUpgrade.Get()) {
      upgrade_result = UpgradeResult::kUpgradeDisabled;
    } else {
      upgrade_result = UpgradeResult::kUpgradeTriggered;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.HttpUpgrade", upgrade_result);
    return upgrade_result == UpgradeResult::kUpgradeTriggered;
  }

  raw_ptr<DnsClient> client_;

  absl::variant<url::SchemeHostPort, std::string> host_;

  base::SafeRef<ResolveContext> resolve_context_;

  // Whether lookups in this DnsTask should occur using DoH or plaintext.
  const bool secure_;
  const SecureDnsMode secure_dns_mode_;

  // The listener to the results of this DnsTask.
  raw_ptr<Delegate> delegate_;
  const NetLogWithSource net_log_;

  base::circular_deque<DnsQueryType> transactions_needed_;
  base::flat_map<std::unique_ptr<DnsTransaction>,
                 DnsQueryType,
                 base::UniquePtrComparator>
      transactions_started_;
  int num_needed_transactions_;
  int num_completed_transactions_;

  // Result from previously completed transactions. Only set if a transaction
  // has completed while others are still in progress.
  absl::optional<HostCache::Entry> saved_results_;

  raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks task_start_time_;

  HttpssvcExperimentDomainCache httpssvc_domain_cache_;
  absl::optional<HttpssvcMetrics> httpssvc_metrics_;

  // Timer for task timeout. Generally started after completion of address
  // transactions to allow aborting experimental or supplemental transactions.
  base::OneShotTimer timeout_timer_;

  // If true, there are still significant fallback options available if this
  // task completes unsuccessfully. Used as a signal that underlying
  // transactions should timeout more quickly.
  bool fallback_available_;
};

//-----------------------------------------------------------------------------

struct HostResolverManager::JobKey {
  JobKey(ResolveContext* resolve_context)
      : resolve_context(resolve_context->AsSafeRef()) {}

  bool operator<(const JobKey& other) const {
    return std::forward_as_tuple(query_type, flags, source, secure_dns_mode,
                                 &*resolve_context, host,
                                 network_isolation_key) <
           std::forward_as_tuple(other.query_type, other.flags, other.source,
                                 other.secure_dns_mode, &*other.resolve_context,
                                 other.host, other.network_isolation_key);
  }

  absl::variant<url::SchemeHostPort, std::string> host;
  NetworkIsolationKey network_isolation_key;
  DnsQueryType query_type;
  HostResolverFlags flags;
  HostResolverSource source;
  SecureDnsMode secure_dns_mode;
  base::SafeRef<ResolveContext> resolve_context;

  HostCache::Key ToCacheKey(bool secure) const {
    HostCache::Key key(host, query_type, flags, source, network_isolation_key);
    key.secure = secure;
    return key;
  }
};

// Aggregates all Requests for the same Key. Dispatched via
// PrioritizedDispatcher.
class HostResolverManager::Job : public PrioritizedDispatcher::Job,
                                 public HostResolverManager::DnsTask::Delegate {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(const base::WeakPtr<HostResolverManager>& resolver,
      JobKey key,
      ResolveHostParameters::CacheUsage cache_usage,
      HostCache* host_cache,
      std::deque<TaskType> tasks,
      RequestPriority priority,
      scoped_refptr<base::TaskRunner> proc_task_runner,
      const NetLogWithSource& source_net_log,
      const base::TickClock* tick_clock)
      : resolver_(resolver),
        key_(std::move(key)),
        cache_usage_(cache_usage),
        host_cache_(host_cache),
        tasks_(tasks),
        job_running_(false),
        priority_tracker_(priority),
        proc_task_runner_(std::move(proc_task_runner)),
        had_non_speculative_request_(false),
        num_occupied_job_slots_(0),
        dispatched_(false),
        dns_task_error_(OK),
        tick_clock_(tick_clock),
        net_log_(
            NetLogWithSource::Make(source_net_log.net_log(),
                                   NetLogSourceType::HOST_RESOLVER_IMPL_JOB)) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_CREATE_JOB);

    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB, [&] {
      return NetLogJobCreationParams(source_net_log.source());
    });
  }

  ~Job() override {
    bool was_queued = is_queued();
    bool was_running = is_running();
    // Clean up now for nice NetLog.
    Finish();
    if (was_running) {
      // This Job was destroyed while still in flight.
      net_log_.EndEventWithNetErrorCode(
          NetLogEventType::HOST_RESOLVER_MANAGER_JOB, ERR_ABORTED);
    } else if (was_queued) {
      // Job was cancelled before it could run.
      // TODO(szym): is there any benefit in having this distinction?
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB);
    }
    // else CompleteRequests logged EndEvent.
    while (!requests_.empty()) {
      // Log any remaining Requests as cancelled.
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      DCHECK_EQ(this, req->job());
      req->OnJobCancelled(this);
    }
  }

  // Add this job to the dispatcher.  If "at_head" is true, adds at the front
  // of the queue.
  void Schedule(bool at_head) {
    DCHECK(!is_queued());
    PrioritizedDispatcher::Handle handle;
    DCHECK(dispatched_);
    if (!at_head) {
      handle = resolver_->dispatcher_->Add(this, priority());
    } else {
      handle = resolver_->dispatcher_->AddAtHead(this, priority());
    }
    // The dispatcher could have started |this| in the above call to Add, which
    // could have called Schedule again. In that case |handle| will be null,
    // but |handle_| may have been set by the other nested call to Schedule.
    if (!handle.is_null()) {
      DCHECK(handle_.is_null());
      handle_ = handle;
    }
  }

  void AddRequest(RequestImpl* request) {
    // Job currently assumes a 1:1 correspondence between ResolveContext and
    // HostCache. Since the ResolveContext is part of the JobKey, any request
    // added to any existing Job should share the same HostCache.
    DCHECK_EQ(host_cache_, request->host_cache());
    // TODO(crbug.com/1206799): Check equality of whole host once Jobs are
    // separated by scheme/port.
    DCHECK_EQ(GetHostname(key_.host), GetHostname(request->request_host()));

    request->AssignJob(this);

    priority_tracker_.Add(request->priority());

    request->source_net_log().AddEventReferencingSource(
        NetLogEventType::HOST_RESOLVER_MANAGER_JOB_ATTACH, net_log_.source());

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB_REQUEST_ATTACH,
                      [&] {
                        return NetLogJobAttachParams(
                            request->source_net_log().source(), priority());
                      });

    if (!request->parameters().is_speculative)
      had_non_speculative_request_ = true;

    requests_.Append(request);

    UpdatePriority();
  }

  void ChangeRequestPriority(RequestImpl* req, RequestPriority priority) {
    // TODO(crbug.com/1206799): Check equality of whole host once Jobs are
    // separated by scheme/port.
    DCHECK_EQ(GetHostname(key_.host), GetHostname(req->request_host()));

    priority_tracker_.Remove(req->priority());
    req->set_priority(priority);
    priority_tracker_.Add(req->priority());
    UpdatePriority();
  }

  // Detach cancelled request. If it was the last active Request, also finishes
  // this Job.
  void CancelRequest(RequestImpl* request) {
    // TODO(crbug.com/1206799): Check equality of whole host once Jobs are
    // separated by scheme/port.
    DCHECK_EQ(GetHostname(key_.host), GetHostname(request->request_host()));
    DCHECK(!requests_.empty());

    priority_tracker_.Remove(request->priority());
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB_REQUEST_DETACH,
                      [&] {
                        return NetLogJobAttachParams(
                            request->source_net_log().source(), priority());
                      });

    if (num_active_requests() > 0) {
      UpdatePriority();
      request->RemoveFromList();
    } else {
      // If we were called from a Request's callback within CompleteRequests,
      // that Request could not have been cancelled, so num_active_requests()
      // could not be 0. Therefore, we are not in CompleteRequests().
      CompleteRequestsWithError(ERR_FAILED /* cancelled */);
    }
  }

  // Called from AbortAllJobs. Completes all requests and destroys
  // the job. This currently assumes the abort is due to a network change.
  // TODO This should not delete |this|.
  void Abort() {
    CompleteRequestsWithError(ERR_NETWORK_CHANGED);
  }

  // Gets a closure that will abort an insecure DnsTask (see
  // AbortInsecureDnsTask()) iff |this| is still valid. Useful if aborting a
  // list of Jobs as some may be cancelled while aborting others.
  base::OnceClosure GetAbortInsecureDnsTaskClosure(int error,
                                                   bool fallback_only) {
    return base::BindOnce(&Job::AbortInsecureDnsTask,
                          weak_ptr_factory_.GetWeakPtr(), error, fallback_only);
  }

  // Aborts or removes any current/future insecure DnsTasks if a ProcTask is
  // available for fallback. If no fallback is available and |fallback_only| is
  // false, a job that is currently running an insecure DnsTask will be
  // completed with |error|.
  void AbortInsecureDnsTask(int error, bool fallback_only) {
    bool has_proc_fallback =
        std::find(tasks_.begin(), tasks_.end(), TaskType::PROC) != tasks_.end();
    if (has_proc_fallback) {
      for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (*it == TaskType::DNS)
          it = tasks_.erase(it);
        else
          ++it;
      }
    }

    if (dns_task_ && !dns_task_->secure()) {
      if (has_proc_fallback) {
        KillDnsTask();
        dns_task_error_ = OK;
        RunNextTask();
      } else if (!fallback_only) {
        CompleteRequestsWithError(error);
      }
    }
  }

  // Called by HostResolverManager when this job is evicted due to queue
  // overflow. Completes all requests and destroys the job. The job could have
  // waiting requests that will receive completion callbacks, so cleanup
  // asynchronously to avoid reentrancy.
  void OnEvicted() {
    DCHECK(!is_running());
    DCHECK(is_queued());
    handle_.Reset();

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB_EVICTED);

    // This signals to CompleteRequests that parts of this job never ran.
    // Job must be saved in |resolver_| to be completed asynchronously.
    // Otherwise the job will be destroyed with requests silently cancelled
    // before completion runs.
    DCHECK(self_iterator_);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Job::CompleteRequestsWithError,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    absl::optional<HostCache::Entry> results = resolver_->ServeFromHosts(
        GetHostname(key_.host), key_.query_type,
        key_.flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6, tasks_);
    if (results) {
      // This will destroy the Job.
      CompleteRequests(results.value(), base::TimeDelta(),
                       true /* allow_cache */, true /* secure */);
      return true;
    }
    return false;
  }

  void OnAddedToJobMap(JobMap::iterator iterator) {
    DCHECK(!self_iterator_);
    DCHECK(iterator != resolver_->jobs_.end());
    self_iterator_ = iterator;
  }

  void OnRemovedFromJobMap() {
    DCHECK(self_iterator_);
    self_iterator_ = absl::nullopt;
  }

  void RunNextTask() {
    // If there are no tasks left to try, cache any stored results and complete
    // the request with the last stored result. All stored results should be
    // errors.
    if (tasks_.empty()) {
      // If there are no stored results, complete with an error.
      if (completion_results_.size() == 0) {
        CompleteRequestsWithError(ERR_NAME_NOT_RESOLVED);
        return;
      }

      // Cache all but the last result here. The last result will be cached
      // as part of CompleteRequests.
      for (size_t i = 0; i < completion_results_.size() - 1; ++i) {
        const auto& result = completion_results_[i];
        DCHECK_NE(OK, result.entry.error());
        MaybeCacheResult(result.entry, result.ttl, result.secure);
      }
      const auto& last_result = completion_results_.back();
      DCHECK_NE(OK, last_result.entry.error());
      CompleteRequests(last_result.entry, last_result.ttl,
                       true /* allow_cache */, last_result.secure);
      return;
    }

    TaskType next_task = tasks_.front();

    // Schedule insecure DnsTasks and ProcTasks with the dispatcher.
    if (!dispatched_ &&
        (next_task == TaskType::DNS || next_task == TaskType::PROC ||
         next_task == TaskType::MDNS)) {
      dispatched_ = true;
      job_running_ = false;
      Schedule(false);
      DCHECK(is_running() || is_queued());

      // Check for queue overflow.
      PrioritizedDispatcher& dispatcher = *resolver_->dispatcher_;
      if (dispatcher.num_queued_jobs() > resolver_->max_queued_jobs_) {
        Job* evicted = static_cast<Job*>(dispatcher.EvictOldestLowest());
        DCHECK(evicted);
        evicted->OnEvicted();
      }
      return;
    }

    if (start_time_ == base::TimeTicks()) {
      net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_JOB_STARTED);
      start_time_ = tick_clock_->NowTicks();
    }
    tasks_.pop_front();
    job_running_ = true;

    switch (next_task) {
      case TaskType::PROC:
        StartProcTask();
        break;
      case TaskType::DNS:
        StartDnsTask(false /* secure */);
        break;
      case TaskType::SECURE_DNS:
        StartDnsTask(true /* secure */);
        break;
      case TaskType::MDNS:
        StartMdnsTask();
        break;
      case TaskType::INSECURE_CACHE_LOOKUP:
        InsecureCacheLookup();
        break;
      case TaskType::SECURE_CACHE_LOOKUP:
      case TaskType::CACHE_LOOKUP:
        // These task types should have been handled synchronously in
        // ResolveLocally() prior to Job creation.
        NOTREACHED();
        break;
    }
  }

  bool is_queued() const { return !handle_.is_null(); }

  bool is_running() const { return job_running_; }

 private:
  base::Value NetLogJobCreationParams(const NetLogSource& source) {
    base::Value dict(base::Value::Type::DICTIONARY);
    source.AddToEventParameters(&dict);
    dict.SetKey("host", ToLogStringValue(key_.host));
    dict.SetIntKey("dns_query_type", static_cast<int>(key_.query_type));
    dict.SetIntKey("secure_dns_mode", static_cast<int>(key_.secure_dns_mode));
    dict.SetStringKey("network_isolation_key",
                      key_.network_isolation_key.ToDebugString());
    return dict;
  }

  void Finish() {
    if (is_running()) {
      // Clean up but don't run any callbacks.
      proc_task_ = nullptr;
      KillDnsTask();
      mdns_task_ = nullptr;
      job_running_ = false;

      if (dispatched_) {
        // Job should only ever occupy one slot after any tasks that may have
        // required additional slots, e.g. DnsTask, have been killed, and
        // additional slots are expected to be vacated as part of killing the
        // task.
        DCHECK_EQ(1, num_occupied_job_slots_);
        if (resolver_)
          resolver_->dispatcher_->OnJobFinished();
        num_occupied_job_slots_ = 0;
      }
    } else if (is_queued()) {
      DCHECK(dispatched_);
      if (resolver_)
        resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    }
  }

  void KillDnsTask() {
    if (dns_task_) {
      if (dispatched_) {
        while (num_occupied_job_slots_ > 1 || is_queued()) {
          ReduceByOneJobSlot();
        }
      }
      dns_task_.reset();
    }
  }

  // Reduce the number of job slots occupied and queued in the dispatcher by
  // one. If the next Job slot is queued in the dispatcher, cancels the queued
  // job. Otherwise, the next Job has been started by the PrioritizedDispatcher,
  // so signals it is complete.
  void ReduceByOneJobSlot() {
    DCHECK_GE(num_occupied_job_slots_, 1);
    DCHECK(dispatched_);
    if (is_queued()) {
      if (resolver_)
        resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    } else if (num_occupied_job_slots_ > 1) {
      if (resolver_)
        resolver_->dispatcher_->OnJobFinished();
      --num_occupied_job_slots_;
    } else {
      NOTREACHED();
    }
  }

  void UpdatePriority() {
    if (is_queued())
      handle_ = resolver_->dispatcher_->ChangePriority(handle_, priority());
  }

  // PrioritizedDispatcher::Job:
  void Start() override {
    handle_.Reset();
    ++num_occupied_job_slots_;

    if (num_occupied_job_slots_ >= 2) {
      if (!dns_task_) {
        resolver_->dispatcher_->OnJobFinished();
        return;
      }
      StartNextDnsTransaction();
      if (dns_task_->needs_another_transaction()) {
        Schedule(true);
      }
      return;
    }

    DCHECK(!is_running());
    DCHECK(!tasks_.empty());
    RunNextTask();
    // Caution: Job::Start must not complete synchronously.
  }

  // TODO(szym): Since DnsTransaction does not consume threads, we can increase
  // the limits on |dispatcher_|. But in order to keep the number of
  // ThreadPool threads low, we will need to use an "inner"
  // PrioritizedDispatcher with tighter limits.
  void StartProcTask() {
    DCHECK(dispatched_);
    DCHECK_EQ(1, num_occupied_job_slots_);
    DCHECK(IsAddressType(key_.query_type));

    proc_task_ = std::make_unique<ProcTask>(
        std::string(GetHostname(key_.host)),
        HostResolver::DnsQueryTypeToAddressFamily(key_.query_type), key_.flags,
        resolver_->proc_params_,
        base::BindOnce(&Job::OnProcTaskComplete, base::Unretained(this),
                       tick_clock_->NowTicks()),
        proc_task_runner_, net_log_, tick_clock_);

    // Start() could be called from within Resolve(), hence it must NOT directly
    // call OnProcTaskComplete, for example, on synchronous failure.
    proc_task_->Start();
  }

  // Called by ProcTask when it completes.
  void OnProcTaskComplete(base::TimeTicks start_time,
                          int net_error,
                          const AddressList& addr_list) {
    DCHECK(proc_task_);

    if (dns_task_error_ != OK && net_error == OK) {
      // This ProcTask was a fallback resolution after a failed insecure
      // DnsTask.
      resolver_->OnFallbackResolve(dns_task_error_);
    }

    if (ContainsIcannNameCollisionIp(addr_list))
      net_error = ERR_ICANN_NAME_COLLISION;

    base::TimeDelta ttl = base::Seconds(kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::Seconds(kCacheEntryTTLSeconds);

    // Source unknown because the system resolver could have gotten it from a
    // hosts file, its own cache, a DNS lookup or somewhere else.
    // Don't store the |ttl| in cache since it's not obtained from the server.
    CompleteRequests(
        HostCache::Entry(net_error,
                         net_error == OK
                             ? AddressList::CopyWithPort(addr_list, 0)
                             : AddressList(),
                         HostCache::Entry::SOURCE_UNKNOWN),
        ttl, true /* allow_cache */, false /* secure */);
  }

  void InsecureCacheLookup() {
    // Insecure cache lookups for requests allowing stale results should have
    // occurred prior to Job creation.
    DCHECK(cache_usage_ != ResolveHostParameters::CacheUsage::STALE_ALLOWED);
    absl::optional<HostCache::EntryStaleness> stale_info;
    absl::optional<HostCache::Entry> resolved = resolver_->MaybeServeFromCache(
        host_cache_, key_.ToCacheKey(/*secure=*/false), cache_usage_,
        false /* ignore_secure */, net_log_, &stale_info);

    if (resolved) {
      DCHECK(stale_info);
      DCHECK(!stale_info.value().is_stale());
      CompleteRequestsWithoutCache(resolved.value(), std::move(stale_info));
    } else {
      RunNextTask();
    }
  }

  void StartDnsTask(bool secure) {
    DCHECK_EQ(secure, !dispatched_);
    DCHECK_EQ(dispatched_ ? 1 : 0, num_occupied_job_slots_);
    DCHECK(!resolver_->HaveTestProcOverride());
    // Need to create the task even if we're going to post a failure instead of
    // running it, as a "started" job needs a task to be properly cleaned up.
    dns_task_ = std::make_unique<DnsTask>(
        resolver_->dns_client_.get(), key_.host, key_.query_type,
        &*key_.resolve_context, secure, key_.secure_dns_mode, this, net_log_,
        tick_clock_, !tasks_.empty() /* fallback_available */);
    dns_task_->StartNextTransaction();
    // Schedule a second transaction, if needed. DoH queries can bypass the
    // dispatcher and start all of their transactions immediately.
    if (secure) {
      while (dns_task_->needs_another_transaction())
        dns_task_->StartNextTransaction();
    } else if (dns_task_->needs_another_transaction()) {
      Schedule(true);
    }
  }

  void StartNextDnsTransaction() {
    DCHECK(dns_task_);
    DCHECK_EQ(dns_task_->secure(), !dispatched_);
    DCHECK(!dispatched_ || num_occupied_job_slots_ >= 1);
    DCHECK(dns_task_->needs_another_transaction());
    dns_task_->StartNextTransaction();
  }

  // Called if DnsTask fails. It is posted from StartDnsTask, so Job may be
  // deleted before this callback. In this case dns_task is deleted as well,
  // so we use it as indicator whether Job is still valid.
  void OnDnsTaskFailure(const base::WeakPtr<DnsTask>& dns_task,
                        base::TimeDelta duration,
                        const HostCache::Entry& failure_results,
                        bool secure) {
    DCHECK_NE(OK, failure_results.error());

    if (key_.secure_dns_mode == SecureDnsMode::kSecure) {
      DCHECK(secure);
      UMA_HISTOGRAM_LONG_TIMES_100(
          "Net.DNS.SecureDnsTask.DnsModeSecure.FailureTime", duration);
    } else if (key_.secure_dns_mode == SecureDnsMode::kAutomatic && secure) {
      UMA_HISTOGRAM_LONG_TIMES_100(
          "Net.DNS.SecureDnsTask.DnsModeAutomatic.FailureTime", duration);
    } else {
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.InsecureDnsTask.FailureTime",
                                   duration);
    }

    if (!dns_task)
      return;

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.Failure",
                                 total_transaction_time_queued_);

    // If one of the fallback tasks doesn't complete the request, store a result
    // to use during request completion.
    base::TimeDelta ttl =
        failure_results.has_ttl() ? failure_results.ttl() : base::Seconds(0);
    completion_results_.push_back({failure_results, ttl, secure});

    dns_task_error_ = failure_results.error();
    KillDnsTask();
    RunNextTask();
  }

  // HostResolverManager::DnsTask::Delegate implementation:

  void OnDnsTaskComplete(base::TimeTicks start_time,
                         HostCache::Entry results,
                         bool secure) override {
    DCHECK(dns_task_);

    // `UNSPECIFIED`-type queries are only considered successful overall if they
    // find address results, but DnsTask may claim success if any transaction,
    // e.g. a supplemental HTTPS transaction, finds results.
    if (key_.query_type == DnsQueryType::UNSPECIFIED && results.error() == OK &&
        (!results.addresses() || results.addresses().value().empty())) {
      results.set_error(ERR_NAME_NOT_RESOLVED);
    }

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (results.error() != OK) {
      OnDnsTaskFailure(dns_task_->AsWeakPtr(), duration, results, secure);
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.DnsTask.SuccessTime", duration);

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.Success",
                                 total_transaction_time_queued_);

    // Reset the insecure DNS failure counter if an insecure DnsTask completed
    // successfully.
    if (!secure)
      resolver_->dns_client_->ClearInsecureFallbackFailures();

    base::TimeDelta bounded_ttl =
        std::max(results.ttl(), base::Seconds(kMinimumTTLSeconds));

    if (results.addresses() &&
        ContainsIcannNameCollisionIp(results.addresses().value())) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
      return;
    }

    CompleteRequests(results, bounded_ttl, true /* allow_cache */, secure);
  }

  void OnIntermediateTransactionComplete() override {
    DCHECK_LE(2, dns_task_->num_needed_transactions());
    DCHECK_EQ(dns_task_->needs_another_transaction(), is_queued());

    if (dispatched_) {
      // We already have a job slot at the dispatcher, so if the next
      // transaction hasn't started, reuse it now instead of waiting in the
      // queue for another slot.
      if (!dns_task_->needs_another_transaction()) {
        // The DnsTask has no more transactions, so we can relinquish this slot.
        DCHECK(!is_queued());
        ReduceByOneJobSlot();
      } else {
        dns_task_->StartNextTransaction();
        if (!dns_task_->needs_another_transaction() && is_queued()) {
          resolver_->dispatcher_->Cancel(handle_);
          handle_.Reset();
        }
      }
    } else if (dns_task_->needs_another_transaction()) {
      dns_task_->StartNextTransaction();
    }
  }

  void AddTransactionTimeQueued(base::TimeDelta time_queued) override {
    total_transaction_time_queued_ += time_queued;
  }

  void StartMdnsTask() {
    // No flags are supported for MDNS except
    // HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6 (which is not actually an
    // input flag).
    DCHECK_EQ(0, key_.flags & ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);

    std::vector<DnsQueryType> query_types;
    if (key_.query_type == DnsQueryType::UNSPECIFIED) {
      query_types.push_back(DnsQueryType::A);
      query_types.push_back(DnsQueryType::AAAA);
    } else {
      query_types.push_back(key_.query_type);
    }

    MDnsClient* client = nullptr;
    int rv = resolver_->GetOrCreateMdnsClient(&client);
    mdns_task_ = std::make_unique<HostResolverMdnsTask>(
        client, std::string(GetHostname(key_.host)), query_types);

    if (rv == OK) {
      mdns_task_->Start(
          base::BindOnce(&Job::OnMdnsTaskComplete, base::Unretained(this)));
    } else {
      // Could not create an mDNS client. Since we cannot complete synchronously
      // from here, post a failure without starting the task.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Job::OnMdnsImmediateFailure,
                                    weak_ptr_factory_.GetWeakPtr(), rv));
    }
  }

  void OnMdnsTaskComplete() {
    DCHECK(mdns_task_);
    // TODO(crbug.com/846423): Consider adding MDNS-specific logging.

    HostCache::Entry results = mdns_task_->GetResults();
    if (results.addresses() &&
        ContainsIcannNameCollisionIp(results.addresses().value())) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
    } else {
      // MDNS uses a separate cache, so skip saving result to cache.
      // TODO(crbug.com/926300): Consider merging caches.
      CompleteRequestsWithoutCache(results, absl::nullopt /* stale_info */);
    }
  }

  void OnMdnsImmediateFailure(int rv) {
    DCHECK(mdns_task_);
    DCHECK_NE(OK, rv);

    CompleteRequestsWithError(rv);
  }

  void RecordJobHistograms(int error) {
    // Used in UMA_HISTOGRAM_ENUMERATION. Do not renumber entries or reuse
    // deprecated values.
    enum Category {
      RESOLVE_SUCCESS = 0,
      RESOLVE_FAIL = 1,
      RESOLVE_SPECULATIVE_SUCCESS = 2,
      RESOLVE_SPECULATIVE_FAIL = 3,
      RESOLVE_ABORT = 4,
      RESOLVE_SPECULATIVE_ABORT = 5,
      RESOLVE_MAX,  // Bounding value.
    };
    Category category = RESOLVE_MAX;  // Illegal value for later DCHECK only.

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time_;
    if (error == OK) {
      if (had_non_speculative_request_) {
        category = RESOLVE_SUCCESS;
        UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime", duration);
      } else {
        category = RESOLVE_SPECULATIVE_SUCCESS;
      }
    } else if (error == ERR_NETWORK_CHANGED ||
               error == ERR_HOST_RESOLVER_QUEUE_TOO_LARGE) {
      category = had_non_speculative_request_ ? RESOLVE_ABORT
                                              : RESOLVE_SPECULATIVE_ABORT;
    } else {
      if (had_non_speculative_request_) {
        category = RESOLVE_FAIL;
        UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime", duration);
      } else {
        category = RESOLVE_SPECULATIVE_FAIL;
      }
    }
    DCHECK_LT(static_cast<int>(category),
              static_cast<int>(RESOLVE_MAX));  // Be sure it was set.
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.ResolveCategory", category, RESOLVE_MAX);

    if (category == RESOLVE_FAIL ||
        (start_time_ != base::TimeTicks() && category == RESOLVE_ABORT)) {
      if (duration < base::Milliseconds(10))
        base::UmaHistogramSparse("Net.DNS.ResolveError.Fast", std::abs(error));
      else
        base::UmaHistogramSparse("Net.DNS.ResolveError.Slow", std::abs(error));
    }

    if (had_non_speculative_request_) {
      UmaHistogramMediumTimes(
          base::StringPrintf(
              "Net.DNS.SecureDnsMode.%s.ResolveTime",
              SecureDnsModeToString(key_.secure_dns_mode).c_str()),
          duration);
    }
  }

  void MaybeCacheResult(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool secure) {
    // If the request did not complete, don't cache it.
    if (!results.did_complete())
      return;
    resolver_->CacheResult(host_cache_, key_.ToCacheKey(secure), results, ttl);
  }

  // Performs Job's last rites. Completes all Requests. Deletes this.
  //
  // If not |allow_cache|, result will not be stored in the host cache, even if
  // result would otherwise allow doing so. Update the key to reflect |secure|,
  // which indicates whether or not the result was obtained securely.
  void CompleteRequests(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool allow_cache,
                        bool secure) {
    CHECK(resolver_.get());

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, if the job was owned by |jobs_|, the job
    // deletes itself when CompleteRequests is done.
    std::unique_ptr<Job> self_deleter;
    if (self_iterator_)
      self_deleter = resolver_->RemoveJob(self_iterator_.value());

    Finish();

    if (num_active_requests() == 0) {
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEventWithNetErrorCode(
          NetLogEventType::HOST_RESOLVER_MANAGER_JOB, OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(
        NetLogEventType::HOST_RESOLVER_MANAGER_JOB, results.error());

    DCHECK(!requests_.empty());

    // Handle all caching before completing requests as completing requests may
    // start new requests that rely on cached results.
    if (allow_cache)
      MaybeCacheResult(results, ttl, secure);

    RecordJobHistograms(results.error());

    // Complete all of the requests that were attached to the job and
    // detach them.
    while (!requests_.empty()) {
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      DCHECK_EQ(this, req->job());

      if (results.error() == OK && !req->parameters().is_speculative) {
        req->set_results(
            results.CopyWithDefaultPort(GetPort(req->request_host())));

        // TODO(cammie): Move the sanitization deeper, possibly in
        // HttpCache::Entry::SetResult(AddressList addresses), so that it
        // doesn't happen on a per-request basis.
        req->SanitizeDnsAliasResults();
      }
      req->OnJobCompleted(
          this, results.error(),
          secure && results.error() != OK /* is_secure_network_error */);

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_.get())
        return;
    }
  }

  void CompleteRequestsWithoutCache(
      const HostCache::Entry& results,
      absl::optional<HostCache::EntryStaleness> stale_info) {
    // Record the stale_info for all non-speculative requests, if it exists.
    if (stale_info) {
      for (auto* node = requests_.head(); node != requests_.end();
           node = node->next()) {
        if (!node->value()->parameters().is_speculative)
          node->value()->set_stale_info(stale_info.value());
      }
    }
    CompleteRequests(results, base::TimeDelta(), false /* allow_cache */,
                     false /* secure */);
  }

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error) {
    DCHECK_NE(OK, net_error);
    CompleteRequests(
        HostCache::Entry(net_error, HostCache::Entry::SOURCE_UNKNOWN),
        base::TimeDelta(), true /* allow_cache */, false /* secure */);
  }

  RequestPriority priority() const override {
    return priority_tracker_.highest_priority();
  }

  // Number of non-canceled requests in |requests_|.
  size_t num_active_requests() const { return priority_tracker_.total_count(); }

  base::WeakPtr<HostResolverManager> resolver_;

  const JobKey key_;
  const ResolveHostParameters::CacheUsage cache_usage_;
  // TODO(crbug.com/969847): Consider allowing requests within a single Job to
  // have different HostCaches.
  const raw_ptr<HostCache> host_cache_;

  struct CompletionResult {
    const HostCache::Entry entry;
    base::TimeDelta ttl;
    bool secure;
  };

  // Results to use in last-ditch attempt to complete request.
  std::vector<CompletionResult> completion_results_;

  // The sequence of tasks to run in this Job. Tasks may be aborted and removed
  // from the sequence, but otherwise the tasks will run in order until a
  // successful result is found.
  std::deque<TaskType> tasks_;

  // Whether the job is running.
  bool job_running_;

  // Tracks the highest priority across |requests_|.
  PriorityTracker priority_tracker_;

  // Task runner used for HostResolverProc.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  bool had_non_speculative_request_;

  // Number of slots occupied by this Job in |dispatcher_|. Should be 0 when
  // the job is not registered with any dispatcher.
  int num_occupied_job_slots_;

  // True once this Job has been sent to `resolver_->dispatcher_`.
  bool dispatched_;

  // Result of DnsTask.
  int dns_task_error_;

  raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks start_time_;

  NetLogWithSource net_log_;

  // Resolves the host using a HostResolverProc.
  std::unique_ptr<ProcTask> proc_task_;

  // Resolves the host using a DnsTransaction.
  std::unique_ptr<DnsTask> dns_task_;

  // Resolves the host using MDnsClient.
  std::unique_ptr<HostResolverMdnsTask> mdns_task_;

  // All Requests waiting for the result of this Job. Some can be canceled.
  base::LinkedList<RequestImpl> requests_;

  // A handle used for |dispatcher_|.
  PrioritizedDispatcher::Handle handle_;

  // Iterator to |this| in the JobMap. |nullopt| if not owned by the JobMap.
  absl::optional<JobMap::iterator> self_iterator_;

  base::TimeDelta total_transaction_time_queued_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

//-----------------------------------------------------------------------------

HostResolverManager::HostResolverManager(
    const HostResolver::ManagerOptions& options,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier,
    NetLog* net_log)
    : max_queued_jobs_(0),
      proc_params_(nullptr, options.max_system_retry_attempts),
      net_log_(net_log),
      system_dns_config_notifier_(system_dns_config_notifier),
      check_ipv6_on_wifi_(options.check_ipv6_on_wifi),
      last_ipv6_probe_result_(true),
      additional_resolver_flags_(0),
      allow_fallback_to_proctask_(true),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      invalidation_in_progress_(false) {
  PrioritizedDispatcher::Limits job_limits = GetDispatcherLimits(options);
  dispatcher_ = std::make_unique<PrioritizedDispatcher>(job_limits);
  max_queued_jobs_ = job_limits.total_jobs * 100u;

  DCHECK_GE(dispatcher_->num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

  proc_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), priority_mode.Get(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
#if (defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->AddObserver(this);
#if defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_OPENBSD) && \
    !defined(OS_ANDROID)
  EnsureDnsReloaderInit();
#endif

  OnConnectionTypeChanged(NetworkChangeNotifier::GetConnectionType());

#if defined(ENABLE_BUILT_IN_DNS)
  dns_client_ = DnsClient::CreateClient(net_log_);
  dns_client_->SetInsecureEnabled(
      options.insecure_dns_client_enabled,
      options.additional_types_via_insecure_dns_enabled);
  dns_client_->SetConfigOverrides(options.dns_config_overrides);
#else
  DCHECK(options.dns_config_overrides == DnsConfigOverrides());
#endif

  allow_fallback_to_proctask_ = !ConfigureAsyncDnsNoFallbackFieldTrial();
}

HostResolverManager::~HostResolverManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Prevent the dispatcher from starting new jobs.
  dispatcher_->SetLimitsToZero();
  // It's now safe for Jobs to call KillDnsTask on destruction, because
  // OnJobComplete will not start any new jobs.
  jobs_.clear();

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->RemoveObserver(this);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverManager::CreateRequest(
    absl::variant<url::SchemeHostPort, HostPortPair> host,
    NetworkIsolationKey network_isolation_key,
    NetLogWithSource net_log,
    absl::optional<ResolveHostParameters> optional_parameters,
    ResolveContext* resolve_context,
    HostCache* host_cache) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

  // ResolveContexts must register (via RegisterResolveContext()) before use to
  // ensure cached data is invalidated on network and configuration changes.
  DCHECK(registered_contexts_.HasObserver(resolve_context));

  return std::make_unique<RequestImpl>(
      std::move(net_log), std::move(host), std::move(network_isolation_key),
      std::move(optional_parameters), resolve_context->GetWeakPtr(), host_cache,
      weak_ptr_factory_.GetWeakPtr(), tick_clock_);
}

std::unique_ptr<HostResolver::ProbeRequest>
HostResolverManager::CreateDohProbeRequest(ResolveContext* context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return std::make_unique<ProbeRequestImpl>(context->GetWeakPtr(),
                                            weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
HostResolverManager::CreateMdnsListener(const HostPortPair& host,
                                        DnsQueryType query_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type);

  auto listener =
      std::make_unique<HostResolverMdnsListenerImpl>(host, query_type);

  MDnsClient* client;
  int rv = GetOrCreateMdnsClient(&client);

  if (rv == OK) {
    std::unique_ptr<net::MDnsListener> inner_listener = client->CreateListener(
        DnsQueryTypeToQtype(query_type), host.host(), listener.get());
    listener->set_inner_listener(std::move(inner_listener));
  } else {
    listener->set_initialization_error(rv);
  }
  return listener;
}

void HostResolverManager::SetInsecureDnsClientEnabled(
    bool enabled,
    bool additional_dns_types_enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!dns_client_)
    return;

  bool enabled_before = dns_client_->CanUseInsecureDnsTransactions();
  bool additional_types_before =
      enabled_before && dns_client_->CanQueryAdditionalTypesViaInsecureDns();
  dns_client_->SetInsecureEnabled(enabled, additional_dns_types_enabled);

  // Abort current tasks if `CanUseInsecureDnsTransactions()` changes or if
  // insecure transactions are enabled and
  // `CanQueryAdditionalTypesViaInsecureDns()` changes. Changes to allowing
  // additional types don't matter if insecure transactions are completely
  // disabled.
  if (dns_client_->CanUseInsecureDnsTransactions() != enabled_before ||
      (dns_client_->CanUseInsecureDnsTransactions() &&
       dns_client_->CanQueryAdditionalTypesViaInsecureDns() !=
           additional_types_before)) {
    AbortInsecureDnsTasks(ERR_NETWORK_CHANGED, false /* fallback_only */);
  }
}

base::Value HostResolverManager::GetDnsConfigAsValue() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!dns_client_.get())
    return base::Value(base::Value::Type::DICTIONARY);

  const DnsConfig* dns_config = dns_client_->GetEffectiveConfig();
  if (!dns_config)
    return base::Value(base::Value::Type::DICTIONARY);

  return dns_config->ToValue();
}

void HostResolverManager::SetDnsConfigOverrides(DnsConfigOverrides overrides) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!dns_client_ && overrides == DnsConfigOverrides())
    return;

  // Not allowed to set overrides if compiled without DnsClient.
  DCHECK(dns_client_);

  bool transactions_allowed_before =
      dns_client_->CanUseSecureDnsTransactions() ||
      dns_client_->CanUseInsecureDnsTransactions();
  bool changed = dns_client_->SetConfigOverrides(std::move(overrides));

  if (changed) {
    NetworkChangeNotifier::TriggerNonSystemDnsChange();

    // Only invalidate cache if new overrides have resulted in a config change.
    InvalidateCaches();

    // Need to update jobs iff transactions were previously allowed because
    // in-progress jobs may be running using a now-invalid configuration.
    if (transactions_allowed_before) {
      UpdateJobsForChangedConfig();
    }
  }
}

void HostResolverManager::RegisterResolveContext(ResolveContext* context) {
  registered_contexts_.AddObserver(context);
  context->InvalidateCachesAndPerSessionData(
      dns_client_ ? dns_client_->GetCurrentSession() : nullptr,
      false /* network_change */);
}

void HostResolverManager::DeregisterResolveContext(
    const ResolveContext* context) {
  registered_contexts_.RemoveObserver(context);

  // Destroy Jobs when their context is closed.
  RemoveAllJobs(context);
}

void HostResolverManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void HostResolverManager::SetMaxQueuedJobsForTesting(size_t value) {
  DCHECK_EQ(0u, dispatcher_->num_queued_jobs());
  DCHECK_GE(value, 0u);
  max_queued_jobs_ = value;
}

void HostResolverManager::SetHaveOnlyLoopbackAddresses(bool result) {
  if (result) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
}

void HostResolverManager::SetMdnsSocketFactoryForTesting(
    std::unique_ptr<MDnsSocketFactory> socket_factory) {
  DCHECK(!mdns_client_);
  mdns_socket_factory_ = std::move(socket_factory);
}

void HostResolverManager::SetMdnsClientForTesting(
    std::unique_ptr<MDnsClient> client) {
  mdns_client_ = std::move(client);
}

void HostResolverManager::SetDnsClientForTesting(
    std::unique_ptr<DnsClient> dns_client) {
  DCHECK(dns_client);
  if (dns_client_) {
    if (!dns_client->GetSystemConfigForTesting())
      dns_client->SetSystemConfig(dns_client_->GetSystemConfigForTesting());
    dns_client->SetConfigOverrides(dns_client_->GetConfigOverridesForTesting());
  }
  dns_client_ = std::move(dns_client);
  // Inform `registered_contexts_` of the new `DnsClient`.
  InvalidateCaches();
}

void HostResolverManager::SetLastIPv6ProbeResultForTesting(
    bool last_ipv6_probe_result) {
  SetLastIPv6ProbeResult(last_ipv6_probe_result);
}

void HostResolverManager::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  proc_task_runner_ = std::move(task_runner);
}

int HostResolverManager::Resolve(RequestImpl* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Request should not yet have a scheduled Job.
  DCHECK(!request->job());
  // Request may only be resolved once.
  DCHECK(!request->complete());
  // MDNS requests do not support skipping cache or stale lookups.
  // TODO(crbug.com/926300): Either add support for skipping the MDNS cache, or
  // merge to use the normal host cache for MDNS requests.
  DCHECK(request->parameters().source != HostResolverSource::MULTICAST_DNS ||
         request->parameters().cache_usage ==
             ResolveHostParameters::CacheUsage::ALLOWED);
  DCHECK(!invalidation_in_progress_);

  const auto& parameters = request->parameters();
  JobKey job_key(request->resolve_context());
  job_key.host =
      CreateHostForJobKey(request->request_host(), parameters.dns_query_type);
  job_key.network_isolation_key = request->network_isolation_key();
  job_key.source = parameters.source;

  IPAddress ip_address;
  bool is_ip = ip_address.AssignFromIPLiteral(GetHostname(job_key.host));

  GetEffectiveParametersForRequest(
      GetHostname(job_key.host), parameters.dns_query_type,
      request->host_resolver_flags(), parameters.secure_dns_policy,
      parameters.cache_usage, is_ip, request->source_net_log(),
      &job_key.query_type, &job_key.flags, &job_key.secure_dns_mode);

  std::deque<TaskType> tasks;
  absl::optional<HostCache::EntryStaleness> stale_info;
  HostCache::Entry results = ResolveLocally(
      job_key, ip_address, parameters.cache_usage, request->source_net_log(),
      request->host_cache(), &tasks, &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY ||
      tasks.empty()) {
    if (results.error() == OK && !request->parameters().is_speculative) {
      request->set_results(
          results.CopyWithDefaultPort(GetPort(request->request_host())));

      // TODO(cammie): Sanitize before adding to the cache instead.
      request->SanitizeDnsAliasResults();
    }
    if (stale_info && !request->parameters().is_speculative)
      request->set_stale_info(std::move(stale_info).value());
    request->set_error_info(results.error(),
                            false /* is_secure_network_error */);
    return HostResolver::SquashErrorCode(results.error());
  }

  CreateAndStartJob(std::move(job_key), std::move(tasks), request);
  return ERR_IO_PENDING;
}

HostCache::Entry HostResolverManager::ResolveLocally(
    const JobKey& job_key,
    const IPAddress& ip_address,
    ResolveHostParameters::CacheUsage cache_usage,
    const NetLogWithSource& source_net_log,
    HostCache* cache,
    std::deque<TaskType>* out_tasks,
    absl::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = absl::nullopt;

  CreateTaskSequence(job_key, cache_usage, out_tasks);

  if (!ip_address.IsValid()) {
    // Check that the caller supplied a valid hostname to resolve. For
    // MULTICAST_DNS, we are less restrictive.
    // TODO(ericorth): Control validation based on an explicit flag rather
    // than implicitly based on |source|.
    const bool is_valid_hostname =
        job_key.source == HostResolverSource::MULTICAST_DNS
            ? IsValidUnrestrictedDNSDomain(GetHostname(job_key.host))
            : IsValidDNSDomain(GetHostname(job_key.host));
    if (!is_valid_hostname) {
      return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }
  }

  bool resolve_canonname = job_key.flags & HOST_RESOLVER_CANONNAME;
  bool default_family_due_to_no_ipv6 =
      job_key.flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (GetHostname(job_key.host).empty() ||
      GetHostname(job_key.host).size() > kMaxHostLength) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  if (ip_address.IsValid())
    return ResolveAsIP(job_key.query_type, resolve_canonname, ip_address);

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  absl::optional<HostCache::Entry> resolved =
      ServeLocalhost(GetHostname(job_key.host), job_key.query_type,
                     default_family_due_to_no_ipv6);
  if (resolved)
    return resolved.value();

  // Do initial cache lookup.
  if (!out_tasks->empty() &&
      (out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP ||
       out_tasks->front() == TaskType::INSECURE_CACHE_LOOKUP ||
       out_tasks->front() == TaskType::CACHE_LOOKUP)) {
    bool secure = out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP;
    HostCache::Key key = job_key.ToCacheKey(secure);

    bool ignore_secure = false;
    if (out_tasks->front() == TaskType::CACHE_LOOKUP)
      ignore_secure = true;

    out_tasks->pop_front();

    resolved = MaybeServeFromCache(cache, key, cache_usage, ignore_secure,
                                   source_net_log, out_stale_info);
    if (resolved) {
      // |MaybeServeFromCache()| will update |*out_stale_info| as needed.
      DCHECK(out_stale_info->has_value());
      source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_CACHE_HIT,
                              [&] { return NetLogResults(resolved.value()); });

      return resolved.value();
    }
    DCHECK(!out_stale_info->has_value());
  }

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  resolved = ServeFromHosts(GetHostname(job_key.host), job_key.query_type,
                            default_family_due_to_no_ipv6, *out_tasks);
  if (resolved) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_HOSTS_HIT,
                            [&] { return NetLogResults(resolved.value()); });
    return resolved.value();
  }

  return HostCache::Entry(ERR_DNS_CACHE_MISS, HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverManager::CreateAndStartJob(JobKey key,
                                            std::deque<TaskType> tasks,
                                            RequestImpl* request) {
  DCHECK(!tasks.empty());

  auto jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    auto new_job = std::make_unique<Job>(
        weak_ptr_factory_.GetWeakPtr(), key, request->parameters().cache_usage,
        request->host_cache(), std::move(tasks), request->priority(),
        proc_task_runner_, request->source_net_log(), tick_clock_);
    job = new_job.get();
    auto insert_result = jobs_.emplace(std::move(key), std::move(new_job));
    DCHECK(insert_result.second);
    job->OnAddedToJobMap(insert_result.first);
    job->AddRequest(request);
    job->RunNextTask();
  } else {
    job = jobit->second.get();
    job->AddRequest(request);
  }
}

HostCache::Entry HostResolverManager::ResolveAsIP(DnsQueryType query_type,
                                                  bool resolve_canonname,
                                                  const IPAddress& ip_address) {
  DCHECK(ip_address.IsValid());

  // IP literals cannot resolve unless the query type is an address query that
  // allows addresses with the same address family as the literal. E.g., don't
  // return IPv6 addresses for IPv4 queries or anything for a non-address query.
  AddressFamily family = GetAddressFamily(ip_address);
  if (query_type != DnsQueryType::UNSPECIFIED &&
      query_type != AddressFamilyToDnsQueryType(family)) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  AddressList addresses = AddressList::CreateFromIPAddress(ip_address, 0);
  if (resolve_canonname)
    addresses.SetDefaultCanonicalName();
  return HostCache::Entry(OK, std::move(addresses),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

absl::optional<HostCache::Entry> HostResolverManager::MaybeServeFromCache(
    HostCache* cache,
    const HostCache::Key& key,
    ResolveHostParameters::CacheUsage cache_usage,
    bool ignore_secure,
    const NetLogWithSource& source_net_log,
    absl::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = absl::nullopt;

  if (!cache)
    return absl::nullopt;

  if (cache_usage == ResolveHostParameters::CacheUsage::DISALLOWED)
    return absl::nullopt;

  // Local-only requests search the cache for non-local-only results.
  HostCache::Key effective_key = key;
  if (effective_key.host_resolver_source == HostResolverSource::LOCAL_ONLY)
    effective_key.host_resolver_source = HostResolverSource::ANY;

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
  HostCache::EntryStaleness staleness;
  if (cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
    cache_result = cache->LookupStale(effective_key, tick_clock_->NowTicks(),
                                      &staleness, ignore_secure);
  } else {
    DCHECK(cache_usage == ResolveHostParameters::CacheUsage::ALLOWED);
    cache_result =
        cache->Lookup(effective_key, tick_clock_->NowTicks(), ignore_secure);
    staleness = HostCache::kNotStale;
  }
  if (cache_result) {
    *out_stale_info = std::move(staleness);
    source_net_log.AddEvent(
        NetLogEventType::HOST_RESOLVER_MANAGER_CACHE_HIT,
        [&] { return NetLogResults(cache_result->second); });
    return cache_result->second;
  }
  return absl::nullopt;
}

absl::optional<HostCache::Entry> HostResolverManager::ServeFromHosts(
    base::StringPiece hostname,
    DnsQueryType query_type,
    bool default_family_due_to_no_ipv6,
    const std::deque<TaskType>& tasks) {
  // Don't attempt a HOSTS lookup if there is no DnsConfig or the HOSTS lookup
  // is going to be done next as part of a system lookup.
  if (!dns_client_ || !IsAddressType(query_type) ||
      (!tasks.empty() && tasks.front() == TaskType::PROC))
    return absl::nullopt;
  const DnsHosts* hosts = dns_client_->GetHosts();

  if (!hosts || hosts->empty())
    return absl::nullopt;

  // HOSTS lookups are case-insensitive.
  std::string effective_hostname = base::ToLowerASCII(hostname);

  // If |address_family| is ADDRESS_FAMILY_UNSPECIFIED other implementations
  // (glibc and c-ares) return the first matching line. We have more
  // flexibility, but lose implicit ordering.
  // We prefer IPv6 because "happy eyeballs" will fall back to IPv4 if
  // necessary.
  AddressList addresses;
  if (query_type == DnsQueryType::AAAA ||
      query_type == DnsQueryType::UNSPECIFIED) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV6));
    if (it != hosts->end())
      addresses.push_back(IPEndPoint(it->second, 0));
  }

  if (query_type == DnsQueryType::A ||
      query_type == DnsQueryType::UNSPECIFIED) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV4));
    if (it != hosts->end())
      addresses.push_back(IPEndPoint(it->second, 0));
  }

  // If got only loopback addresses and the family was restricted, resolve
  // again, without restrictions. See SystemHostResolverCall for rationale.
  if (default_family_due_to_no_ipv6 && IsAllIPv4Loopback(addresses)) {
    return ServeFromHosts(hostname, DnsQueryType::UNSPECIFIED, false, tasks);
  }

  if (!addresses.empty()) {
    return HostCache::Entry(OK, std::move(addresses),
                            HostCache::Entry::SOURCE_HOSTS);
  }

  return absl::nullopt;
}

absl::optional<HostCache::Entry> HostResolverManager::ServeLocalhost(
    base::StringPiece hostname,
    DnsQueryType query_type,
    bool default_family_due_to_no_ipv6) {
  AddressList resolved_addresses;
  if (!IsAddressType(query_type) ||
      !ResolveLocalHostname(hostname, &resolved_addresses)) {
    return absl::nullopt;
  }

  AddressList filtered_addresses;
  for (const auto& address : resolved_addresses) {
    // Include the address if:
    // - caller didn't specify an address family, or
    // - caller specifically asked for the address family of this address, or
    // - this is an IPv6 address and caller specifically asked for IPv4 due
    //   to lack of detected IPv6 support. (See SystemHostResolverCall for
    //   rationale).
    if (query_type == DnsQueryType::UNSPECIFIED ||
        HostResolver::DnsQueryTypeToAddressFamily(query_type) ==
            address.GetFamily() ||
        (address.GetFamily() == ADDRESS_FAMILY_IPV6 &&
         query_type == DnsQueryType::A && default_family_due_to_no_ipv6)) {
      filtered_addresses.push_back(address);
    }
  }

  return HostCache::Entry(OK, std::move(filtered_addresses),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverManager::CacheResult(HostCache* cache,
                                      const HostCache::Key& key,
                                      const HostCache::Entry& entry,
                                      base::TimeDelta ttl) {
  // Don't cache an error unless it has a positive TTL.
  if (cache && (entry.error() == OK || ttl.is_positive()))
    cache->Set(key, entry, tick_clock_->NowTicks(), ttl);
}

std::unique_ptr<HostResolverManager::Job> HostResolverManager::RemoveJob(
    JobMap::iterator job_it) {
  DCHECK(job_it != jobs_.end());
  DCHECK(job_it->second);
  DCHECK_EQ(1u, jobs_.count(job_it->first));

  std::unique_ptr<Job> job;
  job_it->second.swap(job);
  jobs_.erase(job_it);
  job->OnRemovedFromJobMap();

  return job;
}

SecureDnsMode HostResolverManager::GetEffectiveSecureDnsMode(
    SecureDnsPolicy secure_dns_policy) {
  // Use switch() instead of if() to ensure that all policies are handled.
  switch (secure_dns_policy) {
    case SecureDnsPolicy::kDisable:
      return SecureDnsMode::kOff;
    case SecureDnsPolicy::kAllow:
      break;
  }

  const DnsConfig* config =
      dns_client_ ? dns_client_->GetEffectiveConfig() : nullptr;

  SecureDnsMode secure_dns_mode = SecureDnsMode::kOff;
  if (config) {
    secure_dns_mode = config->secure_dns_mode;
  }
  return secure_dns_mode;
}

bool HostResolverManager::HaveTestProcOverride() {
  return !proc_params_.resolver_proc && HostResolverProc::GetDefault();
}

void HostResolverManager::PushDnsTasks(bool proc_task_allowed,
                                       SecureDnsMode secure_dns_mode,
                                       bool insecure_tasks_allowed,
                                       bool allow_cache,
                                       bool prioritize_local_lookups,
                                       ResolveContext* resolve_context,
                                       std::deque<TaskType>* out_tasks) {
  DCHECK(dns_client_);
  DCHECK(dns_client_->GetEffectiveConfig());

  // If a catch-all DNS block has been set for unit tests, we shouldn't send
  // DnsTasks. It is still necessary to call this method, however, so that the
  // correct cache tasks for the secure dns mode are added.
  bool dns_tasks_allowed = !HaveTestProcOverride();
  // Upgrade the insecure DnsTask depending on the secure dns mode.
  switch (secure_dns_mode) {
    case SecureDnsMode::kSecure:
      DCHECK(!allow_cache ||
             out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP);
      DCHECK(dns_client_->CanUseSecureDnsTransactions());
      if (dns_tasks_allowed)
        out_tasks->push_back(TaskType::SECURE_DNS);
      break;
    case SecureDnsMode::kAutomatic:
      DCHECK(!allow_cache || out_tasks->front() == TaskType::CACHE_LOOKUP);
      if (dns_client_->FallbackFromSecureTransactionPreferred(
              resolve_context)) {
        // Don't run a secure DnsTask if there are no available DoH servers.
        if (dns_tasks_allowed && insecure_tasks_allowed)
          out_tasks->push_back(TaskType::DNS);
      } else if (prioritize_local_lookups) {
        // If local lookups are prioritized, the cache should be checked for
        // both secure and insecure results prior to running a secure DnsTask.
        // The task sequence should already contain the appropriate cache task.
        if (dns_tasks_allowed) {
          out_tasks->push_back(TaskType::SECURE_DNS);
          if (insecure_tasks_allowed)
            out_tasks->push_back(TaskType::DNS);
        }
      } else {
        if (allow_cache) {
          // Remove the initial cache lookup task so that the secure and
          // insecure lookups can be separated.
          out_tasks->pop_front();
          out_tasks->push_back(TaskType::SECURE_CACHE_LOOKUP);
        }
        if (dns_tasks_allowed)
          out_tasks->push_back(TaskType::SECURE_DNS);
        if (allow_cache)
          out_tasks->push_back(TaskType::INSECURE_CACHE_LOOKUP);
        if (dns_tasks_allowed && insecure_tasks_allowed)
          out_tasks->push_back(TaskType::DNS);
      }
      break;
    case SecureDnsMode::kOff:
      DCHECK(!allow_cache || out_tasks->front() == TaskType::CACHE_LOOKUP);
      if (dns_tasks_allowed && insecure_tasks_allowed)
        out_tasks->push_back(TaskType::DNS);
      break;
    default:
      NOTREACHED();
      break;
  }

  bool added_dns_task = false;
  for (auto it = out_tasks->begin(); it != out_tasks->end(); ++it) {
    if (*it == TaskType::DNS || *it == TaskType::SECURE_DNS) {
      added_dns_task = true;
      break;
    }
  }
  // The system resolver can be used as a fallback for a non-existent or
  // failing DnsTask if allowed by the request parameters.
  if (proc_task_allowed && (!added_dns_task || allow_fallback_to_proctask_))
    out_tasks->push_back(TaskType::PROC);
}

void HostResolverManager::CreateTaskSequence(
    const JobKey& job_key,
    ResolveHostParameters::CacheUsage cache_usage,
    std::deque<TaskType>* out_tasks) {
  DCHECK(out_tasks->empty());

  // A cache lookup should generally be performed first. For jobs involving a
  // DnsTask, this task may be replaced.
  bool allow_cache =
      cache_usage != ResolveHostParameters::CacheUsage::DISALLOWED;
  if (allow_cache) {
    if (job_key.secure_dns_mode == SecureDnsMode::kSecure) {
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
    } else {
      out_tasks->push_front(TaskType::CACHE_LOOKUP);
    }
  }

  // Determine what type of task a future Job should start.
  bool prioritize_local_lookups =
      cache_usage ==
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  switch (job_key.source) {
    case HostResolverSource::ANY:
      // Force address queries with canonname to use ProcTask to counter poor
      // CNAME support in DnsTask. See https://crbug.com/872665
      //
      // Otherwise, default to DnsTask (with allowed fallback to ProcTask for
      // address queries). But if hostname appears to be an MDNS name (ends in
      // *.local), go with ProcTask for address queries and MdnsTask for non-
      // address queries.
      if ((job_key.flags & HOST_RESOLVER_CANONNAME) &&
          IsAddressType(job_key.query_type)) {
        out_tasks->push_back(TaskType::PROC);
      } else if (!ResemblesMulticastDNSName(GetHostname(job_key.host))) {
        bool proc_task_allowed =
            IsAddressType(job_key.query_type) &&
            job_key.secure_dns_mode != SecureDnsMode::kSecure;
        if (dns_client_ && dns_client_->GetEffectiveConfig()) {
          bool insecure_allowed =
              dns_client_->CanUseInsecureDnsTransactions() &&
              !dns_client_->FallbackFromInsecureTransactionPreferred() &&
              (IsAddressType(job_key.query_type) ||
               dns_client_->CanQueryAdditionalTypesViaInsecureDns());
          PushDnsTasks(proc_task_allowed, job_key.secure_dns_mode,
                       insecure_allowed, allow_cache, prioritize_local_lookups,
                       &*job_key.resolve_context, out_tasks);
        } else if (proc_task_allowed) {
          out_tasks->push_back(TaskType::PROC);
        }
      } else if (IsAddressType(job_key.query_type)) {
        // For *.local address queries, try the system resolver even if the
        // secure dns mode is SECURE. Public recursive resolvers aren't expected
        // to handle these queries.
        out_tasks->push_back(TaskType::PROC);
      } else {
        out_tasks->push_back(TaskType::MDNS);
      }
      break;
    case HostResolverSource::SYSTEM:
      out_tasks->push_back(TaskType::PROC);
      break;
    case HostResolverSource::DNS:
      if (dns_client_ && dns_client_->GetEffectiveConfig()) {
        bool insecure_allowed =
            dns_client_->CanUseInsecureDnsTransactions() &&
            (IsAddressType(job_key.query_type) ||
             dns_client_->CanQueryAdditionalTypesViaInsecureDns());
        PushDnsTasks(false /* proc_task_allowed */, job_key.secure_dns_mode,
                     insecure_allowed, allow_cache, prioritize_local_lookups,
                     &*job_key.resolve_context, out_tasks);
      }
      break;
    case HostResolverSource::MULTICAST_DNS:
      out_tasks->push_back(TaskType::MDNS);
      break;
    case HostResolverSource::LOCAL_ONLY:
      // If no external source allowed, a job should not be created or started
      break;
  }
}

void HostResolverManager::GetEffectiveParametersForRequest(
    base::StringPiece hostname,
    DnsQueryType dns_query_type,
    HostResolverFlags flags,
    SecureDnsPolicy secure_dns_policy,
    ResolveHostParameters::CacheUsage cache_usage,
    bool is_ip,
    const NetLogWithSource& net_log,
    DnsQueryType* out_effective_type,
    HostResolverFlags* out_effective_flags,
    SecureDnsMode* out_effective_secure_dns_mode) {
  *out_effective_flags = flags | additional_resolver_flags_;
  *out_effective_type = dns_query_type;

  bool use_local_ipv6 = true;
  if (dns_client_) {
    const DnsConfig* config = dns_client_->GetEffectiveConfig();
    if (config)
      use_local_ipv6 = config->use_local_ipv6;
  }

  if (*out_effective_type == DnsQueryType::UNSPECIFIED &&
      // When resolving IPv4 literals, there's no need to probe for IPv6.
      // When resolving IPv6 literals, there's no benefit to artificially
      // limiting our resolution based on a probe.  Prior logic ensures
      // that this query is UNSPECIFIED (see effective_query_type check above)
      // so the code requesting the resolution should be amenable to receiving a
      // IPv6 resolution.
      !use_local_ipv6 && !is_ip && !IsIPv6Reachable(net_log)) {
    *out_effective_type = DnsQueryType::A;
    *out_effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  }

  *out_effective_secure_dns_mode = GetEffectiveSecureDnsMode(secure_dns_policy);
}

bool HostResolverManager::IsIPv6Reachable(const NetLogWithSource& net_log) {
  // Don't bother checking if the device is on WiFi and IPv6 is assumed to not
  // work on WiFi.
  if (!check_ipv6_on_wifi_ && NetworkChangeNotifier::GetConnectionType() ==
                                  NetworkChangeNotifier::CONNECTION_WIFI) {
    return false;
  }

  // Cache the result for kIPv6ProbePeriodMs (measured from after
  // IsGloballyReachable() completes).
  bool cached = true;
  if (last_ipv6_probe_time_.is_null() ||
      (tick_clock_->NowTicks() - last_ipv6_probe_time_).InMilliseconds() >
          kIPv6ProbePeriodMs) {
    SetLastIPv6ProbeResult(
        IsGloballyReachable(IPAddress(kIPv6ProbeAddress), net_log));
    cached = false;
  }
  net_log.AddEvent(
      NetLogEventType::HOST_RESOLVER_MANAGER_IPV6_REACHABILITY_CHECK, [&] {
        return NetLogIPv6AvailableParams(last_ipv6_probe_result_, cached);
      });
  return last_ipv6_probe_result_;
}

void HostResolverManager::SetLastIPv6ProbeResult(bool last_ipv6_probe_result) {
  last_ipv6_probe_result_ = last_ipv6_probe_result;
  last_ipv6_probe_time_ = tick_clock_->NowTicks();
}

bool HostResolverManager::IsGloballyReachable(const IPAddress& dest,
                                              const NetLogWithSource& net_log) {
  std::unique_ptr<DatagramClientSocket> socket(
      ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, net_log.net_log(), net_log.source()));
  int rv = socket->Connect(IPEndPoint(dest, 53));
  if (rv != OK)
    return false;
  IPEndPoint endpoint;
  rv = socket->GetLocalAddress(&endpoint);
  if (rv != OK)
    return false;
  DCHECK_EQ(ADDRESS_FAMILY_IPV6, endpoint.GetFamily());
  const IPAddress& address = endpoint.address();

  bool is_link_local =
      (address.bytes()[0] == 0xFE) && ((address.bytes()[1] & 0xC0) == 0x80);
  if (is_link_local)
    return false;

  const uint8_t kTeredoPrefix[] = {0x20, 0x01, 0, 0};
  if (IPAddressStartsWith(address, kTeredoPrefix))
    return false;

  return true;
}

void HostResolverManager::RunLoopbackProbeJob() {
  // Run this asynchronously as it can take 40-100ms and should not block
  // initialization.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HaveOnlyLoopbackAddresses),
      base::BindOnce(&HostResolverManager::SetHaveOnlyLoopbackAddresses,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HostResolverManager::RemoveAllJobs(const ResolveContext* context) {
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    const JobKey& key = it->first;
    if (&*key.resolve_context == context) {
      RemoveJob(it++);
    } else {
      ++it;
    }
  }
}

void HostResolverManager::AbortAllJobs(bool in_progress_only) {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from |jobs_|.
  std::vector<std::unique_ptr<Job>> jobs_to_abort;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    Job* job = it->second.get();
    if (!in_progress_only || job->is_running()) {
      jobs_to_abort.push_back(RemoveJob(it++));
    } else {
      ++it;
    }
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job in |jobs_to_abort| if the DnsConfig just became
  // invalid.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  // Then Abort them.
  for (size_t i = 0; self.get() && i < jobs_to_abort.size(); ++i) {
    jobs_to_abort[i]->Abort();
  }

  if (self)
    dispatcher_->SetLimits(limits);
}

void HostResolverManager::AbortInsecureDnsTasks(int error, bool fallback_only) {
  // Aborting jobs potentially modifies |jobs_| and may even delete some jobs.
  // Create safe closures of all current jobs.
  std::vector<base::OnceClosure> job_abort_closures;
  for (auto& job : jobs_) {
    job_abort_closures.push_back(
        job.second->GetAbortInsecureDnsTaskClosure(error, fallback_only));
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job if the DnsConfig just changed.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  for (base::OnceClosure& closure : job_abort_closures)
    std::move(closure).Run();

  dispatcher_->SetLimits(limits);
}

// TODO(crbug.com/995984): Consider removing this and its usage.
void HostResolverManager::TryServingAllJobsFromHosts() {
  if (!dns_client_ || !dns_client_->GetEffectiveConfig())
    return;

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  for (auto it = jobs_.begin(); self.get() && it != jobs_.end();) {
    Job* job = it->second.get();
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
  }
}

void HostResolverManager::OnIPAddressChanged() {
  last_ipv6_probe_time_ = base::TimeTicks();
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  InvalidateCaches();
#if (defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  AbortAllJobs(true /* in_progress_only */);
  // |this| may be deleted inside AbortAllJobs().
}

void HostResolverManager::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  proc_params_.unresponsive_delay =
      GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
          "DnsUnresponsiveDelayMsByConnectionType",
          ProcTaskParams::kDnsDefaultUnresponsiveDelay, type);

  // Note that NetworkChangeNotifier always sends a CONNECTION_NONE notification
  // before non-NONE notifications. This check therefore just ensures each
  // connection change notification is handled once and has nothing to do with
  // whether the change is to offline or online.
  if (type == NetworkChangeNotifier::CONNECTION_NONE && dns_client_) {
    dns_client_->ReplaceCurrentSession();
    InvalidateCaches(true /* network_change */);
  }
}

void HostResolverManager::OnSystemDnsConfigChanged(
    absl::optional<DnsConfig> config) {
  bool changed = false;
  bool transactions_allowed_before = false;
  if (dns_client_) {
    transactions_allowed_before = dns_client_->CanUseSecureDnsTransactions() ||
                                  dns_client_->CanUseInsecureDnsTransactions();
    changed = dns_client_->SetSystemConfig(std::move(config));
  }

  // Always invalidate cache, even if no change is seen.
  InvalidateCaches();

  if (changed) {
    // Need to update jobs iff transactions were previously allowed because
    // in-progress jobs may be running using a now-invalid configuration.
    if (transactions_allowed_before)
      UpdateJobsForChangedConfig();
  }
}

void HostResolverManager::UpdateJobsForChangedConfig() {
  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  // Existing jobs that were set up using the nameservers and secure dns mode
  // from the original config need to be aborted.
  AbortAllJobs(false /* in_progress_only */);

  // |this| may be deleted inside AbortAllJobs().
  if (self.get())
    TryServingAllJobsFromHosts();
}

void HostResolverManager::OnFallbackResolve(int dns_task_error) {
  DCHECK(dns_client_);
  DCHECK_NE(OK, dns_task_error);

  // Nothing to do if DnsTask is already not preferred.
  if (dns_client_->FallbackFromInsecureTransactionPreferred())
    return;

  dns_client_->IncrementInsecureFallbackFailures();

  // If DnsClient became not preferred, fallback all fallback-allowed insecure
  // DnsTasks to ProcTasks.
  if (dns_client_->FallbackFromInsecureTransactionPreferred())
    AbortInsecureDnsTasks(ERR_FAILED, true /* fallback_only */);
}

int HostResolverManager::GetOrCreateMdnsClient(MDnsClient** out_client) {
#if BUILDFLAG(ENABLE_MDNS)
  if (!mdns_client_) {
    if (!mdns_socket_factory_)
      mdns_socket_factory_ = std::make_unique<MDnsSocketFactoryImpl>(net_log_);
    mdns_client_ = MDnsClient::CreateDefault();
  }

  int rv = OK;
  if (!mdns_client_->IsListening())
    rv = mdns_client_->StartListening(mdns_socket_factory_.get());

  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(rv != OK || mdns_client_->IsListening());
  if (rv == OK)
    *out_client = mdns_client_.get();
  return rv;
#else
  // Should not request MDNS resoltuion unless MDNS is enabled.
  NOTREACHED();
  return ERR_UNEXPECTED;
#endif
}

void HostResolverManager::InvalidateCaches(bool network_change) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

#if DCHECK_IS_ON()
  base::WeakPtr<HostResolverManager> self_ptr = weak_ptr_factory_.GetWeakPtr();
  size_t num_jobs = jobs_.size();
#endif

  invalidation_in_progress_ = true;
  for (auto& context : registered_contexts_) {
    context.InvalidateCachesAndPerSessionData(
        dns_client_ ? dns_client_->GetCurrentSession() : nullptr,
        network_change);
  }
  invalidation_in_progress_ = false;

#if DCHECK_IS_ON()
  // Sanity checks that invalidation does not have reentrancy issues.
  DCHECK(self_ptr);
  DCHECK_EQ(num_jobs, jobs_.size());
#endif
}

std::unique_ptr<DnsProbeRunner> HostResolverManager::CreateDohProbeRunner(
    ResolveContext* resolve_context) {
  DCHECK(resolve_context);
  DCHECK(registered_contexts_.HasObserver(resolve_context));
  if (!dns_client_->CanUseSecureDnsTransactions())
    return nullptr;

  return dns_client_->GetTransactionFactory()->CreateDohProbeRunner(
      resolve_context);
}

HostResolverManager::RequestImpl::~RequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_)
    return;

  job_->CancelRequest(this);
  LogCancelRequest();
}

void HostResolverManager::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(job_);
  job_->ChangeRequestPriority(this, priority);
}

}  // namespace net

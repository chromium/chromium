// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager.h"

#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/debug/debugger.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/identity.h"
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
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
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
#include "net/base/network_anonymization_key.h"
#include "net/base/network_interfaces.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_mdns_listener_impl.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_nat64_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/public/util.h"
#include "net/dns/record_parsed.h"
#include "net/dns/resolve_context.h"
#include "net/dns/test_dns_config_service.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <Winsock2.h>
#include "net/base/winsock_init.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <net/if.h>
#include "net/base/sys_addrinfo.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_library.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include <ifaddrs.h>
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

namespace net {

namespace {

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Default TTL for successful resolutions with HostResolverSystemTask.
const unsigned kCacheEntryTTLSeconds = 60;

// Default TTL for unsuccessful resolutions with HostResolverSystemTask.
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

bool ContainsIcannNameCollisionIp(const std::vector<IPEndPoint>& endpoints) {
  for (const auto& endpoint : endpoints) {
    const IPAddress& addr = endpoint.address();
    if (addr.IsIPv4() && IPAddressStartsWith(addr, kIcanNameCollisionIp)) {
      return true;
    }
  }
  return false;
}

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(base::StringPiece hostname) {
  return base::EndsWith(hostname, ".local") ||
         base::EndsWith(hostname, ".local.");
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

// Returns true if NAT64 can be used in place of an IPv4 address during host
// resolution.
bool MayUseNAT64ForIPv4Literal(HostResolverFlags flags,
                               HostResolverSource source,
                               const IPAddress& ip_address) {
  return !(flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) &&
         ip_address.IsValid() && ip_address.IsIPv4() &&
         base::FeatureList::IsEnabled(features::kUseNAT64ForIPv4Literal) &&
         (source != HostResolverSource::LOCAL_ONLY);
}

//-----------------------------------------------------------------------------

// Returns true if it can determine that only loopback addresses are configured.
// i.e. if only 127.0.0.1 and ::1 are routable.
// Also returns false if it cannot determine this.
bool HaveOnlyLoopbackAddresses() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
#if BUILDFLAG(IS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#elif BUILDFLAG(IS_ANDROID)
  return android::HaveOnlyLoopbackAddresses();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  struct ifaddrs* interface_addr = nullptr;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVPLOG(1) << "getifaddrs() failed";
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr; interface != nullptr;
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

// Creates NetLog parameters when the DnsTask failed.
base::Value::Dict NetLogDnsTaskFailedParams(
    int net_error,
    absl::optional<DnsQueryType> failed_transaction_type,
    absl::optional<base::TimeDelta> ttl,
    const HostCache::Entry* saved_results) {
  base::Value::Dict dict;
  if (failed_transaction_type) {
    dict.Set("dns_query_type", kDnsQueryTypes.at(*failed_transaction_type));
  }
  if (ttl)
    dict.Set("error_ttl_sec",
             base::saturated_cast<int>(ttl.value().InSeconds()));
  dict.Set("net_error", net_error);
  if (saved_results)
    dict.Set("saved_results", saved_results->NetLogParams());
  return dict;
}

base::Value::Dict NetLogDnsTaskExtractionFailureParams(
    DnsResponseResultExtractor::ExtractionError extraction_error,
    DnsQueryType dns_query_type,
    const HostCache::Entry& results) {
  base::Value::Dict dict;
  dict.Set("extraction_error", base::strict_cast<int>(extraction_error));
  dict.Set("dns_query_type", kDnsQueryTypes.at(dns_query_type));
  dict.Set("results", results.NetLogParams());
  return dict;
}

// Creates NetLog parameters for HOST_RESOLVER_MANAGER_JOB_ATTACH/DETACH events.
base::Value::Dict NetLogJobAttachParams(const NetLogSource& source,
                                        RequestPriority priority) {
  base::Value::Dict dict;
  source.AddToEventParameters(dict);
  dict.Set("priority", RequestPriorityToString(priority));
  return dict;
}

base::Value::Dict NetLogIPv6AvailableParams(bool ipv6_available, bool cached) {
  base::Value::Dict dict;
  dict.Set("ipv6_available", ipv6_available);
  dict.Set("cached", cached);
  return dict;
}

// The logging routines are defined here because some requests are resolved
// without a Request object.

//-----------------------------------------------------------------------------

// Maximum of 64 concurrent resolver calls (excluding retries).
// Between 2010 and 2020, the limit was set to 6 because of a report of a broken
// home router that would fail in the presence of more simultaneous queries.
// In 2020, we conducted an experiment to see if this kind of router was still
// present on the Internet, and found no evidence of any remaining issues, so
// we increased the limit to 64 at that time.
const size_t kDefaultMaxSystemTasks = 64u;

PrioritizedDispatcher::Limits GetDispatcherLimits(
    const HostResolver::ManagerOptions& options) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES,
                                       options.max_concurrent_resolves);

  // If not using default, do not use the field trial.
  if (limits.total_jobs != HostResolver::ManagerOptions::kDefaultParallelism)
    return limits;

  // Default, without trial is no reserved slots.
  limits.total_jobs = kDefaultMaxSystemTasks;

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
  for (size_t i = 0; i < group_parts.size(); ++i) {
    if (!base::StringToSizeT(group_parts[i], &parsed[i])) {
      NOTREACHED();
      return limits;
    }
  }

  const size_t total_jobs = parsed.back();
  parsed.pop_back();

  const size_t total_reserved_slots =
      std::accumulate(parsed.begin(), parsed.end(), 0u);

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
      : highest_priority_(initial_priority) {}

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
  size_t total_count_ = 0;
  size_t counts_[NUM_PRIORITIES] = {};
};

base::Value::Dict NetLogResults(const HostCache::Entry& results) {
  base::Value::Dict dict;
  dict.Set("results", results.NetLogParams());
  return dict;
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

// Only use scheme/port in JobKey if `https_svcb_options_enabled` is true
// (or the query is explicitly for HTTPS). Otherwise DNS will not give different
// results for the same hostname.
absl::variant<url::SchemeHostPort, std::string> CreateHostForJobKey(
    const HostResolver::Host& input,
    DnsQueryType query_type,
    bool https_svcb_options_enabled) {
  if ((https_svcb_options_enabled || query_type == DnsQueryType::HTTPS) &&
      input.HasScheme()) {
    return input.AsSchemeHostPort();
  }

  return std::string(input.GetHostnameWithoutBrackets());
}

DnsResponse CreateFakeEmptyResponse(base::StringPiece hostname,
                                    DnsQueryType query_type) {
  absl::optional<std::vector<uint8_t>> qname =
      dns_names_util::DottedNameToNetwork(
          hostname, /*require_valid_internet_hostname=*/true);
  CHECK(qname.has_value());
  return DnsResponse::CreateEmptyNoDataResponse(
      /*id=*/0u, /*is_authoritative=*/true, qname.value(),
      DnsQueryTypeToQtype(query_type));
}

std::vector<IPEndPoint> FilterAddresses(std::vector<IPEndPoint> addresses,
                                        DnsQueryTypeSet query_types) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));
  DCHECK(!query_types.Empty());

  const AddressFamily want_family =
      HostResolver::DnsQueryTypeSetToAddressFamily(query_types);

  if (want_family == ADDRESS_FAMILY_UNSPECIFIED)
    return addresses;

  // Keep only the endpoints that match `want_family`.
  addresses.erase(
      base::ranges::remove_if(
          addresses,
          [want_family](AddressFamily family) { return family != want_family; },
          &IPEndPoint::GetFamily),
      addresses.end());
  return addresses;
}

void RecordResolveTimeDiffForBucket(const char* histogram_variant,
                                    const char* histogram_bucket,
                                    base::TimeDelta diff) {
  base::UmaHistogramTimes(
      base::StrCat({"Net.Dns.ResolveTimeDiff.", histogram_variant,
                    ".FirstRecord", histogram_bucket}),
      diff);
}

void RecordResolveTimeDiff(const char* histogram_variant,
                           base::TimeTicks start_time,
                           base::TimeTicks first_record_end_time,
                           base::TimeTicks second_record_end_time) {
  CHECK_LE(start_time, first_record_end_time);
  CHECK_LE(first_record_end_time, second_record_end_time);
  base::TimeDelta first_elapsed = first_record_end_time - start_time;
  base::TimeDelta diff = second_record_end_time - first_record_end_time;

  if (first_elapsed < base::Milliseconds(10)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "FasterThan10ms", diff);
  } else if (first_elapsed < base::Milliseconds(25)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "10msTo25ms", diff);
  } else if (first_elapsed < base::Milliseconds(50)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "25msTo50ms", diff);
  } else if (first_elapsed < base::Milliseconds(100)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "50msTo100ms", diff);
  } else if (first_elapsed < base::Milliseconds(250)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "100msTo250ms", diff);
  } else if (first_elapsed < base::Milliseconds(500)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "250msTo500ms", diff);
  } else if (first_elapsed < base::Seconds(1)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "500msTo1s", diff);
  } else {
    RecordResolveTimeDiffForBucket(histogram_variant, "SlowerThan1s", diff);
  }
}

}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(base::StringPiece host,
                          std::vector<IPEndPoint>* address_list) {
  address_list->clear();
  if (!IsLocalHostname(host))
    return false;

  address_list->emplace_back(IPAddress::IPv6Localhost(), 0);
  address_list->emplace_back(IPAddress::IPv4Localhost(), 0);

  return true;
}

struct HostResolverManager::JobKey {
  explicit JobKey(ResolveContext* resolve_context)
      : resolve_context(resolve_context->AsSafeRef()) {}

  bool operator<(const JobKey& other) const {
    return std::forward_as_tuple(query_types.ToEnumBitmask(), flags, source,
                                 secure_dns_mode, &*resolve_context, host,
                                 network_anonymization_key) <
           std::forward_as_tuple(other.query_types.ToEnumBitmask(), other.flags,
                                 other.source, other.secure_dns_mode,
                                 &*other.resolve_context, other.host,
                                 other.network_anonymization_key);
  }

  bool operator==(const JobKey& other) const {
    return !(*this < other || other < *this);
  }

  absl::variant<url::SchemeHostPort, std::string> host;
  NetworkAnonymizationKey network_anonymization_key;
  DnsQueryTypeSet query_types;
  HostResolverFlags flags;
  HostResolverSource source;
  SecureDnsMode secure_dns_mode;
  base::SafeRef<ResolveContext> resolve_context;

  HostCache::Key ToCacheKey(bool secure) const {
    if (query_types.Size() != 1) {
      // This function will produce identical cache keys for `JobKey` structs
      // that differ only in their (non-singleton) `query_types` fields. When we
      // enable new query types, this behavior could lead to subtle bugs. That
      // is why the following DCHECK restricts the allowable query types.
      DCHECK(Difference(query_types,
                        DnsQueryTypeSet(DnsQueryType::A, DnsQueryType::AAAA,
                                        DnsQueryType::HTTPS))
                 .Empty());
    }
    const DnsQueryType query_type_for_key = query_types.Size() == 1
                                                ? *query_types.begin()
                                                : DnsQueryType::UNSPECIFIED;
    HostCache::Key key(host, query_type_for_key, flags, source,
                       network_anonymization_key);
    key.secure = secure;
    return key;
  }

  handles::NetworkHandle GetTargetNetwork() const {
    return resolve_context->GetTargetNetwork();
  }
};

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
              HostResolver::Host request_host,
              NetworkAnonymizationKey network_anonymization_key,
              absl::optional<ResolveHostParameters> optional_parameters,
              base::WeakPtr<ResolveContext> resolve_context,
              HostCache* host_cache,
              base::WeakPtr<HostResolverManager> resolver,
              const base::TickClock* tick_clock)
      : source_net_log_(std::move(source_net_log)),
        request_host_(std::move(request_host)),
        network_anonymization_key_(
            NetworkAnonymizationKey::IsPartitioningEnabled()
                ? std::move(network_anonymization_key)
                : NetworkAnonymizationKey()),
        parameters_(optional_parameters ? std::move(optional_parameters).value()
                                        : ResolveHostParameters()),
        resolve_context_(std::move(resolve_context)),
        host_cache_(host_cache),
        host_resolver_flags_(
            HostResolver::ParametersToHostResolverFlags(parameters_)),
        priority_(parameters_.initial_priority),
        job_key_(JobKey(resolve_context_.get())),
        resolver_(std::move(resolver)),
        tick_clock_(tick_clock) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override;

  int Start(CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);
    // Start() may only be called once per request.
    CHECK(!job_.has_value());
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    if (!resolve_context_) {
      complete_ = true;
      resolver_.reset();
      set_error_info(ERR_CONTEXT_SHUT_DOWN, false);
      return ERR_NAME_NOT_RESOLVED;
    }

    LogStartRequest();

    next_state_ = STATE_IPV6_REACHABILITY;
    callback_ = std::move(callback);

    int rv = OK;
    rv = DoLoop(rv);
    return rv;
  }

  int DoLoop(int rv) {
    do {
      ResolveState state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_IPV6_REACHABILITY:
          rv = DoIPv6Reachability();
          break;
        case STATE_GET_PARAMETERS:
          DCHECK_EQ(OK, rv);
          rv = DoGetParameters();
          break;
        case STATE_GET_PARAMETERS_COMPLETE:
          rv = DoGetParametersComplete(rv);
          break;
        case STATE_RESOLVE_LOCALLY:
          rv = DoResolveLocally();
          break;
        case STATE_START_JOB:
          rv = DoStartJob();
          break;
        case STATE_FINISH_REQUEST:
          rv = DoFinishRequest(rv);
          break;
        default:
          NOTREACHED() << "next_state_: " << next_state_;
          break;
      }
    } while (next_state_ != STATE_NONE && rv != ERR_IO_PENDING);

    return rv;
  }

  void OnIOComplete(int rv) {
    rv = DoLoop(rv);
    if (rv != ERR_IO_PENDING && !callback_.is_null()) {
      std::move(callback_).Run(rv);
    }
  }

  int DoIPv6Reachability() {
    next_state_ = STATE_GET_PARAMETERS;
    // If a single reachability probe has not been completed, and the latest
    // probe will return asynchronously, return ERR_NAME_NOT_RESOLVED when the
    // request source is LOCAL_ONLY. This is due to LOCAL_ONLY requiring a
    // synchronous response, so it cannot wait on an async probe result and
    // cannot make assumptions about reachability.
    if (parameters_.source == HostResolverSource::LOCAL_ONLY) {
      int rv = resolver_->StartIPv6ReachabilityCheck(
          source_net_log_, base::DoNothingAs<void(int)>());
      if (rv == ERR_IO_PENDING) {
        next_state_ = STATE_FINISH_REQUEST;
        return ERR_NAME_NOT_RESOLVED;
      }
      return OK;
    }
    return resolver_->StartIPv6ReachabilityCheck(
        source_net_log_, base::BindOnce(&RequestImpl::OnIOComplete,
                                        weak_ptr_factory_.GetWeakPtr()));
  }

  int DoGetParameters() {
    job_key_.host =
        CreateHostForJobKey(request_host_, parameters_.dns_query_type,
                            resolver_->https_svcb_options_.enable);
    job_key_.network_anonymization_key = network_anonymization_key_;
    job_key_.source = parameters_.source;

    bool is_ip = ip_address_.AssignFromIPLiteral(GetHostname(job_key_.host));

    resolver_->GetEffectiveParametersForRequest(
        job_key_.host, parameters_.dns_query_type, host_resolver_flags_,
        parameters_.secure_dns_policy, is_ip, source_net_log_,
        &job_key_.query_types, &job_key_.flags, &job_key_.secure_dns_mode);

    // A reachability probe to determine if the network is only reachable on
    // IPv6 will be scheduled if the parameters are met for using NAT64 in place
    // of an IPv4 address.
    if (MayUseNAT64ForIPv4Literal(job_key_.flags, parameters_.source,
                                  ip_address_) &&
        resolver_->last_ipv6_probe_result_) {
      next_state_ = STATE_GET_PARAMETERS_COMPLETE;
      return resolver_->StartGloballyReachableCheck(
          ip_address_, source_net_log_,
          base::BindOnce(&RequestImpl::OnIOComplete,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    next_state_ = STATE_RESOLVE_LOCALLY;
    return OK;
  }

  int DoGetParametersComplete(int rv) {
    next_state_ = STATE_RESOLVE_LOCALLY;
    only_ipv6_reachable_ = (rv == ERR_FAILED) ? true : false;
    return OK;
  }

  int DoResolveLocally() {
    absl::optional<HostCache::EntryStaleness> stale_info;
    HostCache::Entry results = resolver_->ResolveLocally(
        only_ipv6_reachable_, job_key_, ip_address_, parameters_.cache_usage,
        parameters_.secure_dns_policy, parameters_.source, source_net_log_,
        host_cache_, &tasks_, &stale_info);
    if (results.error() != ERR_DNS_CACHE_MISS ||
        parameters_.source == HostResolverSource::LOCAL_ONLY ||
        tasks_.empty()) {
      if (results.error() == OK && !parameters_.is_speculative) {
        set_results(results.CopyWithDefaultPort(request_host_.GetPort()));
      }
      if (stale_info && !parameters_.is_speculative) {
        set_stale_info(std::move(stale_info).value());
      }
      next_state_ = STATE_FINISH_REQUEST;
      return results.error();
    }
    next_state_ = STATE_START_JOB;
    return OK;
  }

  int DoStartJob() {
    resolver_->CreateAndStartJob(std::move(job_key_), std::move(tasks_), this);
    DCHECK(!complete_);
    resolver_.reset();
    return ERR_IO_PENDING;
  }

  int DoFinishRequest(int rv) {
    CHECK(!job_.has_value());
    complete_ = true;
    set_error_info(rv, /*is_secure_network_error=*/false);
    rv = HostResolver::SquashErrorCode(rv);
    LogFinishRequest(rv, /*async_completion=*/false);
    return rv;
  }

  const AddressList* GetAddressResults() const override {
    DCHECK(complete_);
    return base::OptionalToPtr(legacy_address_results_);
  }

  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override {
    DCHECK(complete_);
    return base::OptionalToPtr(endpoint_results_);
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

  const std::set<std::string>* GetDnsAliasResults() const override {
    DCHECK(complete_);

    // If `include_canonical_name` param was true, should only ever have at most
    // a single alias, representing the expected "canonical name".
#if DCHECK_IS_ON()
    if (parameters().include_canonical_name && fixed_up_dns_alias_results_) {
      DCHECK_LE(fixed_up_dns_alias_results_->size(), 1u);
      if (GetAddressResults()) {
        std::set<std::string> address_list_aliases_set(
            GetAddressResults()->dns_aliases().begin(),
            GetAddressResults()->dns_aliases().end());
        DCHECK(address_list_aliases_set == fixed_up_dns_alias_results_.value());
      }
    }
#endif  // DCHECK_IS_ON()

    return base::OptionalToPtr(fixed_up_dns_alias_results_);
  }

  const std::vector<bool>* GetExperimentalResultsForTesting() const override {
    DCHECK(complete_);
    return results_ ? results_.value().https_record_compatibility() : nullptr;
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
    FixUpEndpointAndAliasResults();
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

  void AssignJob(base::SafeRef<Job> job) {
    CHECK(!job_.has_value());
    job_ = std::move(job);
  }

  bool HasJob() const { return job_.has_value(); }

  // Gets the Job's key. Crashes if no Job has been assigned.
  const JobKey& GetJobKey() const;

  // Unassigns the Job without calling completion callback.
  void OnJobCancelled(const JobKey& key);

  // Cleans up Job assignment, marks request completed, and calls the completion
  // callback. |is_secure_network_error| indicates whether |error| came from a
  // secure DNS lookup.
  void OnJobCompleted(const JobKey& job_key,
                      int error,
                      bool is_secure_network_error);

  // NetLog for the source, passed in HostResolver::Resolve.
  const NetLogWithSource& source_net_log() { return source_net_log_; }

  const HostResolver::Host& request_host() const { return request_host_; }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  ResolveContext* resolve_context() const { return resolve_context_.get(); }

  HostCache* host_cache() const { return host_cache_; }

  HostResolverFlags host_resolver_flags() const { return host_resolver_flags_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  bool complete() const { return complete_; }

 private:
  enum ResolveState {
    STATE_IPV6_REACHABILITY,
    STATE_GET_PARAMETERS,
    STATE_GET_PARAMETERS_COMPLETE,
    STATE_RESOLVE_LOCALLY,
    STATE_START_JOB,
    STATE_FINISH_REQUEST,
    STATE_NONE,
  };

  void FixUpEndpointAndAliasResults() {
    DCHECK(results_.has_value());
    DCHECK(!legacy_address_results_.has_value());
    DCHECK(!endpoint_results_.has_value());
    DCHECK(!fixed_up_dns_alias_results_.has_value());

    endpoint_results_ = results_.value().GetEndpoints();
    if (endpoint_results_.has_value()) {
      DCHECK(results_.value().aliases());
      fixed_up_dns_alias_results_ = *results_.value().aliases();

      // Skip fixups for `include_canonical_name` requests. Just use the
      // canonical name exactly as it was received from the system resolver.
      if (parameters().include_canonical_name) {
        DCHECK_LE(fixed_up_dns_alias_results_.value().size(), 1u);
      } else {
        fixed_up_dns_alias_results_ = dns_alias_utility::FixUpDnsAliases(
            fixed_up_dns_alias_results_.value());
      }

      legacy_address_results_ = HostResolver::EndpointResultToAddressList(
          endpoint_results_.value(), fixed_up_dns_alias_results_.value());
    }
  }

  // Logging and metrics for when a request has just been started.
  void LogStartRequest() {
    DCHECK(request_time_.is_null());
    request_time_ = tick_clock_->NowTicks();

    source_net_log_.BeginEvent(
        NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST, [this] {
          base::Value::Dict dict;
          dict.Set("host", request_host_.ToString());
          dict.Set("dns_query_type",
                   kDnsQueryTypes.at(parameters_.dns_query_type));
          dict.Set("allow_cached_response",
                   parameters_.cache_usage !=
                       ResolveHostParameters::CacheUsage::DISALLOWED);
          dict.Set("is_speculative", parameters_.is_speculative);
          dict.Set("network_anonymization_key",
                   network_anonymization_key_.ToDebugString());
          dict.Set("secure_dns_policy",
                   base::strict_cast<int>(parameters_.secure_dns_policy));
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

  const HostResolver::Host request_host_;
  const NetworkAnonymizationKey network_anonymization_key_;
  ResolveHostParameters parameters_;
  base::WeakPtr<ResolveContext> resolve_context_;
  const raw_ptr<HostCache, DanglingUntriaged> host_cache_;
  const HostResolverFlags host_resolver_flags_;

  RequestPriority priority_;

  ResolveState next_state_;
  JobKey job_key_;
  IPAddress ip_address_;

  std::deque<TaskType> tasks_;
  // The resolve job that this request is dependent on.
  absl::optional<base::SafeRef<Job>> job_;
  base::WeakPtr<HostResolverManager> resolver_ = nullptr;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_ = false;
  bool only_ipv6_reachable_ = false;
  absl::optional<HostCache::Entry> results_;
  absl::optional<HostCache::EntryStaleness> stale_info_;
  absl::optional<AddressList> legacy_address_results_;
  absl::optional<std::vector<HostResolverEndpointResult>> endpoint_results_;
  absl::optional<std::set<std::string>> fixed_up_dns_alias_results_;
  ResolveErrorInfo error_info_;

  const raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks request_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
                                   bool allow_fallback,
                                   HostCache::Entry results,
                                   bool secure) = 0;

    // Called when one or more transactions complete or get cancelled, but only
    // if more transactions are needed. If no more transactions are needed,
    // expect `OnDnsTaskComplete()` to be called instead.
    virtual void OnIntermediateTransactionsComplete() = 0;

    virtual RequestPriority priority() const = 0;

    virtual void AddTransactionTimeQueued(base::TimeDelta time_queued) = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  DnsTask(DnsClient* client,
          absl::variant<url::SchemeHostPort, std::string> host,
          DnsQueryTypeSet query_types,
          ResolveContext* resolve_context,
          bool secure,
          SecureDnsMode secure_dns_mode,
          Delegate* delegate,
          const NetLogWithSource& job_net_log,
          const base::TickClock* tick_clock,
          bool fallback_available,
          const HostResolver::HttpsSvcbOptions& https_svcb_options)
      : client_(client),
        host_(std::move(host)),
        resolve_context_(resolve_context->AsSafeRef()),
        secure_(secure),
        secure_dns_mode_(secure_dns_mode),
        delegate_(delegate),
        net_log_(job_net_log),
        tick_clock_(tick_clock),
        task_start_time_(tick_clock_->NowTicks()),
        fallback_available_(fallback_available),
        https_svcb_options_(https_svcb_options) {
    DCHECK(client_);
    DCHECK(delegate_);

    if (!secure_) {
      DCHECK(client_->CanUseInsecureDnsTransactions());
    }

    PushTransactionsNeeded(MaybeDisableAdditionalQueries(query_types));
  }

  DnsTask(const DnsTask&) = delete;
  DnsTask& operator=(const DnsTask&) = delete;

  int num_additional_transactions_needed() const {
    return base::checked_cast<int>(transactions_needed_.size());
  }

  int num_transactions_in_progress() const {
    return base::checked_cast<int>(transactions_in_progress_.size());
  }

  bool secure() const { return secure_; }

  void StartNextTransaction() {
    DCHECK_GE(num_additional_transactions_needed(), 1);

    if (!any_transaction_started_) {
      net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK,
                          [&] { return NetLogDnsTaskCreationParams(); });
    }
    any_transaction_started_ = true;

    TransactionInfo transaction_info = std::move(transactions_needed_.front());
    transactions_needed_.pop_front();

    DCHECK(IsAddressType(transaction_info.type) || secure_ ||
           client_->CanQueryAdditionalTypesViaInsecureDns());

    // Record how long this transaction has been waiting to be created.
    base::TimeDelta time_queued = tick_clock_->NowTicks() - task_start_time_;
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.PerTransaction",
                                 time_queued);
    delegate_->AddTransactionTimeQueued(time_queued);

    CreateAndStartTransaction(std::move(transaction_info));
  }

 private:
  enum class TransactionErrorBehavior {
    // Errors lead to task fallback (immediately unless another pending/started
    // transaction has the `kFatalOrEmpty` behavior).
    kFallback,

    // Transaction errors are treated as if a NOERROR response were received,
    // allowing task success if other transactions complete successfully.
    kSynthesizeEmpty,

    // Transaction errors are potentially fatal (determined by
    // `OnTransactionComplete` and often its helper
    // `IsFatalTransactionFailure()`) for the entire Job and may disallow
    // fallback. Otherwise, same as `kSynthesizeEmpty`.
    // TODO(crbug.com/1264933): Implement the fatality behavior.
    kFatalOrEmpty,
  };

  struct TransactionInfo {
    explicit TransactionInfo(DnsQueryType type,
                             TransactionErrorBehavior error_behavior =
                                 TransactionErrorBehavior::kFallback)
        : type(type), error_behavior(error_behavior) {}
    TransactionInfo(TransactionInfo&&) = default;
    TransactionInfo& operator=(TransactionInfo&&) = default;

    bool operator<(const TransactionInfo& other) const {
      return std::tie(type, error_behavior, transaction) <
             std::tie(other.type, other.error_behavior, other.transaction);
    }

    DnsQueryType type;
    TransactionErrorBehavior error_behavior;
    std::unique_ptr<DnsTransaction> transaction;
  };

  base::Value::Dict NetLogDnsTaskCreationParams() {
    base::Value::Dict dict;
    dict.Set("secure", secure());

    base::Value::List transactions_needed_value;
    for (const TransactionInfo& info : transactions_needed_) {
      base::Value::Dict transaction_dict;
      transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
      transactions_needed_value.Append(std::move(transaction_dict));
    }
    dict.Set("transactions_needed", std::move(transactions_needed_value));

    return dict;
  }

  base::Value::Dict NetLogDnsTaskTimeoutParams() {
    base::Value::Dict dict;

    if (!transactions_in_progress_.empty()) {
      base::Value::List list;
      for (const TransactionInfo& info : transactions_in_progress_) {
        base::Value::Dict transaction_dict;
        transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
        list.Append(std::move(transaction_dict));
      }
      dict.Set("started_transactions", std::move(list));
    }

    if (!transactions_needed_.empty()) {
      base::Value::List list;
      for (const TransactionInfo& info : transactions_needed_) {
        base::Value::Dict transaction_dict;
        transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
        list.Append(std::move(transaction_dict));
      }
      dict.Set("queued_transactions", std::move(list));
    }

    return dict;
  }

  DnsQueryTypeSet MaybeDisableAdditionalQueries(DnsQueryTypeSet types) {
    DCHECK(!types.Empty());
    DCHECK(!types.Has(DnsQueryType::UNSPECIFIED));

    // No-op if the caller explicitly requested this one query type.
    if (types.Size() == 1)
      return types;

    if (types.Has(DnsQueryType::HTTPS)) {
      if (!secure_ && !client_->CanQueryAdditionalTypesViaInsecureDns()) {
        types.Remove(DnsQueryType::HTTPS);
      } else {
        DCHECK(!httpssvc_metrics_);
        httpssvc_metrics_.emplace(secure_);
      }
    }
    DCHECK(!types.Empty());
    return types;
  }

  void PushTransactionsNeeded(DnsQueryTypeSet query_types) {
    DCHECK(transactions_needed_.empty());

    if (query_types.Has(DnsQueryType::HTTPS) &&
        features::kUseDnsHttpsSvcbEnforceSecureResponse.Get() && secure_) {
      query_types.Remove(DnsQueryType::HTTPS);
      transactions_needed_.emplace_back(
          DnsQueryType::HTTPS, TransactionErrorBehavior::kFatalOrEmpty);
    }

    // Give AAAA/A queries a head start by pushing them to the queue first.
    constexpr DnsQueryType kHighPriorityQueries[] = {DnsQueryType::AAAA,
                                                     DnsQueryType::A};
    for (DnsQueryType high_priority_query : kHighPriorityQueries) {
      if (query_types.Has(high_priority_query)) {
        query_types.Remove(high_priority_query);
        transactions_needed_.emplace_back(high_priority_query);
      }
    }
    for (DnsQueryType remaining_query : query_types) {
      if (remaining_query == DnsQueryType::HTTPS) {
        // Ignore errors for these types. In most cases treating them normally
        // would only result in fallback to resolution without querying the
        // type. Instead, synthesize empty results.
        transactions_needed_.emplace_back(
            remaining_query, TransactionErrorBehavior::kSynthesizeEmpty);
      } else {
        transactions_needed_.emplace_back(remaining_query);
      }
    }
  }

  void CreateAndStartTransaction(TransactionInfo transaction_info) {
    DCHECK(!transaction_info.transaction);
    DCHECK_NE(DnsQueryType::UNSPECIFIED, transaction_info.type);

    std::string transaction_hostname(GetHostname(host_));

    // For HTTPS, prepend "_<port>._https." for any non-default port.
    uint16_t request_port = 0;
    if (transaction_info.type == DnsQueryType::HTTPS &&
        absl::holds_alternative<url::SchemeHostPort>(host_)) {
      const auto& scheme_host_port = absl::get<url::SchemeHostPort>(host_);
      transaction_hostname =
          dns_util::GetNameForHttpsQuery(scheme_host_port, &request_port);
    }

    transaction_info.transaction =
        client_->GetTransactionFactory()->CreateTransaction(
            std::move(transaction_hostname),
            DnsQueryTypeToQtype(transaction_info.type), net_log_, secure_,
            secure_dns_mode_, &*resolve_context_,
            fallback_available_ /* fast_timeout */);
    transaction_info.transaction->SetRequestPriority(delegate_->priority());

    auto transaction_info_it =
        transactions_in_progress_.insert(std::move(transaction_info)).first;

    // Safe to pass `transaction_info_it` because it is only modified/removed
    // after async completion of this call or by destruction (which cancels the
    // transaction and prevents callback because it owns the `DnsTransaction`
    // object).
    transaction_info_it->transaction->Start(base::BindOnce(
        &DnsTask::OnDnsTransactionComplete, base::Unretained(this),
        transaction_info_it, request_port));
  }

  void OnTimeout() {
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK_TIMEOUT,
                      [&] { return NetLogDnsTaskTimeoutParams(); });

    for (const TransactionInfo& transaction : transactions_in_progress_) {
      base::TimeDelta elapsed_time = tick_clock_->NowTicks() - task_start_time_;

      switch (transaction.type) {
        case DnsQueryType::HTTPS:
          DCHECK(!secure_ ||
                 !features::kUseDnsHttpsSvcbEnforceSecureResponse.Get());
          if (httpssvc_metrics_) {
            // Don't record provider ID for timeouts. It is not precisely known
            // at this level which provider is actually to blame for the
            // timeout, and breaking metrics out by provider is no longer
            // important for current experimentation goals.
            httpssvc_metrics_->SaveForHttps(HttpssvcDnsRcode::kTimedOut,
                                            /*condensed_records=*/{},
                                            elapsed_time);
          }
          break;
        default:
          // The timeout timer is only started when all other transactions have
          // completed.
          NOTREACHED();
      }
    }

    transactions_needed_.clear();
    transactions_in_progress_.clear();

    OnTransactionsFinished();
  }

  // Called on completion of a `DnsTransaction`, but not necessarily completion
  // of all work for the individual transaction in this task (see
  // `OnTransactionsFinished()`).
  void OnDnsTransactionComplete(
      std::set<TransactionInfo>::iterator transaction_info_it,
      uint16_t request_port,
      int net_error,
      const DnsResponse* response) {
    DCHECK(transaction_info_it != transactions_in_progress_.end());
    DCHECK(transactions_in_progress_.find(*transaction_info_it) !=
           transactions_in_progress_.end());

    // Pull the TransactionInfo out of `transactions_in_progress_` now, so it
    // and its underlying DnsTransaction will be deleted on completion of
    // OnTransactionComplete. Note: Once control leaves OnTransactionComplete,
    // there's no further need for the transaction object. On the other hand,
    // since it owns `*response`, it should stay around while
    // OnTransactionComplete executes.
    TransactionInfo transaction_info = std::move(
        transactions_in_progress_.extract(transaction_info_it).value());

    const base::TimeTicks now = tick_clock_->NowTicks();
    base::TimeDelta elapsed_time = now - task_start_time_;
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
    bool fatal_error =
        IsFatalTransactionFailure(net_error, transaction_info, response);
    absl::optional<DnsResponse> fake_response;
    if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                             response->IsValid())) {
      if (transaction_info.error_behavior ==
              TransactionErrorBehavior::kFallback ||
          fatal_error) {
        // Fail task (or maybe Job) completely on network failure.
        OnFailure(net_error, /*allow_fallback=*/!fatal_error,
                  /*ttl=*/absl::nullopt, transaction_info.type);
        return;
      } else {
        DCHECK((transaction_info.error_behavior ==
                    TransactionErrorBehavior::kFatalOrEmpty &&
                !fatal_error) ||
               transaction_info.error_behavior ==
                   TransactionErrorBehavior::kSynthesizeEmpty);
        // For non-fatal failures, synthesize an empty response.
        fake_response =
            CreateFakeEmptyResponse(GetHostname(host_), transaction_info.type);
        response = &fake_response.value();
      }
    }

    HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    DnsResponseResultExtractor extractor(response);
    DnsResponseResultExtractor::ExtractionError extraction_error =
        extractor.ExtractDnsResults(transaction_info.type,
                                    /*original_domain_name=*/GetHostname(host_),
                                    request_port, &results);
    DCHECK_NE(extraction_error,
              DnsResponseResultExtractor::ExtractionError::kUnexpected);

    if (results.error() != OK && results.error() != ERR_NAME_NOT_RESOLVED) {
      net_log_.AddEvent(
          NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK_EXTRACTION_FAILURE,
          [&] {
            return NetLogDnsTaskExtractionFailureParams(
                extraction_error, transaction_info.type, results);
          });
      if (transaction_info.error_behavior ==
              TransactionErrorBehavior::kFatalOrEmpty ||
          transaction_info.error_behavior ==
              TransactionErrorBehavior::kSynthesizeEmpty) {
        // No extraction errors are currently considered fatal, otherwise, there
        // would need to be a call to some sort of
        // IsFatalTransactionExtractionError() function.
        DCHECK(!fatal_error);
        results = DnsResponseResultExtractor::CreateEmptyResult(
            transaction_info.type);
      } else {
        OnFailure(results.error(), /*allow_fallback=*/true,
                  results.GetOptionalTtl(), transaction_info.type);
        return;
      }
    }

    if (httpssvc_metrics_) {
      if (transaction_info.type == DnsQueryType::HTTPS) {
        const std::vector<bool>* record_compatibility =
            results.https_record_compatibility();
        CHECK(record_compatibility);
        httpssvc_metrics_->SaveForHttps(rcode_for_httpssvc,
                                        *record_compatibility, elapsed_time);
      } else {
        httpssvc_metrics_->SaveForAddressQuery(elapsed_time,
                                               rcode_for_httpssvc);
      }
    }

    // Trigger HTTP->HTTPS upgrade if an HTTPS record is received for an "http"
    // or "ws" request.
    if (transaction_info.type == DnsQueryType::HTTPS &&
        ShouldTriggerHttpToHttpsUpgrade(results)) {
      // Disallow fallback. Otherwise DNS could be reattempted without HTTPS
      // queries, and that would hide this error instead of triggering upgrade.
      OnFailure(ERR_DNS_NAME_HTTPS_ONLY, /*allow_fallback=*/false,
                results.GetOptionalTtl(), transaction_info.type);
      return;
    }

    HideMetadataResultsIfNotDesired(results);

    switch (transaction_info.type) {
      case DnsQueryType::A:
        a_record_end_time_ = now;
        if (!aaaa_record_end_time_.is_null()) {
          RecordResolveTimeDiff("AAAABeforeA", task_start_time_,
                                aaaa_record_end_time_, a_record_end_time_);
        }
        break;
      case DnsQueryType::AAAA:
        aaaa_record_end_time_ = now;
        if (!a_record_end_time_.is_null()) {
          RecordResolveTimeDiff("ABeforeAAAA", task_start_time_,
                                a_record_end_time_, aaaa_record_end_time_);
        }
        break;
      case DnsQueryType::HTTPS: {
        base::TimeTicks first_address_end_time =
            std::min(a_record_end_time_, aaaa_record_end_time_);
        if (!first_address_end_time.is_null()) {
          RecordResolveTimeDiff("AddressRecordBeforeHTTPS", task_start_time_,
                                first_address_end_time, now);
        }
        break;
      }
      default:
        break;
    }

    // Merge results with saved results from previous transactions.
    if (saved_results_) {
      // If saved result is a deferred failure, try again to complete with that
      // failure.
      if (saved_results_is_failure_) {
        OnFailure(saved_results_.value().error(), /*allow_fallback=*/true,
                  saved_results_.value().GetOptionalTtl());
        return;
      }

      switch (transaction_info.type) {
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
        case DnsQueryType::HTTPS:
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
    OnTransactionsFinished();
  }

  bool IsFatalTransactionFailure(int transaction_error,
                                 const TransactionInfo& transaction_info,
                                 const DnsResponse* response) {
    if (transaction_info.type != DnsQueryType::HTTPS) {
      DCHECK(transaction_info.error_behavior !=
             TransactionErrorBehavior::kFatalOrEmpty);
      return false;
    }

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
      DCHECK(transaction_info.error_behavior !=
             TransactionErrorBehavior::kFatalOrEmpty);
      error = HttpsTransactionError::kInsecureError;
    } else if (transaction_error == ERR_DNS_SERVER_FAILED && response &&
               response->rcode() != dns_protocol::kRcodeSERVFAIL) {
      // For server failures, only SERVFAIL is fatal.
      error = HttpsTransactionError::kNonFatalError;
    } else if (features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
      DCHECK(transaction_info.error_behavior ==
             TransactionErrorBehavior::kFatalOrEmpty);
      error = HttpsTransactionError::kFatalErrorEnabled;
    } else {
      DCHECK(transaction_info.error_behavior !=
             TransactionErrorBehavior::kFatalOrEmpty);
      error = HttpsTransactionError::kFatalErrorDisabled;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.SvcbHttpsTransactionError",
                              error);
    return error == HttpsTransactionError::kFatalErrorEnabled;
  }

  // Called on processing for one or more individual transaction being
  // completed/cancelled. Processes overall results if all transactions are
  // finished.
  void OnTransactionsFinished() {
    if (!transactions_in_progress_.empty() || !transactions_needed_.empty()) {
      delegate_->OnIntermediateTransactionsComplete();
      MaybeStartTimeoutTimer();
      return;
    }

    DCHECK(saved_results_.has_value());
    HostCache::Entry results = std::move(*saved_results_);

    timeout_timer_.Stop();

    absl::optional<std::vector<IPEndPoint>> ip_endpoints;
    ip_endpoints = base::OptionalFromPtr(results.ip_endpoints());

    if (ip_endpoints.has_value()) {
      // If there are multiple addresses, and at least one is IPv6, need to
      // sort them.
      bool at_least_one_ipv6_address = base::ranges::any_of(
          ip_endpoints.value(),
          [](auto& e) { return e.GetFamily() == ADDRESS_FAMILY_IPV6; });

      if (at_least_one_ipv6_address) {
        // Sort addresses if needed.  Sort could complete synchronously.
        client_->GetAddressSorter()->Sort(
            ip_endpoints.value(),
            base::BindOnce(&DnsTask::OnSortComplete, AsWeakPtr(),
                           tick_clock_->NowTicks(), std::move(results),
                           secure_));
        return;
      }
    }
    OnSuccess(std::move(results));
  }

  void OnSortComplete(base::TimeTicks sort_start_time,
                      HostCache::Entry results,
                      bool secure,
                      bool success,
                      std::vector<IPEndPoint> sorted) {
    DCHECK(results.ip_endpoints());
    results.set_ip_endpoints(std::move(sorted));

    if (!success) {
      OnFailure(ERR_DNS_SORT_ERROR, /*allow_fallback=*/true,
                results.GetOptionalTtl());
      return;
    }

    // AddressSorter prunes unusable destinations.
    if ((!results.ip_endpoints() || results.ip_endpoints()->empty()) &&
        results.text_records().value_or(std::vector<std::string>()).empty() &&
        results.hostnames().value_or(std::vector<HostPortPair>()).empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, /*allow_fallback=*/true,
                results.GetOptionalTtl());
      return;
    }

    OnSuccess(std::move(results));
  }

  bool AnyPotentiallyFatalTransactionsRemain() {
    auto is_fatal_or_empty_error = [](TransactionErrorBehavior behavior) {
      return behavior == TransactionErrorBehavior::kFatalOrEmpty;
    };

    return base::ranges::any_of(transactions_needed_, is_fatal_or_empty_error,
                                &TransactionInfo::error_behavior) ||
           base::ranges::any_of(transactions_in_progress_,
                                is_fatal_or_empty_error,
                                &TransactionInfo::error_behavior);
  }

  void CancelNonFatalTransactions() {
    auto has_non_fatal_or_empty_error = [](const TransactionInfo& info) {
      return info.error_behavior != TransactionErrorBehavior::kFatalOrEmpty;
    };

    base::EraseIf(transactions_needed_, has_non_fatal_or_empty_error);
    base::EraseIf(transactions_in_progress_, has_non_fatal_or_empty_error);
  }

  void OnFailure(
      int net_error,
      bool allow_fallback,
      absl::optional<base::TimeDelta> ttl = absl::nullopt,
      absl::optional<DnsQueryType> failed_transaction_type = absl::nullopt) {
    if (httpssvc_metrics_ && failed_transaction_type.has_value() &&
        IsAddressType(failed_transaction_type.value())) {
      httpssvc_metrics_->SaveAddressQueryFailure();
    }

    DCHECK_NE(OK, net_error);
    HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN, ttl);

    // On non-fatal errors, if any potentially fatal transactions remain, need
    // to defer ending the task in case any of those remaining transactions end
    // with a fatal failure.
    if (allow_fallback && AnyPotentiallyFatalTransactionsRemain()) {
      saved_results_ = std::move(results);
      saved_results_is_failure_ = true;

      CancelNonFatalTransactions();
      OnTransactionsFinished();
      return;
    }

    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK, [&] {
      return NetLogDnsTaskFailedParams(net_error, failed_transaction_type, ttl,
                                       base::OptionalToPtr(saved_results_));
    });

    // Expect this to result in destroying `this` and thus cancelling any
    // remaining transactions.
    delegate_->OnDnsTaskComplete(task_start_time_, allow_fallback,
                                 std::move(results), secure_);
  }

  void OnSuccess(HostCache::Entry results) {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_DNS_TASK,
                      [&] { return NetLogResults(results); });
    delegate_->OnDnsTaskComplete(task_start_time_, /*allow_fallback=*/true,
                                 std::move(results), secure_);
  }

  // Returns whether any transactions left to finish are of a transaction type
  // in `types`. Used for logging and starting the timeout timer (see
  // MaybeStartTimeoutTimer()).
  bool AnyOfTypeTransactionsRemain(
      std::initializer_list<DnsQueryType> types) const {
    // Should only be called if some transactions are still running or waiting
    // to run.
    DCHECK(!transactions_needed_.empty() || !transactions_in_progress_.empty());

    // Check running transactions.
    if (base::ranges::find_first_of(transactions_in_progress_, types,
                                    /*pred=*/{},
                                    /*proj1=*/&TransactionInfo::type) !=
        transactions_in_progress_.end()) {
      return true;
    }

    // Check queued transactions, in case it ever becomes possible to get here
    // without the transactions being started first.
    return base::ranges::find_first_of(transactions_needed_, types, /*pred=*/{},
                                       /*proj1=*/&TransactionInfo::type) !=
           transactions_needed_.end();
  }

  void MaybeStartTimeoutTimer() {
    // Should only be called if some transactions are still running or waiting
    // to run.
    DCHECK(!transactions_in_progress_.empty() || !transactions_needed_.empty());

    // Timer already running.
    if (timeout_timer_.IsRunning())
      return;

    // Always wait for address transactions.
    if (AnyOfTypeTransactionsRemain({DnsQueryType::A, DnsQueryType::AAAA}))
      return;

    base::TimeDelta timeout_max;
    int extra_time_percent = 0;
    base::TimeDelta timeout_min;

    if (AnyOfTypeTransactionsRemain({DnsQueryType::HTTPS})) {
      DCHECK(https_svcb_options_.enable);

      if (secure_) {
        timeout_max = https_svcb_options_.secure_extra_time_max;
        extra_time_percent = https_svcb_options_.secure_extra_time_percent;
        timeout_min = https_svcb_options_.secure_extra_time_min;
      } else {
        timeout_max = https_svcb_options_.insecure_extra_time_max;
        extra_time_percent = https_svcb_options_.insecure_extra_time_percent;
        timeout_min = https_svcb_options_.insecure_extra_time_min;
      }

      // Skip timeout for secure requests if the timeout would be a fatal
      // failure.
      if (secure_ && features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
        timeout_max = base::TimeDelta();
        extra_time_percent = 0;
        timeout_min = base::TimeDelta();
      }
    } else {
      // Unhandled supplemental type.
      NOTREACHED();
    }

    base::TimeDelta timeout;
    if (extra_time_percent > 0) {
      base::TimeDelta total_time_for_other_transactions =
          tick_clock_->NowTicks() - task_start_time_;
      timeout = total_time_for_other_transactions * extra_time_percent / 100;
      // Use at least 1ms to ensure timeout doesn't occur immediately in tests.
      timeout = std::max(timeout, base::Milliseconds(1));

      if (!timeout_max.is_zero())
        timeout = std::min(timeout, timeout_max);
      if (!timeout_min.is_zero())
        timeout = std::max(timeout, timeout_min);
    } else {
      // If no relative timeout, use a non-zero min/max as timeout. If both are
      // non-zero, that's not very sensible, but arbitrarily take the higher
      // timeout.
      timeout = std::max(timeout_min, timeout_max);
    }

    if (!timeout.is_zero())
      timeout_timer_.Start(
          FROM_HERE, timeout,
          base::BindOnce(&DnsTask::OnTimeout, base::Unretained(this)));
  }

  bool ShouldTriggerHttpToHttpsUpgrade(const HostCache::Entry& results) {
    // Upgrade if at least one HTTPS record was compatible, and the host uses an
    // upgradable scheme.
    return results.https_record_compatibility() &&
           base::ranges::any_of(*results.https_record_compatibility(),
                                base::identity()) &&
           (GetScheme(host_) == url::kHttpScheme ||
            GetScheme(host_) == url::kWsScheme);
  }

  // Only keep metadata results (from HTTPS records) for appropriate schemes.
  // This is needed to ensure metadata isn't included in results if the current
  // Feature setup allows querying HTTPS for http:// or ws:// but doesn't enable
  // scheme upgrade to error out on finding an HTTPS record.
  //
  // TODO(crbug.com/1206455): Remove once all requests that query HTTPS will
  // either allow metadata results or error out.
  void HideMetadataResultsIfNotDesired(HostCache::Entry& results) {
    if (GetScheme(host_) == url::kHttpsScheme ||
        GetScheme(host_) == url::kWssScheme) {
      return;
    }

    results.ClearMetadatas();
  }

  const raw_ptr<DnsClient> client_;

  absl::variant<url::SchemeHostPort, std::string> host_;

  base::SafeRef<ResolveContext> resolve_context_;

  // Whether lookups in this DnsTask should occur using DoH or plaintext.
  const bool secure_;
  const SecureDnsMode secure_dns_mode_;

  // The listener to the results of this DnsTask.
  const raw_ptr<Delegate> delegate_;
  const NetLogWithSource net_log_;

  bool any_transaction_started_ = false;
  base::circular_deque<TransactionInfo> transactions_needed_;
  // Active transactions have iterators pointing to their entry in this set, so
  // individual entries should not be modified or removed until completion or
  // cancellation of the transaction.
  std::set<TransactionInfo> transactions_in_progress_;

  // For histograms.
  base::TimeTicks a_record_end_time_;
  base::TimeTicks aaaa_record_end_time_;

  absl::optional<HostCache::Entry> saved_results_;
  bool saved_results_is_failure_ = false;

  const raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks task_start_time_;

  absl::optional<HttpssvcMetrics> httpssvc_metrics_;

  // Timer for task timeout. Generally started after completion of address
  // transactions to allow aborting experimental or supplemental transactions.
  base::OneShotTimer timeout_timer_;

  // If true, there are still significant fallback options available if this
  // task completes unsuccessfully. Used as a signal that underlying
  // transactions should timeout more quickly.
  bool fallback_available_;

  const HostResolver::HttpsSvcbOptions https_svcb_options_;
};

//-----------------------------------------------------------------------------

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
      const NetLogWithSource& source_net_log,
      const base::TickClock* tick_clock,
      const HostResolver::HttpsSvcbOptions& https_svcb_options)
      : resolver_(resolver),
        key_(std::move(key)),
        cache_usage_(cache_usage),
        host_cache_(host_cache),
        tasks_(tasks),
        priority_tracker_(priority),
        tick_clock_(tick_clock),
        https_svcb_options_(https_svcb_options),
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
      CHECK(key_ == req->GetJobKey());
      req->OnJobCancelled(key_);
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
    DCHECK_EQ(GetHostname(key_.host),
              request->request_host().GetHostnameWithoutBrackets());

    request->AssignJob(weak_ptr_factory_.GetSafeRef());

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
    DCHECK_EQ(GetHostname(key_.host),
              req->request_host().GetHostnameWithoutBrackets());

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
    DCHECK_EQ(GetHostname(key_.host),
              request->request_host().GetHostnameWithoutBrackets());
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
      CompleteRequestsWithError(ERR_DNS_REQUEST_CANCELLED,
                                /*task_type=*/absl::nullopt);
    }
  }

  // Called from AbortJobsWithoutTargetNetwork(). Completes all requests and
  // destroys the job. This currently assumes the abort is due to a network
  // change.
  // TODO This should not delete |this|.
  void Abort() {
    CompleteRequestsWithError(ERR_NETWORK_CHANGED, /*task_type=*/absl::nullopt);
  }

  // Gets a closure that will abort an insecure DnsTask (see
  // AbortInsecureDnsTask()) iff |this| is still valid. Useful if aborting a
  // list of Jobs as some may be cancelled while aborting others.
  base::OnceClosure GetAbortInsecureDnsTaskClosure(int error,
                                                   bool fallback_only) {
    return base::BindOnce(&Job::AbortInsecureDnsTask,
                          weak_ptr_factory_.GetWeakPtr(), error, fallback_only);
  }

  // Aborts or removes any current/future insecure DnsTasks if a
  // HostResolverSystemTask is available for fallback. If no fallback is
  // available and |fallback_only| is false, a job that is currently running an
  // insecure DnsTask will be completed with |error|.
  void AbortInsecureDnsTask(int error, bool fallback_only) {
    bool has_system_fallback = base::Contains(tasks_, TaskType::SYSTEM);
    if (has_system_fallback) {
      for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (*it == TaskType::DNS)
          it = tasks_.erase(it);
        else
          ++it;
      }
    }

    if (dns_task_ && !dns_task_->secure()) {
      if (has_system_fallback) {
        KillDnsTask();
        dns_task_error_ = OK;
        RunNextTask();
      } else if (!fallback_only) {
        CompleteRequestsWithError(error, /*task_type=*/absl::nullopt);
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Job::CompleteRequestsWithError,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  ERR_HOST_RESOLVER_QUEUE_TOO_LARGE,
                                  /*task_type=*/absl::nullopt));
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    absl::optional<HostCache::Entry> results = resolver_->ServeFromHosts(
        GetHostname(key_.host), key_.query_types,
        key_.flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6, tasks_);
    if (results) {
      // This will destroy the Job.
      CompleteRequests(results.value(), base::TimeDelta(),
                       true /* allow_cache */, true /* secure */,
                       TaskType::HOSTS);
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
        CompleteRequestsWithError(ERR_NAME_NOT_RESOLVED,
                                  /*task_type=*/absl::nullopt);
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
      CompleteRequests(
          last_result.entry, last_result.ttl, true /* allow_cache */,
          last_result.secure,
          last_result.secure ? TaskType::SECURE_DNS : TaskType::DNS);
      return;
    }

    TaskType next_task = tasks_.front();

    // Schedule insecure DnsTasks and HostResolverSystemTasks with the
    // dispatcher.
    if (!dispatched_ &&
        (next_task == TaskType::DNS || next_task == TaskType::SYSTEM ||
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
      case TaskType::SYSTEM:
        StartSystemTask();
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
      case TaskType::NAT64:
        StartNat64Task();
        break;
      case TaskType::SECURE_CACHE_LOOKUP:
      case TaskType::CACHE_LOOKUP:
      case TaskType::CONFIG_PRESET:
      case TaskType::HOSTS:
        // These task types should have been handled synchronously in
        // ResolveLocally() prior to Job creation.
        NOTREACHED();
        break;
    }
  }

  const JobKey& key() const { return key_; }

  bool is_queued() const { return !handle_.is_null(); }

  bool is_running() const { return job_running_; }

  bool HasTargetNetwork() const {
    return key_.GetTargetNetwork() != handles::kInvalidNetworkHandle;
  }

 private:
  base::Value::Dict NetLogJobCreationParams(const NetLogSource& source) {
    base::Value::Dict dict;
    source.AddToEventParameters(dict);
    dict.Set("host", ToLogStringValue(key_.host));
    base::Value::List query_types_list;
    for (DnsQueryType query_type : key_.query_types)
      query_types_list.Append(kDnsQueryTypes.at(query_type));
    dict.Set("dns_query_types", std::move(query_types_list));
    dict.Set("secure_dns_mode", base::strict_cast<int>(key_.secure_dns_mode));
    dict.Set("network_anonymization_key",
             key_.network_anonymization_key.ToDebugString());
    return dict;
  }

  void Finish() {
    if (is_running()) {
      // Clean up but don't run any callbacks.
      system_task_ = nullptr;
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
      DCHECK_EQ(num_occupied_job_slots_,
                dns_task_->num_transactions_in_progress());
      if (dns_task_->num_additional_transactions_needed() >= 1) {
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
  void StartSystemTask() {
    DCHECK(dispatched_);
    DCHECK_EQ(1, num_occupied_job_slots_);
    DCHECK(HasAddressType(key_.query_types));

    system_task_ = HostResolverSystemTask::Create(
        std::string(GetHostname(key_.host)),
        HostResolver::DnsQueryTypeSetToAddressFamily(key_.query_types),
        key_.flags, resolver_->host_resolver_system_params_, net_log_,
        key_.GetTargetNetwork());

    // Start() could be called from within Resolve(), hence it must NOT directly
    // call OnSystemTaskComplete, for example, on synchronous failure.
    system_task_->Start(base::BindOnce(&Job::OnSystemTaskComplete,
                                       base::Unretained(this),
                                       tick_clock_->NowTicks()));
  }

  // Called by HostResolverSystemTask when it completes.
  void OnSystemTaskComplete(base::TimeTicks start_time,
                            const AddressList& addr_list,
                            int /*os_error*/,
                            int net_error) {
    DCHECK(system_task_);

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (net_error == OK)
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.SystemTask.SuccessTime", duration);
    else
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.SystemTask.FailureTime", duration);

    if (dns_task_error_ != OK && net_error == OK) {
      // This HostResolverSystemTask was a fallback resolution after a failed
      // insecure DnsTask.
      resolver_->OnFallbackResolve(dns_task_error_);
    }

    if (ContainsIcannNameCollisionIp(addr_list.endpoints()))
      net_error = ERR_ICANN_NAME_COLLISION;

    base::TimeDelta ttl = base::Seconds(kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::Seconds(kCacheEntryTTLSeconds);

    auto aliases = std::set<std::string>(addr_list.dns_aliases().begin(),
                                         addr_list.dns_aliases().end());

    // Source unknown because the system resolver could have gotten it from a
    // hosts file, its own cache, a DNS lookup or somewhere else.
    // Don't store the |ttl| in cache since it's not obtained from the server.
    CompleteRequests(
        HostCache::Entry(
            net_error,
            net_error == OK ? addr_list.endpoints() : std::vector<IPEndPoint>(),
            std::move(aliases), HostCache::Entry::SOURCE_UNKNOWN),
        ttl, /*allow_cache=*/true, /*secure=*/false, TaskType::SYSTEM);
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
      CompleteRequestsWithoutCache(resolved.value(), std::move(stale_info),
                                   TaskType::INSECURE_CACHE_LOOKUP);
    } else {
      RunNextTask();
    }
  }

  void StartDnsTask(bool secure) {
    DCHECK_EQ(secure, !dispatched_);
    DCHECK_EQ(dispatched_ ? 1 : 0, num_occupied_job_slots_);
    DCHECK(!resolver_->ShouldForceSystemResolverDueToTestOverride());
    // Need to create the task even if we're going to post a failure instead of
    // running it, as a "started" job needs a task to be properly cleaned up.
    dns_task_ = std::make_unique<DnsTask>(
        resolver_->dns_client_.get(), key_.host, key_.query_types,
        &*key_.resolve_context, secure, key_.secure_dns_mode, this, net_log_,
        tick_clock_, !tasks_.empty() /* fallback_available */,
        https_svcb_options_);
    dns_task_->StartNextTransaction();
    // Schedule a second transaction, if needed. DoH queries can bypass the
    // dispatcher and start all of their transactions immediately.
    if (secure) {
      while (dns_task_->num_additional_transactions_needed() >= 1)
        dns_task_->StartNextTransaction();
      DCHECK_EQ(dns_task_->num_additional_transactions_needed(), 0);
    } else if (dns_task_->num_additional_transactions_needed() >= 1) {
      Schedule(true);
    }
  }

  void StartNextDnsTransaction() {
    DCHECK(dns_task_);
    DCHECK_EQ(dns_task_->secure(), !dispatched_);
    DCHECK(!dispatched_ || num_occupied_job_slots_ ==
                               dns_task_->num_transactions_in_progress() + 1);
    DCHECK_GE(dns_task_->num_additional_transactions_needed(), 1);
    dns_task_->StartNextTransaction();
  }

  // Called if DnsTask fails. It is posted from StartDnsTask, so Job may be
  // deleted before this callback. In this case dns_task is deleted as well,
  // so we use it as indicator whether Job is still valid.
  void OnDnsTaskFailure(const base::WeakPtr<DnsTask>& dns_task,
                        base::TimeDelta duration,
                        bool allow_fallback,
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

    if (!allow_fallback)
      tasks_.clear();

    RunNextTask();
  }

  // HostResolverManager::DnsTask::Delegate implementation:

  void OnDnsTaskComplete(base::TimeTicks start_time,
                         bool allow_fallback,
                         HostCache::Entry results,
                         bool secure) override {
    DCHECK(dns_task_);

    // Tasks containing address queries are only considered successful overall
    // if they find address results. However, DnsTask may claim success if any
    // transaction, e.g. a supplemental HTTPS transaction, finds results.
    DCHECK(!key_.query_types.Has(DnsQueryType::UNSPECIFIED));
    if (HasAddressType(key_.query_types) && results.error() == OK &&
        (!results.ip_endpoints() || results.ip_endpoints()->empty())) {
      results.set_error(ERR_NAME_NOT_RESOLVED);
    }

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (results.error() != OK) {
      OnDnsTaskFailure(dns_task_->AsWeakPtr(), duration, allow_fallback,
                       results, secure);
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

    if ((results.ip_endpoints() &&
         ContainsIcannNameCollisionIp(*results.ip_endpoints()))) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION,
                                secure ? TaskType::SECURE_DNS : TaskType::DNS);
      return;
    }

    CompleteRequests(results, bounded_ttl, true /* allow_cache */, secure,
                     secure ? TaskType::SECURE_DNS : TaskType::DNS);
  }

  void OnIntermediateTransactionsComplete() override {
    if (dispatched_) {
      DCHECK_GE(num_occupied_job_slots_,
                dns_task_->num_transactions_in_progress());
      int unused_slots =
          num_occupied_job_slots_ - dns_task_->num_transactions_in_progress();

      // Reuse vacated slots for any remaining transactions.
      while (unused_slots > 0 &&
             dns_task_->num_additional_transactions_needed() > 0) {
        dns_task_->StartNextTransaction();
        --unused_slots;
      }

      // If all remaining transactions found a slot, no more needed from the
      // dispatcher.
      if (is_queued() && dns_task_->num_additional_transactions_needed() == 0) {
        resolver_->dispatcher_->Cancel(handle_);
        handle_.Reset();
      }

      // Relinquish any remaining extra slots.
      while (unused_slots > 0) {
        ReduceByOneJobSlot();
        --unused_slots;
      }
    } else if (dns_task_->num_additional_transactions_needed() >= 1) {
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

    MDnsClient* client = nullptr;
    int rv = resolver_->GetOrCreateMdnsClient(&client);
    mdns_task_ = std::make_unique<HostResolverMdnsTask>(
        client, std::string{GetHostname(key_.host)}, key_.query_types);

    if (rv == OK) {
      mdns_task_->Start(
          base::BindOnce(&Job::OnMdnsTaskComplete, base::Unretained(this)));
    } else {
      // Could not create an mDNS client. Since we cannot complete synchronously
      // from here, post a failure without starting the task.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Job::OnMdnsImmediateFailure,
                                    weak_ptr_factory_.GetWeakPtr(), rv));
    }
  }

  void OnMdnsTaskComplete() {
    DCHECK(mdns_task_);
    // TODO(crbug.com/846423): Consider adding MDNS-specific logging.

    HostCache::Entry results = mdns_task_->GetResults();

    if ((results.ip_endpoints() &&
         ContainsIcannNameCollisionIp(*results.ip_endpoints()))) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION, TaskType::MDNS);
      return;
    }
    // MDNS uses a separate cache, so skip saving result to cache.
    // TODO(crbug.com/926300): Consider merging caches.
    CompleteRequestsWithoutCache(results, absl::nullopt /* stale_info */,
                                 TaskType::MDNS);
  }

  void OnMdnsImmediateFailure(int rv) {
    DCHECK(mdns_task_);
    DCHECK_NE(OK, rv);

    CompleteRequestsWithError(rv, TaskType::MDNS);
  }

  void StartNat64Task() {
    DCHECK(!nat64_task_);
    RequestImpl* req = requests_.head()->value();
    nat64_task_ = std::make_unique<HostResolverNat64Task>(
        std::string{GetHostname(key_.host)}, req->network_anonymization_key(),
        req->source_net_log(), req->resolve_context(), req->host_cache(),
        resolver_);
    nat64_task_->Start(base::BindOnce(&Job::OnNat64TaskComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNat64TaskComplete() {
    DCHECK(nat64_task_);
    HostCache::Entry results = nat64_task_->GetResults();
    CompleteRequestsWithoutCache(results, absl::nullopt /* stale_info */,
                                 TaskType::NAT64);
  }

  void RecordJobHistograms(const HostCache::Entry& results,
                           absl::optional<TaskType> task_type) {
    int error = results.error();
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

    if (error == OK) {
      DCHECK(task_type.has_value());
      // Record, for HTTPS-capable queries to a host known to serve HTTPS
      // records, whether the HTTPS record was successfully received.
      if (key_.query_types.Has(DnsQueryType::HTTPS) &&
          // Skip http- and ws-schemed hosts. Although they query HTTPS records,
          // successful queries are reported as errors, which would skew the
          // metrics.
          (GetScheme(key_.host) == url::kHttpsScheme ||
           GetScheme(key_.host) == url::kWssScheme) &&
          IsGoogleHostWithAlpnH3(GetHostname(key_.host))) {
        bool has_metadata =
            results.GetMetadatas() && !results.GetMetadatas()->empty();
        base::UmaHistogramExactLinear(
            "Net.DNS.H3SupportedGoogleHost.TaskTypeMetadataAvailability2",
            static_cast<int>(task_type.value()) * 2 + (has_metadata ? 1 : 0),
            (static_cast<int>(TaskType::kMaxValue) + 1) * 2);
      }
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
                        bool secure,
                        absl::optional<TaskType> task_type) {
    CHECK(resolver_.get());

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, if the job was owned by |jobs_|, the job
    // deletes itself when CompleteRequests is done.
    std::unique_ptr<Job> self_deleter;
    if (self_iterator_)
      self_deleter = resolver_->RemoveJob(self_iterator_.value());

    Finish();

    if (results.error() == ERR_DNS_REQUEST_CANCELLED) {
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEventWithNetErrorCode(
          NetLogEventType::HOST_RESOLVER_MANAGER_JOB, OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(
        NetLogEventType::HOST_RESOLVER_MANAGER_JOB, results.error());

    // Handle all caching before completing requests as completing requests may
    // start new requests that rely on cached results.
    if (allow_cache)
      MaybeCacheResult(results, ttl, secure);

    RecordJobHistograms(results, task_type);

    // Complete all of the requests that were attached to the job and
    // detach them.
    while (!requests_.empty()) {
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      CHECK(key_ == req->GetJobKey());

      if (results.error() == OK && !req->parameters().is_speculative) {
        req->set_results(
            results.CopyWithDefaultPort(req->request_host().GetPort()));
      }
      req->OnJobCompleted(
          key_, results.error(),
          /*is_secure_network_error=*/secure && results.error() != OK);

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_.get())
        return;
    }

    // TODO(crbug.com/1200908): Call StartBootstrapFollowup() if any of the
    // requests have the Bootstrap policy.  Note: A naive implementation could
    // cause an infinite loop if the bootstrap result has TTL=0.
  }

  void CompleteRequestsWithoutCache(
      const HostCache::Entry& results,
      absl::optional<HostCache::EntryStaleness> stale_info,
      TaskType task_type) {
    // Record the stale_info for all non-speculative requests, if it exists.
    if (stale_info) {
      for (auto* node = requests_.head(); node != requests_.end();
           node = node->next()) {
        if (!node->value()->parameters().is_speculative)
          node->value()->set_stale_info(stale_info.value());
      }
    }
    CompleteRequests(results, base::TimeDelta(), false /* allow_cache */,
                     false /* secure */, task_type);
  }

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error,
                                 absl::optional<TaskType> task_type) {
    DCHECK_NE(OK, net_error);
    CompleteRequests(
        HostCache::Entry(net_error, HostCache::Entry::SOURCE_UNKNOWN),
        base::TimeDelta(), true /* allow_cache */, false /* secure */,
        task_type);
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
  bool job_running_ = false;

  // Tracks the highest priority across |requests_|.
  PriorityTracker priority_tracker_;

  bool had_non_speculative_request_ = false;

  // Number of slots occupied by this Job in |dispatcher_|. Should be 0 when
  // the job is not registered with any dispatcher.
  int num_occupied_job_slots_ = 0;

  // True once this Job has been sent to `resolver_->dispatcher_`.
  bool dispatched_ = false;

  // Result of DnsTask.
  int dns_task_error_ = OK;

  raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks start_time_;

  HostResolver::HttpsSvcbOptions https_svcb_options_;

  NetLogWithSource net_log_;

  // Resolves the host using the system DNS resolver, which can be overridden
  // for tests.
  std::unique_ptr<HostResolverSystemTask> system_task_;

  // Resolves the host using a DnsTransaction.
  std::unique_ptr<DnsTask> dns_task_;

  // Resolves the host using MDnsClient.
  std::unique_ptr<HostResolverMdnsTask> mdns_task_;

  // Perform NAT64 address synthesis to a given IPv4 literal.
  std::unique_ptr<HostResolverNat64Task> nat64_task_;

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
    : HostResolverManager(PassKey(),
                          options,
                          system_dns_config_notifier,
                          handles::kInvalidNetworkHandle,
                          net_log) {}

HostResolverManager::HostResolverManager(
    base::PassKey<HostResolverManager>,
    const HostResolver::ManagerOptions& options,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier,
    handles::NetworkHandle target_network,
    NetLog* net_log)
    : host_resolver_system_params_(nullptr, options.max_system_retry_attempts),
      net_log_(net_log),
      system_dns_config_notifier_(system_dns_config_notifier),
      target_network_(target_network),
      check_ipv6_on_wifi_(options.check_ipv6_on_wifi),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      https_svcb_options_(
          options.https_svcb_options
              ? *options.https_svcb_options
              : HostResolver::HttpsSvcbOptions::FromFeatures()) {
  PrioritizedDispatcher::Limits job_limits = GetDispatcherLimits(options);
  dispatcher_ = std::make_unique<PrioritizedDispatcher>(job_limits);
  max_queued_jobs_ = job_limits.total_jobs * 100u;

  DCHECK_GE(dispatcher_->num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

#if BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
#endif
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)) || \
    BUILDFLAG(IS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  // Network-bound HostResolverManagers don't need to act on network changes.
  if (!IsBoundToNetwork()) {
    NetworkChangeNotifier::AddIPAddressObserver(this);
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->AddObserver(this);
  EnsureSystemHostResolverCallReady();

  auto connection_type =
      IsBoundToNetwork()
          ? NetworkChangeNotifier::GetNetworkConnectionType(target_network)
          : NetworkChangeNotifier::GetConnectionType();
  UpdateConnectionType(connection_type);

#if defined(ENABLE_BUILT_IN_DNS)
  dns_client_ = DnsClient::CreateClient(net_log_);
  dns_client_->SetInsecureEnabled(
      options.insecure_dns_client_enabled,
      options.additional_types_via_insecure_dns_enabled);
  dns_client_->SetConfigOverrides(options.dns_config_overrides);
#else
  DCHECK(options.dns_config_overrides == DnsConfigOverrides());
#endif

  allow_fallback_to_systemtask_ = !ConfigureAsyncDnsNoFallbackFieldTrial();
}

HostResolverManager::~HostResolverManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Prevent the dispatcher from starting new jobs.
  dispatcher_->SetLimitsToZero();
  // It's now safe for Jobs to call KillDnsTask on destruction, because
  // OnJobComplete will not start any new jobs.
  jobs_.clear();

  if (target_network_ == handles::kInvalidNetworkHandle) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->RemoveObserver(this);
}

// static
std::unique_ptr<HostResolverManager>
HostResolverManager::CreateNetworkBoundHostResolverManager(
    const HostResolver::ManagerOptions& options,
    handles::NetworkHandle target_network,
    NetLog* net_log) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(NetworkChangeNotifier::AreNetworkHandlesSupported());
  return std::make_unique<HostResolverManager>(
      PassKey(), options, nullptr /* system_dns_config_notifier */,
      target_network, net_log);
#else   // !BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverManager::CreateRequest(
    absl::variant<url::SchemeHostPort, HostPortPair> host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    absl::optional<ResolveHostParameters> optional_parameters,
    ResolveContext* resolve_context,
    HostCache* host_cache) {
  return CreateRequest(HostResolver::Host(std::move(host)),
                       std::move(network_anonymization_key), std::move(net_log),
                       std::move(optional_parameters), resolve_context,
                       host_cache);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverManager::CreateRequest(
    HostResolver::Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    absl::optional<ResolveHostParameters> optional_parameters,
    ResolveContext* resolve_context,
    HostCache* host_cache) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

  DCHECK_EQ(resolve_context->GetTargetNetwork(), target_network_);
  // ResolveContexts must register (via RegisterResolveContext()) before use to
  // ensure cached data is invalidated on network and configuration changes.
  DCHECK(registered_contexts_.HasObserver(resolve_context));

  return std::make_unique<RequestImpl>(
      std::move(net_log), std::move(host), std::move(network_anonymization_key),
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

base::Value::Dict HostResolverManager::GetDnsConfigAsValue() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return dns_client_ ? dns_client_->GetDnsConfigAsValueForNetLog()
                     : base::Value::Dict();
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

// static
bool HostResolverManager::IsLocalTask(TaskType task) {
  switch (task) {
    case TaskType::SECURE_CACHE_LOOKUP:
    case TaskType::INSECURE_CACHE_LOOKUP:
    case TaskType::CACHE_LOOKUP:
    case TaskType::CONFIG_PRESET:
    case TaskType::HOSTS:
      return true;
    default:
      return false;
  }
}

HostCache::Entry HostResolverManager::ResolveLocally(
    bool only_ipv6_reachable,
    const JobKey& job_key,
    const IPAddress& ip_address,
    ResolveHostParameters::CacheUsage cache_usage,
    SecureDnsPolicy secure_dns_policy,
    HostResolverSource source,
    const NetLogWithSource& source_net_log,
    HostCache* cache,
    std::deque<TaskType>* out_tasks,
    absl::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = absl::nullopt;

  CreateTaskSequence(job_key, cache_usage, secure_dns_policy, out_tasks);

  if (!ip_address.IsValid()) {
    // Check that the caller supplied a valid hostname to resolve. For
    // MULTICAST_DNS, we are less restrictive.
    // TODO(ericorth): Control validation based on an explicit flag rather
    // than implicitly based on |source|.
    const bool is_valid_hostname =
        job_key.source == HostResolverSource::MULTICAST_DNS
            ? dns_names_util::IsValidDnsName(GetHostname(job_key.host))
            : IsCanonicalizedHostCompliant(GetHostname(job_key.host));
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

  if (ip_address.IsValid()) {
    // Use NAT64Task for IPv4 literal when the network is IPv6 only.
    if (MayUseNAT64ForIPv4Literal(job_key.flags, source, ip_address) &&
        only_ipv6_reachable) {
      out_tasks->push_front(TaskType::NAT64);
      return HostCache::Entry(ERR_DNS_CACHE_MISS,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }

    return ResolveAsIP(job_key.query_types, resolve_canonname, ip_address);
  }

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  absl::optional<HostCache::Entry> resolved =
      ServeLocalhost(GetHostname(job_key.host), job_key.query_types,
                     default_family_due_to_no_ipv6);
  if (resolved)
    return resolved.value();

  // Do initial cache lookups.
  while (!out_tasks->empty() && IsLocalTask(out_tasks->front())) {
    TaskType task = out_tasks->front();
    out_tasks->pop_front();
    if (task == TaskType::SECURE_CACHE_LOOKUP ||
        task == TaskType::INSECURE_CACHE_LOOKUP ||
        task == TaskType::CACHE_LOOKUP) {
      bool secure = task == TaskType::SECURE_CACHE_LOOKUP;
      HostCache::Key key = job_key.ToCacheKey(secure);

      bool ignore_secure = task == TaskType::CACHE_LOOKUP;
      resolved = MaybeServeFromCache(cache, key, cache_usage, ignore_secure,
                                     source_net_log, out_stale_info);
      if (resolved) {
        // |MaybeServeFromCache()| will update |*out_stale_info| as needed.
        DCHECK(out_stale_info->has_value());
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_CACHE_HIT,
            [&] { return NetLogResults(resolved.value()); });

        // TODO(crbug.com/1200908): Call StartBootstrapFollowup() if the Secure
        // DNS Policy is kBootstrap and the result is not secure.  Note: A naive
        // implementation could cause an infinite loop if |resolved| always
        // expires or is evicted before the followup runs.
        return resolved.value();
      }
      DCHECK(!out_stale_info->has_value());
    } else if (task == TaskType::CONFIG_PRESET) {
      resolved = MaybeReadFromConfig(job_key);
      if (resolved) {
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_CONFIG_PRESET_MATCH,
            [&] { return NetLogResults(resolved.value()); });
        StartBootstrapFollowup(job_key, cache, source_net_log);
        return resolved.value();
      }
    } else if (task == TaskType::HOSTS) {
      resolved = ServeFromHosts(GetHostname(job_key.host), job_key.query_types,
                                default_family_due_to_no_ipv6, *out_tasks);
      if (resolved) {
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_HOSTS_HIT,
            [&] { return NetLogResults(resolved.value()); });
        return resolved.value();
      }
    } else {
      NOTREACHED();
    }
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
    job = AddJobWithoutRequest(key, request->parameters().cache_usage,
                               request->host_cache(), std::move(tasks),
                               request->priority(), request->source_net_log());
    job->AddRequest(request);
    job->RunNextTask();
  } else {
    job = jobit->second.get();
    job->AddRequest(request);
  }
}

HostResolverManager::Job* HostResolverManager::AddJobWithoutRequest(
    JobKey key,
    ResolveHostParameters::CacheUsage cache_usage,
    HostCache* host_cache,
    std::deque<TaskType> tasks,
    RequestPriority priority,
    const NetLogWithSource& source_net_log) {
  auto new_job =
      std::make_unique<Job>(weak_ptr_factory_.GetWeakPtr(), key, cache_usage,
                            host_cache, std::move(tasks), priority,
                            source_net_log, tick_clock_, https_svcb_options_);
  auto insert_result = jobs_.emplace(std::move(key), std::move(new_job));
  auto& iterator = insert_result.first;
  bool is_new = insert_result.second;
  DCHECK(is_new);
  auto& job = iterator->second;
  job->OnAddedToJobMap(iterator);
  return job.get();
}

HostCache::Entry HostResolverManager::ResolveAsIP(DnsQueryTypeSet query_types,
                                                  bool resolve_canonname,
                                                  const IPAddress& ip_address) {
  DCHECK(ip_address.IsValid());
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));

  // IP literals cannot resolve unless the query type is an address query that
  // allows addresses with the same address family as the literal. E.g., don't
  // return IPv6 addresses for IPv4 queries or anything for a non-address query.
  AddressFamily family = GetAddressFamily(ip_address);
  if (!query_types.Has(AddressFamilyToDnsQueryType(family))) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  std::set<std::string> aliases;
  if (resolve_canonname) {
    aliases = {ip_address.ToString()};
  }
  return HostCache::Entry(OK, {IPEndPoint(ip_address, 0)}, std::move(aliases),
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

absl::optional<HostCache::Entry> HostResolverManager::MaybeReadFromConfig(
    const JobKey& key) {
  DCHECK(HasAddressType(key.query_types));
  if (!absl::holds_alternative<url::SchemeHostPort>(key.host))
    return absl::nullopt;
  absl::optional<std::vector<IPEndPoint>> preset_addrs =
      dns_client_->GetPresetAddrs(absl::get<url::SchemeHostPort>(key.host));
  if (!preset_addrs)
    return absl::nullopt;

  std::vector<IPEndPoint> filtered_addresses =
      FilterAddresses(std::move(*preset_addrs), key.query_types);
  if (filtered_addresses.empty())
    return absl::nullopt;

  return HostCache::Entry(OK, std::move(filtered_addresses), /*aliases=*/{},
                          HostCache::Entry::SOURCE_CONFIG);
}

void HostResolverManager::StartBootstrapFollowup(
    JobKey key,
    HostCache* host_cache,
    const NetLogWithSource& source_net_log) {
  DCHECK_EQ(SecureDnsMode::kOff, key.secure_dns_mode);
  DCHECK(host_cache);

  key.secure_dns_mode = SecureDnsMode::kSecure;
  if (jobs_.count(key) != 0)
    return;

  Job* job = AddJobWithoutRequest(
      key, ResolveHostParameters::CacheUsage::ALLOWED, host_cache,
      {TaskType::SECURE_DNS}, RequestPriority::LOW, source_net_log);
  job->RunNextTask();
}

absl::optional<HostCache::Entry> HostResolverManager::ServeFromHosts(
    base::StringPiece hostname,
    DnsQueryTypeSet query_types,
    bool default_family_due_to_no_ipv6,
    const std::deque<TaskType>& tasks) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));
  // Don't attempt a HOSTS lookup if there is no DnsConfig or the HOSTS lookup
  // is going to be done next as part of a system lookup.
  if (!dns_client_ || !HasAddressType(query_types) ||
      (!tasks.empty() && tasks.front() == TaskType::SYSTEM))
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
  std::vector<IPEndPoint> addresses;
  if (query_types.Has(DnsQueryType::AAAA)) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV6));
    if (it != hosts->end()) {
      addresses.emplace_back(it->second, 0);
    }
  }

  if (query_types.Has(DnsQueryType::A)) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV4));
    if (it != hosts->end()) {
      addresses.emplace_back(it->second, 0);
    }
  }

  // If got only loopback addresses and the family was restricted, resolve
  // again, without restrictions. See SystemHostResolverCall for rationale.
  if (default_family_due_to_no_ipv6 &&
      base::ranges::all_of(addresses, &IPAddress::IsIPv4,
                           &IPEndPoint::address) &&
      base::ranges::all_of(addresses, &IPAddress::IsLoopback,
                           &IPEndPoint::address)) {
    query_types.Put(DnsQueryType::AAAA);
    return ServeFromHosts(hostname, query_types, false, tasks);
  }

  if (addresses.empty())
    return absl::nullopt;

  return HostCache::Entry(OK, std::move(addresses),
                          /*aliases=*/{}, HostCache::Entry::SOURCE_HOSTS);
}

absl::optional<HostCache::Entry> HostResolverManager::ServeLocalhost(
    base::StringPiece hostname,
    DnsQueryTypeSet query_types,
    bool default_family_due_to_no_ipv6) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));

  std::vector<IPEndPoint> resolved_addresses;
  if (!HasAddressType(query_types) ||
      !ResolveLocalHostname(hostname, &resolved_addresses)) {
    return absl::nullopt;
  }

  if (default_family_due_to_no_ipv6 && query_types.Has(DnsQueryType::A) &&
      !query_types.Has(DnsQueryType::AAAA)) {
    // The caller disabled the AAAA query due to lack of detected IPv6 support.
    // (See SystemHostResolverCall for rationale).
    query_types.Put(DnsQueryType::AAAA);
  }
  std::vector<IPEndPoint> filtered_addresses =
      FilterAddresses(std::move(resolved_addresses), query_types);
  return HostCache::Entry(OK, std::move(filtered_addresses), /*aliases=*/{},
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
    case SecureDnsPolicy::kBootstrap:
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

bool HostResolverManager::ShouldForceSystemResolverDueToTestOverride() const {
  // If tests have provided a catch-all DNS block and then disabled it, check
  // that we are not at risk of sending queries beyond the local network.
  if (HostResolverProc::GetDefault() && system_resolver_disabled_for_testing_) {
    DCHECK(dns_client_);
    DCHECK(dns_client_->GetEffectiveConfig());
    DCHECK(base::ranges::none_of(dns_client_->GetEffectiveConfig()->nameservers,
                                 &IPAddress::IsPubliclyRoutable,
                                 &IPEndPoint::address))
        << "Test could query a publicly-routable address.";
  }
  return !host_resolver_system_params_.resolver_proc &&
         HostResolverProc::GetDefault() &&
         !system_resolver_disabled_for_testing_;
}

void HostResolverManager::PushDnsTasks(bool system_task_allowed,
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
  const bool dns_tasks_allowed = !ShouldForceSystemResolverDueToTestOverride();
  // Upgrade the insecure DnsTask depending on the secure dns mode.
  switch (secure_dns_mode) {
    case SecureDnsMode::kSecure:
      DCHECK(!allow_cache ||
             out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP);
      // Policy misconfiguration can put us in secure DNS mode without any DoH
      // servers to query. See https://crbug.com/1326526.
      if (dns_tasks_allowed && dns_client_->CanUseSecureDnsTransactions())
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
      DCHECK(!allow_cache || IsLocalTask(out_tasks->front()));
      if (dns_tasks_allowed && insecure_tasks_allowed)
        out_tasks->push_back(TaskType::DNS);
      break;
    default:
      NOTREACHED();
      break;
  }

  constexpr TaskType kWantTasks[] = {TaskType::DNS, TaskType::SECURE_DNS};
  const bool no_dns_or_secure_tasks =
      base::ranges::find_first_of(*out_tasks, kWantTasks) == out_tasks->end();
  // The system resolver can be used as a fallback for a non-existent or
  // failing DnsTask if allowed by the request parameters.
  if (system_task_allowed &&
      (no_dns_or_secure_tasks || allow_fallback_to_systemtask_))
    out_tasks->push_back(TaskType::SYSTEM);
}

void HostResolverManager::CreateTaskSequence(
    const JobKey& job_key,
    ResolveHostParameters::CacheUsage cache_usage,
    SecureDnsPolicy secure_dns_policy,
    std::deque<TaskType>* out_tasks) {
  DCHECK(out_tasks->empty());

  // A cache lookup should generally be performed first. For jobs involving a
  // DnsTask, this task may be replaced.
  bool allow_cache =
      cache_usage != ResolveHostParameters::CacheUsage::DISALLOWED;
  if (secure_dns_policy == SecureDnsPolicy::kBootstrap) {
    DCHECK_EQ(SecureDnsMode::kOff, job_key.secure_dns_mode);
    if (allow_cache)
      out_tasks->push_front(TaskType::INSECURE_CACHE_LOOKUP);
    out_tasks->push_front(TaskType::CONFIG_PRESET);
    if (allow_cache)
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
  } else if (allow_cache) {
    if (job_key.secure_dns_mode == SecureDnsMode::kSecure) {
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
    } else {
      out_tasks->push_front(TaskType::CACHE_LOOKUP);
    }
  }
  out_tasks->push_back(TaskType::HOSTS);

  // Determine what type of task a future Job should start.
  bool prioritize_local_lookups =
      cache_usage ==
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  const bool has_address_type = HasAddressType(job_key.query_types);

  switch (job_key.source) {
    case HostResolverSource::ANY:
      // Force address queries with canonname to use HostResolverSystemTask to
      // counter poor CNAME support in DnsTask. See https://crbug.com/872665
      //
      // Otherwise, default to DnsTask (with allowed fallback to
      // HostResolverSystemTask for address queries). But if hostname appears to
      // be an MDNS name (ends in *.local), go with HostResolverSystemTask for
      // address queries and MdnsTask for non- address queries.
      if ((job_key.flags & HOST_RESOLVER_CANONNAME) && has_address_type) {
        out_tasks->push_back(TaskType::SYSTEM);
      } else if (!ResemblesMulticastDNSName(GetHostname(job_key.host))) {
        bool system_task_allowed =
            has_address_type &&
            job_key.secure_dns_mode != SecureDnsMode::kSecure;
        if (dns_client_ && dns_client_->GetEffectiveConfig()) {
          bool insecure_allowed =
              dns_client_->CanUseInsecureDnsTransactions() &&
              !dns_client_->FallbackFromInsecureTransactionPreferred() &&
              (has_address_type ||
               dns_client_->CanQueryAdditionalTypesViaInsecureDns());
          PushDnsTasks(system_task_allowed, job_key.secure_dns_mode,
                       insecure_allowed, allow_cache, prioritize_local_lookups,
                       &*job_key.resolve_context, out_tasks);
        } else if (system_task_allowed) {
          out_tasks->push_back(TaskType::SYSTEM);
        }
      } else if (has_address_type) {
        // For *.local address queries, try the system resolver even if the
        // secure dns mode is SECURE. Public recursive resolvers aren't expected
        // to handle these queries.
        out_tasks->push_back(TaskType::SYSTEM);
      } else {
        out_tasks->push_back(TaskType::MDNS);
      }
      break;
    case HostResolverSource::SYSTEM:
      out_tasks->push_back(TaskType::SYSTEM);
      break;
    case HostResolverSource::DNS:
      if (dns_client_ && dns_client_->GetEffectiveConfig()) {
        bool insecure_allowed =
            dns_client_->CanUseInsecureDnsTransactions() &&
            (has_address_type ||
             dns_client_->CanQueryAdditionalTypesViaInsecureDns());
        PushDnsTasks(false /* system_task_allowed */, job_key.secure_dns_mode,
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

  // `HOST_RESOLVER_CANONNAME` is only supported through system resolution.
  if (job_key.flags & HOST_RESOLVER_CANONNAME) {
    DCHECK(base::ranges::find(*out_tasks, TaskType::DNS) == out_tasks->end());
    DCHECK(base::ranges::find(*out_tasks, TaskType::MDNS) == out_tasks->end());
  }
}

void HostResolverManager::GetEffectiveParametersForRequest(
    const absl::variant<url::SchemeHostPort, std::string>& host,
    DnsQueryType dns_query_type,
    HostResolverFlags flags,
    SecureDnsPolicy secure_dns_policy,
    bool is_ip,
    const NetLogWithSource& net_log,
    DnsQueryTypeSet* out_effective_types,
    HostResolverFlags* out_effective_flags,
    SecureDnsMode* out_effective_secure_dns_mode) {
  const SecureDnsMode secure_dns_mode =
      GetEffectiveSecureDnsMode(secure_dns_policy);

  *out_effective_secure_dns_mode = secure_dns_mode;
  *out_effective_flags = flags | additional_resolver_flags_;

  if (dns_query_type != DnsQueryType::UNSPECIFIED) {
    *out_effective_types = dns_query_type;
    return;
  }

  DnsQueryTypeSet effective_types(DnsQueryType::A, DnsQueryType::AAAA);

  // Disable AAAA queries when we cannot do anything with the results.
  bool use_local_ipv6 = true;
  if (dns_client_) {
    const DnsConfig* config = dns_client_->GetEffectiveConfig();
    if (config)
      use_local_ipv6 = config->use_local_ipv6;
  }
  // When resolving IPv4 literals, there's no need to probe for IPv6. When
  // resolving IPv6 literals, there's no benefit to artificially limiting our
  // resolution based on a probe. Prior logic ensures that this is an automatic
  // query, so the code requesting the resolution should be amenable to
  // receiving an IPv6 resolution.
  if (!use_local_ipv6 && !is_ip && !last_ipv6_probe_result_) {
    *out_effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    effective_types.Remove(DnsQueryType::AAAA);
  }

  // Optimistically enable feature-controlled queries. These queries may be
  // skipped at a later point.

  // `https_svcb_options_.enable` has precedence, so if enabled, ignore any
  // other related features.
  if (https_svcb_options_.enable) {
    static const char* const kSchemesForHttpsQuery[] = {
        url::kHttpScheme, url::kHttpsScheme, url::kWsScheme, url::kWssScheme};
    if (base::Contains(kSchemesForHttpsQuery, GetScheme(host)))
      effective_types.Put(DnsQueryType::HTTPS);
  }

  *out_effective_types = effective_types;
}

namespace {

bool RequestWillUseWiFi(handles::NetworkHandle network) {
  NetworkChangeNotifier::ConnectionType connection_type;
  if (network == handles::kInvalidNetworkHandle)
    connection_type = NetworkChangeNotifier::GetConnectionType();
  else
    connection_type = NetworkChangeNotifier::GetNetworkConnectionType(network);

  return connection_type == NetworkChangeNotifier::CONNECTION_WIFI;
}

}  // namespace

void HostResolverManager::FinishIPv6ReachabilityCheck(
    CompletionOnceCallback callback,
    int rv) {
  SetLastIPv6ProbeResult((rv == OK) ? true : false);
  std::move(callback).Run(OK);
  if (!ipv6_request_callbacks_.empty()) {
    std::vector<CompletionOnceCallback> tmp_request_callbacks;
    ipv6_request_callbacks_.swap(tmp_request_callbacks);
    for (auto& request_callback : tmp_request_callbacks) {
      std::move(request_callback).Run(OK);
    }
  }
}

int HostResolverManager::StartIPv6ReachabilityCheck(
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  // Don't bother checking if the request will use WiFi and IPv6 is assumed to
  // not work on WiFi.
  if (!check_ipv6_on_wifi_ && RequestWillUseWiFi(target_network_)) {
    probing_ipv6_ = false;
    last_ipv6_probe_result_ = false;
    last_ipv6_probe_time_ = base::TimeTicks();
    return OK;
  }

  if (probing_ipv6_) {
    ipv6_request_callbacks_.push_back(std::move(callback));
    return ERR_IO_PENDING;
  }
  // Cache the result for kIPv6ProbePeriodMs (measured from after
  // StartGloballyReachableCheck() completes).
  int rv = OK;
  bool cached = true;
  if (last_ipv6_probe_time_.is_null() ||
      (tick_clock_->NowTicks() - last_ipv6_probe_time_).InMilliseconds() >
          kIPv6ProbePeriodMs) {
    probing_ipv6_ = true;
    rv = StartGloballyReachableCheck(
        IPAddress(kIPv6ProbeAddress), net_log,
        base::BindOnce(&HostResolverManager::FinishIPv6ReachabilityCheck,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    if (rv != ERR_IO_PENDING) {
      SetLastIPv6ProbeResult((rv == OK) ? true : false);
      rv = OK;
    }
    cached = false;
  }
  net_log.AddEvent(
      NetLogEventType::HOST_RESOLVER_MANAGER_IPV6_REACHABILITY_CHECK, [&] {
        return NetLogIPv6AvailableParams(last_ipv6_probe_result_, cached);
      });
  return rv;
}

void HostResolverManager::SetLastIPv6ProbeResult(bool last_ipv6_probe_result) {
  probing_ipv6_ = false;
  last_ipv6_probe_result_ = last_ipv6_probe_result;
  last_ipv6_probe_time_ = tick_clock_->NowTicks();
}

int HostResolverManager::StartGloballyReachableCheck(
    const IPAddress& dest,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  std::unique_ptr<DatagramClientSocket> probing_socket =
      ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, net_log.net_log(), net_log.source());
  DatagramClientSocket* probing_socket_ptr = probing_socket.get();
  auto refcounted_socket = base::MakeRefCounted<
      base::RefCountedData<std::unique_ptr<DatagramClientSocket>>>(
      std::move(probing_socket));
  int rv = probing_socket_ptr->ConnectAsync(
      IPEndPoint(dest, 53),
      base::BindOnce(&HostResolverManager::RunFinishGloballyReachableCheck,
                     weak_ptr_factory_.GetWeakPtr(), refcounted_socket,
                     std::move(callback)));
  if (rv != ERR_IO_PENDING) {
    rv = FinishGloballyReachableCheck(probing_socket_ptr, rv) ? OK : ERR_FAILED;
  }
  return rv;
}

bool HostResolverManager::FinishGloballyReachableCheck(
    DatagramClientSocket* socket,
    int rv) {
  if (rv != OK) {
    return false;
  }
  IPEndPoint endpoint;
  rv = socket->GetLocalAddress(&endpoint);

  if (rv != OK) {
    return false;
  }
  const IPAddress& address = endpoint.address();

  if (address.IsLinkLocal()) {
    return false;
  }

  if (address.IsIPv6()) {
    const uint8_t kTeredoPrefix[] = {0x20, 0x01, 0, 0};
    if (IPAddressStartsWith(address, kTeredoPrefix)) {
      return false;
    }
  }

  return true;
}

void HostResolverManager::RunFinishGloballyReachableCheck(
    scoped_refptr<base::RefCountedData<std::unique_ptr<DatagramClientSocket>>>
        socket,
    CompletionOnceCallback callback,
    int rv) {
  bool is_reachable = FinishGloballyReachableCheck(socket->data.get(), rv);
  std::move(callback).Run(is_reachable ? OK : ERR_FAILED);
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

void HostResolverManager::AbortJobsWithoutTargetNetwork(bool in_progress_only) {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from `jobs_`.
  std::vector<std::unique_ptr<Job>> jobs_to_abort;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    Job* job = it->second.get();
    if (!job->HasTargetNetwork() && (!in_progress_only || job->is_running())) {
      jobs_to_abort.push_back(RemoveJob(it++));
    } else {
      ++it;
    }
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job in `jobs_to_abort` if the DnsConfig just became
  // invalid.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  // Life check to bail once `this` is deleted.
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
  DCHECK(!IsBoundToNetwork());
  last_ipv6_probe_time_ = base::TimeTicks();
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  InvalidateCaches();
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)) || \
    BUILDFLAG(IS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  AbortJobsWithoutTargetNetwork(true /* in_progress_only */);
  // `this` may be deleted inside AbortJobsWithoutTargetNetwork().
}

void HostResolverManager::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(!IsBoundToNetwork());
  UpdateConnectionType(type);
}

void HostResolverManager::OnSystemDnsConfigChanged(
    absl::optional<DnsConfig> config) {
  DCHECK(!IsBoundToNetwork());
  // If tests have provided a catch-all DNS block and then disabled it, check
  // that we are not at risk of sending queries beyond the local network.
  if (HostResolverProc::GetDefault() && system_resolver_disabled_for_testing_ &&
      config.has_value()) {
    DCHECK(base::ranges::none_of(config->nameservers,
                                 &IPAddress::IsPubliclyRoutable,
                                 &IPEndPoint::address))
        << "Test could query a publicly-routable address.";
  }

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
  // Life check to bail once `this` is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  // Existing jobs that were set up using the nameservers and secure dns mode
  // from the original config need to be aborted (does not apply to jobs
  // targeting a specific network).
  AbortJobsWithoutTargetNetwork(false /* in_progress_only */);

  // `this` may be deleted inside AbortJobsWithoutTargetNetwork().
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
  // DnsTasks to HostResolverSystemTasks.
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

void HostResolverManager::UpdateConnectionType(
    NetworkChangeNotifier::ConnectionType type) {
  host_resolver_system_params_.unresponsive_delay =
      GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
          "DnsUnresponsiveDelayMsByConnectionType",
          HostResolverSystemTask::Params::kDnsDefaultUnresponsiveDelay, type);

  // Note that NetworkChangeNotifier always sends a CONNECTION_NONE notification
  // before non-NONE notifications. This check therefore just ensures each
  // connection change notification is handled once and has nothing to do with
  // whether the change is to offline or online.
  if (type == NetworkChangeNotifier::CONNECTION_NONE && dns_client_) {
    dns_client_->ReplaceCurrentSession();
    InvalidateCaches(true /* network_change */);
  }
}

std::unique_ptr<DnsProbeRunner> HostResolverManager::CreateDohProbeRunner(
    ResolveContext* resolve_context) {
  DCHECK(resolve_context);
  DCHECK(registered_contexts_.HasObserver(resolve_context));
  if (!dns_client_ || !dns_client_->CanUseSecureDnsTransactions()) {
    return nullptr;
  }

  return dns_client_->GetTransactionFactory()->CreateDohProbeRunner(
      resolve_context);
}

HostResolverManager::RequestImpl::~RequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value())
    return;

  job_.value()->CancelRequest(this);
  LogCancelRequest();
}

void HostResolverManager::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    priority_ = priority;
    return;
  }
  job_.value()->ChangeRequestPriority(this, priority);
}

const HostResolverManager::JobKey& HostResolverManager::RequestImpl::GetJobKey()
    const {
  CHECK(job_.has_value());
  return job_.value()->key();
}

void HostResolverManager::RequestImpl::OnJobCancelled(const JobKey& job_key) {
  CHECK(job_.has_value());
  CHECK(job_key == job_.value()->key());
  job_.reset();
  DCHECK(!complete_);
  DCHECK(callback_);
  callback_.Reset();

  // No results should be set.
  DCHECK(!results_);

  LogCancelRequest();
}

void HostResolverManager::RequestImpl::OnJobCompleted(
    const JobKey& job_key,
    int error,
    bool is_secure_network_error) {
  set_error_info(error, is_secure_network_error);

  CHECK(job_.has_value());
  CHECK(job_key == job_.value()->key());
  job_.reset();

  DCHECK(!complete_);
  complete_ = true;

  LogFinishRequest(error, true /* async_completion */);

  DCHECK(callback_);
  std::move(callback_).Run(HostResolver::SquashErrorCode(error));
}

}  // namespace net

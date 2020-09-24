// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager.h"
#include "base/task/thread_pool.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <netdb.h>
#include <netinet/in.h>
#if !defined(OS_NACL)
#include <net/if.h>
#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_NACL)
#endif  // defined(OS_WIN)

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/containers/queue.h"
#include "base/debug/debugger.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_reloader.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_mdns_listener_impl.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/resolve_error_info.h"
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

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_library.h"
#endif

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
bool ResemblesMulticastDNSName(const std::string& hostname) {
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
#elif defined(OS_NACL)
  NOTIMPLEMENTED();
  return false;
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

// Creates NetLog parameters when the DnsTask failed.
base::Value NetLogDnsTaskFailedParams(const HostCache::Entry& results,
                                      int dns_error) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("net_error", results.error());
  if (dns_error)
    dict.SetIntKey("dns_error", dns_error);
  dict.SetKey("resolve_results", results.NetLogParams());
  return dict;
}

// Creates NetLog parameters for the creation of a HostResolverManager::Job.
base::Value NetLogJobCreationParams(const NetLogSource& source,
                                    const std::string& host) {
  base::Value dict(base::Value::Type::DICTIONARY);
  source.AddToEventParameters(&dict);
  dict.SetStringKey("host", host);
  return dict;
}

// Creates NetLog parameters for HOST_RESOLVER_IMPL_JOB_ATTACH/DETACH events.
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

// Maximum of 6 concurrent resolver threads (excluding retries).
// Some routers (or resolvers) appear to start to provide host-not-found if
// too many simultaneous resolutions are pending.  This number needs to be
// further optimized, but 8 is what FF currently does. We found some routers
// that limit this to 6, so we're temporarily holding it at that level.
const size_t kDefaultMaxProcTasks = 6u;

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

void NetLogHostCacheEntry(const NetLogWithSource& net_log,
                          NetLogEventType type,
                          NetLogEventPhase phase,
                          const HostCache::Entry& results) {
  net_log.AddEntry(type, phase, [&] { return results.NetLogParams(); });
}

}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(base::StringPiece host, AddressList* address_list) {
  address_list->clear();

  bool is_local6;
  if (!IsLocalHostname(host, &is_local6))
    return false;

  address_list->push_back(IPEndPoint(IPAddress::IPv6Localhost(), 0));
  if (!is_local6) {
    address_list->push_back(IPEndPoint(IPAddress::IPv4Localhost(), 0));
  }

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
    : public CancellableResolveHostRequest,
      public base::LinkNode<HostResolverManager::RequestImpl> {
 public:
  RequestImpl(const NetLogWithSource& source_net_log,
              const HostPortPair& request_host,
              const NetworkIsolationKey& network_isolation_key,
              const base::Optional<ResolveHostParameters>& optional_parameters,
              ResolveContext* resolve_context,
              HostCache* host_cache,
              base::WeakPtr<HostResolverManager> resolver,
              const base::TickClock* tick_clock)
      : source_net_log_(source_net_log),
        request_host_(request_host),
        network_isolation_key_(
            base::FeatureList::IsEnabled(
                net::features::kSplitHostCacheByNetworkIsolationKey)
                ? network_isolation_key
                : NetworkIsolationKey()),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        resolve_context_(resolve_context),
        host_cache_(host_cache),
        host_resolver_flags_(
            HostResolver::ParametersToHostResolverFlags(parameters_)),
        priority_(parameters_.initial_priority),
        job_(nullptr),
        resolver_(resolver),
        complete_(false),
        tick_clock_(tick_clock) {}

  ~RequestImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Cancel();
  }

  void Cancel() override;

  int Start(CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK(!job_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

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

  const base::Optional<AddressList>& GetAddressResults() const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<AddressList>> nullopt_result;
    return results_ ? results_.value().addresses() : *nullopt_result;
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<std::string>>>
        nullopt_result;
    return results_ ? results_.value().text_records() : *nullopt_result;
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<HostPortPair>>>
        nullopt_result;
    return results_ ? results_.value().hostnames() : *nullopt_result;
  }

  const base::Optional<std::vector<bool>>& GetIntegrityResultsForTesting()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<bool>>>
        nullopt_result;
    return results_ ? results_.value().integrity_data() : *nullopt_result;
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    DCHECK(complete_);
    return error_info_;
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
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

  const HostPortPair& request_host() const { return request_host_; }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  ResolveContext* resolve_context() const { return resolve_context_; }

  HostCache* host_cache() const { return host_cache_; }

  HostResolverFlags host_resolver_flags() const { return host_resolver_flags_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  bool complete() const { return complete_; }

 private:
  // Logging and metrics for when a request has just been started.
  void LogStartRequest() {
    DCHECK(request_time_.is_null());
    request_time_ = tick_clock_->NowTicks();

    source_net_log_.BeginEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_REQUEST, [this] {
          base::Value dict(base::Value::Type::DICTIONARY);
          dict.SetStringKey("host", request_host_.ToString());
          dict.SetIntKey("dns_query_type",
                         static_cast<int>(parameters_.dns_query_type));
          dict.SetBoolKey("allow_cached_response",
                          parameters_.cache_usage !=
                              ResolveHostParameters::CacheUsage::DISALLOWED);
          dict.SetBoolKey("is_speculative", parameters_.is_speculative);
          dict.SetStringKey("network_isolation_key",
                            network_isolation_key_.ToDebugString());
          return dict;
        });
  }

  // Logging and metrics for when a request has just completed (before its
  // callback is run).
  void LogFinishRequest(int net_error, bool async_completion) {
    source_net_log_.EndEventWithNetErrorCode(
        NetLogEventType::HOST_RESOLVER_IMPL_REQUEST, net_error);

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
    source_net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST);
  }

  const NetLogWithSource source_net_log_;

  const HostPortPair request_host_;
  const NetworkIsolationKey network_isolation_key_;
  ResolveHostParameters parameters_;
  // TODO(ericorth@chromium.org): Use base::UnownedPtr once available.
  ResolveContext* const resolve_context_;
  HostCache* const host_cache_;
  const HostResolverFlags host_resolver_flags_;

  RequestPriority priority_;

  // The resolve job that this request is dependent on.
  Job* job_;
  base::WeakPtr<HostResolverManager> resolver_;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_;
  base::Optional<HostCache::Entry> results_;
  base::Optional<HostCache::EntryStaleness> stale_info_;
  ResolveErrorInfo error_info_;

  const base::TickClock* const tick_clock_;
  base::TimeTicks request_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

class HostResolverManager::ProbeRequestImpl
    : public CancellableProbeRequest,
      public ResolveContext::DohStatusObserver {
 public:
  ProbeRequestImpl(ResolveContext* context,
                   base::WeakPtr<HostResolverManager> resolver)
      : context_(context), resolver_(resolver) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override { Cancel(); }

  void Cancel() override {
    runner_.reset();

    if (context_)
      context_->UnregisterDohStatusObserver(this);
    context_ = nullptr;
  }

  int Start() override {
    DCHECK(resolver_);
    DCHECK(context_);
    DCHECK(!runner_);

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

    if (!runner_)
      runner_ = resolver_->CreateDohProbeRunner(context_);
    if (runner_)
      runner_->Start(network_change);
  }

  void CancelRunner() {
    runner_.reset();

    // Cancel any asynchronous StartRunner() calls.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  // TODO(ericorth@chromium.org): Use base::UnownedPtr once available.
  ResolveContext* context_;
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

  // Cancels this ProcTask. Any outstanding resolve attempts running on worker
  // thread will continue running, but they will post back to the network thread
  // before checking their WeakPtrs to find that this task is cancelled.
  ~ProcTask() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());

    // If this is cancellation, log the EndEvent (otherwise this was logged in
    // OnLookupComplete()).
    if (!was_completed())
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK);
  }

  void Start() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());
    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK);
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
        NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_STARTED, "attempt_number",
        attempt_number_);

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
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK, [&] {
        return NetLogProcTaskFailedParams(0, error, os_error);
      });
      net_log_.AddEvent(
          NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_FINISHED, [&] {
            return NetLogProcTaskFailedParams(attempt_number, error, os_error);
          });
    } else {
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK,
                        [&] { return results.NetLogParams(); });
      net_log_.AddEventWithIntParams(
          NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_FINISHED,
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

  const base::TickClock* tick_clock_;

  // Used to loop back from the blocking lookup attempt tasks as well as from
  // delayed retry tasks. Invalidate WeakPtrs on completion and cancellation to
  // cancel handling of such posted tasks.
  base::WeakPtrFactory<ProcTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProcTask);
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
                                   const HostCache::Entry& results,
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
          base::StringPiece hostname,
          DnsQueryType query_type,
          ResolveContext* resolve_context,
          bool secure,
          SecureDnsMode secure_dns_mode,
          Delegate* delegate,
          const NetLogWithSource& job_net_log,
          const base::TickClock* tick_clock)
      : client_(client),
        hostname_(hostname),
        resolve_context_(resolve_context),
        secure_(secure),
        secure_dns_mode_(secure_dns_mode),
        delegate_(delegate),
        net_log_(job_net_log),
        num_completed_transactions_(0),
        tick_clock_(tick_clock),
        task_start_time_(tick_clock_->NowTicks()) {
    DCHECK(client_);
    if (secure_)
      DCHECK(client_->CanUseSecureDnsTransactions());
    else
      DCHECK(client_->CanUseInsecureDnsTransactions());

    if (query_type != DnsQueryType::UNSPECIFIED) {
      transactions_needed_.push(query_type);
    } else {
      transactions_needed_.push(DnsQueryType::A);
      transactions_needed_.push(DnsQueryType::AAAA);

      // Queue up an INTEGRITY query if we are allowed to.
      const bool is_httpssvc_experiment_domain =
          httpssvc_domain_cache_.IsExperimental(hostname);
      const bool is_httpssvc_control_domain =
          httpssvc_domain_cache_.IsControl(hostname);
      if (base::FeatureList::IsEnabled(features::kDnsHttpssvc) &&
          features::kDnsHttpssvcUseIntegrity.Get() &&
          (secure_ || features::kDnsHttpssvcEnableQueryOverInsecure.Get()) &&
          (is_httpssvc_experiment_domain || is_httpssvc_control_domain)) {
        // We should not be configured to query HTTPSSVC *and* INTEGRITY.
        DCHECK(!features::kDnsHttpssvcUseHttpssvc.Get());

        httpssvc_metrics_.emplace(
            is_httpssvc_experiment_domain /* expect_intact */);
        transactions_needed_.push(DnsQueryType::INTEGRITY);
      }
    }
    num_needed_transactions_ = transactions_needed_.size();

    DCHECK(delegate_);
  }

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
        static_cast<int>(transactions_needed_.size()))
      net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK);

    DnsQueryType type = transactions_needed_.front();
    transactions_needed_.pop();

    // Record how long this transaction has been waiting to be created.
    base::TimeDelta time_queued = tick_clock_->NowTicks() - task_start_time_;
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.PerTransaction",
                                 time_queued);
    delegate_->AddTransactionTimeQueued(time_queued);

    std::unique_ptr<DnsTransaction> transaction = CreateTransaction(type);
    transaction->Start();
    transactions_started_.insert(std::move(transaction));
  }

 private:
  static const HostCache::Entry& GetMalformedResponseResult() {
    static const base::NoDestructor<HostCache::Entry> kMalformedResponseResult(
        ERR_DNS_MALFORMED_RESPONSE, HostCache::Entry::SOURCE_DNS);
    return *kMalformedResponseResult;
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(
      DnsQueryType dns_query_type) {
    DCHECK_NE(DnsQueryType::UNSPECIFIED, dns_query_type);

    std::unique_ptr<DnsTransaction> trans =
        client_->GetTransactionFactory()->CreateTransaction(
            hostname_, DnsQueryTypeToQtype(dns_query_type),
            base::BindOnce(&DnsTask::OnTransactionComplete,
                           base::Unretained(this), tick_clock_->NowTicks(),
                           dns_query_type),
            net_log_, secure_, secure_dns_mode_, resolve_context_);
    trans->SetRequestPriority(delegate_->priority());
    return trans;
  }

  void OnExperimentalQueryTimeout(uint16_t qtype,
                                  base::Optional<std::string> doh_provider_id) {
    // The experimental query timer is only started when all other transactions
    // have completed.
    DCHECK(TaskIsCompleteOrOnlyQtypeTransactionsRemain(qtype));

    num_completed_transactions_ += transactions_started_.size();
    DCHECK(num_completed_transactions_ == num_needed_transactions());
    transactions_started_.clear();

    if (qtype == dns_protocol::kExperimentalTypeIntegrity) {
      DCHECK(httpssvc_metrics_);

      // Record that this INTEGRITY query timed out in the metrics.
      base::TimeDelta elapsed_time = tick_clock_->NowTicks() - task_start_time_;
      httpssvc_metrics_->SaveForIntegrity(
          doh_provider_id, HttpssvcDnsRcode::kTimedOut, {}, elapsed_time);
    }

    ProcessResultsOnCompletion();
  }

  void OnTransactionComplete(const base::TimeTicks& start_time,
                             DnsQueryType dns_query_type,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response,
                             base::Optional<std::string> doh_provider_id) {
    DCHECK(transaction);

    // Once control leaves OnTransactionComplete, there's no further
    // need for the transaction object. On the other hand, since it owns
    // |*response|, it should stay around while OnTransactionComplete
    // executes.
    std::unique_ptr<DnsTransaction> destroy_transaction_on_return;
    {
      auto it = transactions_started_.find(transaction);
      DCHECK(it != transactions_started_.end());

      destroy_transaction_on_return = std::move(*it);
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

    if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                             response->IsValid())) {
      if (dns_query_type == DnsQueryType::INTEGRITY) {
        // Do not allow an INTEGRITY query to fail the whole DnsTask.
        response = nullptr;
      } else {
        OnFailure(net_error, DnsResponse::DNS_PARSE_OK, base::nullopt);
        return;
      }
    }

    DnsResponse::Result parse_result = DnsResponse::DNS_PARSE_RESULT_MAX;
    HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    switch (dns_query_type) {
      case DnsQueryType::UNSPECIFIED:
        // Should create multiple transactions with specified types.
        NOTREACHED();
        break;
      case DnsQueryType::A:
      case DnsQueryType::AAAA:
        parse_result = ParseAddressDnsResponse(response, &results);
        break;
      case DnsQueryType::TXT:
        parse_result = ParseTxtDnsResponse(response, &results);
        break;
      case DnsQueryType::PTR:
        parse_result = ParsePointerDnsResponse(response, &results);
        break;
      case DnsQueryType::SRV:
        parse_result = ParseServiceDnsResponse(response, &results);
        break;
      case DnsQueryType::INTEGRITY:
        // Parse the INTEGRITY records, condensing them into a vector<bool>.
        parse_result = ParseIntegrityDnsResponse(response, &results);
        break;
    }
    DCHECK_LT(parse_result, DnsResponse::DNS_PARSE_RESULT_MAX);

    if (results.error() != OK && results.error() != ERR_NAME_NOT_RESOLVED) {
      OnFailure(results.error(), parse_result, results.GetOptionalTtl());
      return;
    }

    if (httpssvc_metrics_) {
      if (dns_query_type != DnsQueryType::INTEGRITY) {
        httpssvc_metrics_->SaveForNonIntegrity(doh_provider_id, elapsed_time,
                                               rcode_for_httpssvc);
      } else {
        const base::Optional<std::vector<bool>>& condensed =
            results.integrity_data();
        CHECK(condensed.has_value());
        // INTEGRITY queries can time out the normal way (here), or when the
        // experimental query timer runs out (OnExperimentalQueryTimeout).
        httpssvc_metrics_->SaveForIntegrity(doh_provider_id, rcode_for_httpssvc,
                                            *condensed, elapsed_time);
      }
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
      // If the experimental query times out, blame the provider that gave the
      // last A/AAAA result. If we were being 100% correct, we would blame the
      // provider associated with the experimental query.
      MaybeStartExperimentalQueryTimer(doh_provider_id);
      return;
    }

    // Since all transactions are complete, in particular, all experimental
    // transactions are complete (if any were started).
    experimental_query_cancellation_timer_.Stop();

    ProcessResultsOnCompletion();
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

    OnSuccess(results);
  }

  DnsResponse::Result ParseAddressDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    DCHECK(response);
    AddressList addresses;
    base::TimeDelta ttl;
    DnsResponse::Result parse_result =
        response->ParseToAddressList(&addresses, &ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
    } else if (addresses.empty()) {
      *out_results = HostCache::Entry(ERR_NAME_NOT_RESOLVED, AddressList(),
                                      HostCache::Entry::SOURCE_DNS, ttl);
    } else {
      addresses.Deduplicate();
      *out_results = HostCache::Entry(OK, std::move(addresses),
                                      HostCache::Entry::SOURCE_DNS, ttl);
    }
    return parse_result;
  }

  DnsResponse::Result ParseTxtDnsResponse(const DnsResponse* response,
                                          HostCache::Entry* out_results) {
    DCHECK(response);
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypeTXT, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<std::string> text_records;
    for (const auto& record : records) {
      const TxtRecordRdata* rdata = record->rdata<net::TxtRecordRdata>();
      text_records.insert(text_records.end(), rdata->texts().begin(),
                          rdata->texts().end());
    }

    *out_results = HostCache::Entry(
        text_records.empty() ? ERR_NAME_NOT_RESOLVED : OK,
        std::move(text_records), HostCache::Entry::SOURCE_DNS, response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  DnsResponse::Result ParsePointerDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    DCHECK(response);
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypePTR, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<HostPortPair> pointers;
    for (const auto& record : records) {
      const PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();
      std::string pointer = rdata->ptrdomain();

      // Skip pointers to the root domain.
      if (!pointer.empty())
        pointers.emplace_back(std::move(pointer), 0);
    }

    *out_results = HostCache::Entry(
        pointers.empty() ? ERR_NAME_NOT_RESOLVED : OK, std::move(pointers),
        HostCache::Entry::SOURCE_DNS, response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  DnsResponse::Result ParseServiceDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    DCHECK(response);
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypeSRV, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<const SrvRecordRdata*> fitered_rdatas;
    for (const auto& record : records) {
      const SrvRecordRdata* rdata = record->rdata<net::SrvRecordRdata>();

      // Skip pointers to the root domain.
      if (!rdata->target().empty())
        fitered_rdatas.push_back(rdata);
    }

    std::vector<HostPortPair> ordered_service_targets =
        SortServiceTargets(fitered_rdatas);

    *out_results = HostCache::Entry(
        ordered_service_targets.empty() ? ERR_NAME_NOT_RESOLVED : OK,
        std::move(ordered_service_targets), HostCache::Entry::SOURCE_DNS,
        response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  DnsResponse::Result ParseIntegrityDnsResponse(const DnsResponse* response,
                                                HostCache::Entry* out_results) {
    base::Optional<base::TimeDelta> response_ttl;
    const HostCache::Entry default_entry(
        OK, std::vector<bool>(), HostCache::Entry::SOURCE_DNS, response_ttl);

    if (response == nullptr) {
      *out_results = default_entry;
      return DnsResponse::Result::DNS_PARSE_OK;
    }

    std::vector<std::unique_ptr<const RecordParsed>> records;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kExperimentalTypeIntegrity, &records,
        &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = default_entry;
      return DnsResponse::Result::DNS_PARSE_OK;
    }

    // Condense results into a list of booleans. We do not cache the results,
    // but this enables us to write some unit tests.
    std::vector<bool> condensed_results;
    for (const auto& record : records) {
      const IntegrityRecordRdata& rdata =
          *record->rdata<IntegrityRecordRdata>();
      condensed_results.push_back(rdata.IsIntact());
    }

    *out_results = HostCache::Entry(OK, std::move(condensed_results),
                                    HostCache::Entry::SOURCE_DNS, response_ttl);
    DCHECK_EQ(parse_result, DnsResponse::DNS_PARSE_OK);
    return parse_result;
  }

  // Sort service targets per RFC2782.  In summary, sort first by |priority|,
  // lowest first.  For targets with the same priority, secondary sort randomly
  // using |weight| with higher weighted objects more likely to go first.
  std::vector<HostPortPair> SortServiceTargets(
      const std::vector<const SrvRecordRdata*>& rdatas) {
    std::map<uint16_t, std::unordered_set<const SrvRecordRdata*>>
        ordered_by_priority;
    for (const SrvRecordRdata* rdata : rdatas)
      ordered_by_priority[rdata->priority()].insert(rdata);

    std::vector<HostPortPair> sorted_targets;
    for (auto& priority : ordered_by_priority) {
      // With (num results) <= UINT16_MAX (and in practice, much less) and
      // (weight per result) <= UINT16_MAX, then it should be the case that
      // (total weight) <= UINT32_MAX, but use CheckedNumeric for extra safety.
      auto total_weight = base::MakeCheckedNum<uint32_t>(0);
      for (const SrvRecordRdata* rdata : priority.second)
        total_weight += rdata->weight();

      // Add 1 to total weight because, to deal with 0-weight targets, we want
      // our random selection to be inclusive [0, total].
      total_weight++;

      // Order by weighted random. Make such random selections, removing from
      // |priority.second| until |priority.second| only contains 1 rdata.
      while (priority.second.size() >= 2) {
        uint32_t random_selection =
            base::RandGenerator(total_weight.ValueOrDie());
        const SrvRecordRdata* selected_rdata = nullptr;
        for (const SrvRecordRdata* rdata : priority.second) {
          // >= to always select the first target on |random_selection| == 0,
          // even if its weight is 0.
          if (rdata->weight() >= random_selection) {
            selected_rdata = rdata;
            break;
          }
          random_selection -= rdata->weight();
        }

        DCHECK(selected_rdata);
        sorted_targets.emplace_back(selected_rdata->target(),
                                    selected_rdata->port());
        total_weight -= selected_rdata->weight();
        size_t removed = priority.second.erase(selected_rdata);
        DCHECK_EQ(1u, removed);
      }

      DCHECK_EQ(1u, priority.second.size());
      DCHECK_EQ((total_weight - 1).ValueOrDie(),
                (*priority.second.begin())->weight());
      const SrvRecordRdata* rdata = *priority.second.begin();
      sorted_targets.emplace_back(rdata->target(), rdata->port());
    }

    return sorted_targets;
  }

  DnsResponse::Result ParseAndFilterResponseRecords(
      const DnsResponse* response,
      uint16_t filter_dns_type,
      std::vector<std::unique_ptr<const RecordParsed>>* out_records,
      base::Optional<base::TimeDelta>* out_response_ttl) {
    out_records->clear();
    out_response_ttl->reset();

    DnsRecordParser parser = response->Parser();

    // Expected to be validated by DnsTransaction.
    DCHECK_EQ(filter_dns_type, response->qtype());

    for (unsigned i = 0; i < response->answer_count(); ++i) {
      std::unique_ptr<const RecordParsed> record =
          RecordParsed::CreateFrom(&parser, base::Time::Now());

      if (!record)
        return DnsResponse::DNS_MALFORMED_RESPONSE;
      if (!base::EqualsCaseInsensitiveASCII(record->name(),
                                            response->GetDottedName())) {
        return DnsResponse::DNS_NAME_MISMATCH;
      }

      // Ignore any records that are not class Internet and type
      // |filter_dns_type|.
      if (record->klass() == dns_protocol::kClassIN &&
          record->type() == filter_dns_type) {
        base::TimeDelta ttl = base::TimeDelta::FromSeconds(record->ttl());
        *out_response_ttl =
            std::min(out_response_ttl->value_or(base::TimeDelta::Max()), ttl);

        out_records->push_back(std::move(record));
      }
    }

    return DnsResponse::DNS_PARSE_OK;
  }

  void OnSortComplete(base::TimeTicks sort_start_time,
                      HostCache::Entry results,
                      bool secure,
                      bool success,
                      const AddressList& addr_list) {
    results.set_addresses(addr_list);

    if (!success) {
      OnFailure(ERR_DNS_SORT_ERROR, DnsResponse::DNS_PARSE_OK,
                results.GetOptionalTtl());
      return;
    }

    // AddressSorter prunes unusable destinations.
    if (addr_list.empty() &&
        results.text_records().value_or(std::vector<std::string>()).empty() &&
        results.hostnames().value_or(std::vector<HostPortPair>()).empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK,
                results.GetOptionalTtl());
      return;
    }

    OnSuccess(results);
  }

  void OnFailure(int net_error,
                 DnsResponse::Result parse_result,
                 base::Optional<base::TimeDelta> ttl) {
    if (httpssvc_metrics_)
      httpssvc_metrics_->SaveNonIntegrityFailure();

    DCHECK_NE(OK, net_error);
    HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN);

    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK, [&] {
      return NetLogDnsTaskFailedParams(results, parse_result);
    });

    // If we have a TTL from a previously completed transaction, use it.
    base::TimeDelta previous_transaction_ttl;
    if (saved_results_ && saved_results_.value().has_ttl() &&
        saved_results_.value().ttl() <
            base::TimeDelta::FromSeconds(
                std::numeric_limits<uint32_t>::max())) {
      previous_transaction_ttl = saved_results_.value().ttl();
      if (ttl)
        results.set_ttl(std::min(ttl.value(), previous_transaction_ttl));
      else
        results.set_ttl(previous_transaction_ttl);
    } else if (ttl) {
      results.set_ttl(ttl.value());
    }

    delegate_->OnDnsTaskComplete(task_start_time_, results, secure_);
  }

  void OnSuccess(const HostCache::Entry& results) {
    NetLogHostCacheEntry(net_log_, NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK,
                         NetLogEventPhase::END, results);
    delegate_->OnDnsTaskComplete(task_start_time_, results, secure_);
  }

  // Returns whether all transactions left to execute are of transaction type
  // |qtype|. (In particular, this is the case if all transactions are
  // complete.) Used for logging and starting the experimental query timer (see
  // MaybeStartExperimentalQueryTimer).
  bool TaskIsCompleteOrOnlyQtypeTransactionsRemain(uint16_t qtype) const {
    // Since DoH runs all transactions concurrently and experimental types are
    // only queried over DoH, this method only needs to check the transactions
    // in transactions_started_ because transactions_needed_ is empty from the
    // time the first transaction is started.
    DCHECK(transactions_needed_.empty());

    return std::all_of(
        transactions_started_.begin(), transactions_started_.end(),
        [&](const std::unique_ptr<DnsTransaction>& p) {
          DCHECK(p);
          return p->GetType() == qtype;
        });
  }


  void MaybeStartExperimentalQueryTimer(
      base::Optional<std::string> doh_provider_id) {
    DCHECK(!transactions_started_.empty());

    // Abort if neither HTTPSSVC nor INTEGRITY querying is enabled.
    if (!base::FeatureList::IsEnabled(features::kDnsHttpssvc) ||
        (!features::kDnsHttpssvcUseIntegrity.Get() &&
         !features::kDnsHttpssvcUseHttpssvc.Get())) {
      return;
    }

    if (!experimental_query_cancellation_timer_.IsRunning() &&
        TaskIsCompleteOrOnlyQtypeTransactionsRemain(
            dns_protocol::kExperimentalTypeIntegrity)) {
      const base::TimeDelta kExtraTimeAbsolute =
          features::dns_httpssvc_experiment::GetExtraTimeAbsolute();
      const int kExtraTimePercent =
          features::kDnsHttpssvcExtraTimePercent.Get();

      base::TimeDelta total_time_for_other_transactions =
          tick_clock_->NowTicks() - task_start_time_;
      base::TimeDelta relative_timeout =
          total_time_for_other_transactions * kExtraTimePercent / 100;

      base::TimeDelta timeout = std::min(kExtraTimeAbsolute, relative_timeout);

      experimental_query_cancellation_timer_.Start(
          FROM_HERE, timeout,
          base::BindOnce(
              &DnsTask::OnExperimentalQueryTimeout, base::Unretained(this),
              dns_protocol::kExperimentalTypeIntegrity, doh_provider_id));
    }
  }

  DnsClient* client_;
  std::string hostname_;
  // TODO(ericorth@chromium.org): Use base::UnownedPtr once available.
  ResolveContext* const resolve_context_;

  // Whether lookups in this DnsTask should occur using DoH or plaintext.
  const bool secure_;
  const SecureDnsMode secure_dns_mode_;

  // The listener to the results of this DnsTask.
  Delegate* delegate_;
  const NetLogWithSource net_log_;

  base::queue<DnsQueryType> transactions_needed_;
  base::flat_set<std::unique_ptr<DnsTransaction>, base::UniquePtrComparator>
      transactions_started_;
  int num_needed_transactions_;
  int num_completed_transactions_;

  // Result from previously completed transactions. Only set if a transaction
  // has completed while others are still in progress.
  base::Optional<HostCache::Entry> saved_results_;

  const base::TickClock* tick_clock_;
  base::TimeTicks task_start_time_;

  HttpssvcExperimentDomainCache httpssvc_domain_cache_;
  base::Optional<HttpssvcMetrics> httpssvc_metrics_;

  // Timer for early abort of experimental queries. See comments describing the
  // timeout parameters in net/base/features.h.
  base::OneShotTimer experimental_query_cancellation_timer_;

  DISALLOW_COPY_AND_ASSIGN(DnsTask);
};

//-----------------------------------------------------------------------------

struct HostResolverManager::JobKey {
  bool operator<(const JobKey& other) const {
    return std::forward_as_tuple(query_type, flags, source, secure_dns_mode,
                                 resolve_context, hostname,
                                 network_isolation_key_) <
           std::forward_as_tuple(other.query_type, other.flags, other.source,
                                 other.secure_dns_mode, other.resolve_context,
                                 other.hostname, other.network_isolation_key_);
  }

  std::string hostname;
  NetworkIsolationKey network_isolation_key_;
  DnsQueryType query_type;
  HostResolverFlags flags;
  HostResolverSource source;
  SecureDnsMode secure_dns_mode;
  // TODO(ericorth@chromium.org): Use base::UnownedPtr once available.
  ResolveContext* resolve_context;
};

// Aggregates all Requests for the same Key. Dispatched via
// PrioritizedDispatcher.
class HostResolverManager::Job : public PrioritizedDispatcher::Job,
                                 public HostResolverManager::DnsTask::Delegate {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(const base::WeakPtr<HostResolverManager>& resolver,
      base::StringPiece hostname,
      const NetworkIsolationKey& network_isolation_key,
      DnsQueryType query_type,
      HostResolverFlags host_resolver_flags,
      HostResolverSource requested_source,
      ResolveHostParameters::CacheUsage cache_usage,
      SecureDnsMode secure_dns_mode,
      ResolveContext* resolve_context,
      HostCache* host_cache,
      std::deque<TaskType> tasks,
      RequestPriority priority,
      scoped_refptr<base::TaskRunner> proc_task_runner,
      const NetLogWithSource& source_net_log,
      const base::TickClock* tick_clock)
      : resolver_(resolver),
        hostname_(hostname),
        network_isolation_key_(network_isolation_key),
        query_type_(query_type),
        host_resolver_flags_(host_resolver_flags),
        requested_source_(requested_source),
        cache_usage_(cache_usage),
        secure_dns_mode_(secure_dns_mode),
        resolve_context_(resolve_context),
        host_cache_(host_cache),
        tasks_(tasks),
        job_running_(false),
        priority_tracker_(priority),
        proc_task_runner_(std::move(proc_task_runner)),
        had_non_speculative_request_(false),
        num_occupied_job_slots_(0),
        dispatcher_(nullptr),
        dns_task_error_(OK),
        is_secure_dns_task_error_(false),
        tick_clock_(tick_clock),
        start_time_(base::TimeTicks()),
        net_log_(
            NetLogWithSource::Make(source_net_log.net_log(),
                                   NetLogSourceType::HOST_RESOLVER_IMPL_JOB)) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_CREATE_JOB);

    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB, [&] {
      return NetLogJobCreationParams(source_net_log.source(), hostname_);
    });
  }

  ~Job() override {
    if (is_running()) {
      // |resolver_| was destroyed with this Job still in flight.
      // Clean-up, record in the log, but don't run any callbacks.
      proc_task_ = nullptr;
      // Clean up now for nice NetLog.
      KillDnsTask();
      net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                        ERR_ABORTED);
    } else if (is_queued()) {
      // |resolver_| was destroyed without running this Job.
      // TODO(szym): is there any benefit in having this distinction?
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB);
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
    DCHECK(dispatcher_);
    if (!at_head) {
      handle = dispatcher_->Add(this, priority());
    } else {
      handle = dispatcher_->AddAtHead(this, priority());
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
    DCHECK_EQ(hostname_, request->request_host().host());

    request->AssignJob(this);

    priority_tracker_.Add(request->priority());

    request->source_net_log().AddEventReferencingSource(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_ATTACH, net_log_.source());

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_ATTACH,
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
    DCHECK_EQ(hostname_, req->request_host().host());

    priority_tracker_.Remove(req->priority());
    req->set_priority(priority);
    priority_tracker_.Add(req->priority());
    UpdatePriority();
  }

  // Detach cancelled request. If it was the last active Request, also finishes
  // this Job.
  void CancelRequest(RequestImpl* request) {
    DCHECK_EQ(hostname_, request->request_host().host());
    DCHECK(!requests_.empty());

    priority_tracker_.Remove(request->priority());
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_DETACH,
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
        is_secure_dns_task_error_ = false;
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

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_EVICTED);

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
    base::Optional<HostCache::Entry> results = resolver_->ServeFromHosts(
        hostname_, query_type_,
        host_resolver_flags_ & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6,
        tasks_);
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
    self_iterator_ = base::nullopt;
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
    if (!dispatcher_ &&
        (next_task == TaskType::DNS || next_task == TaskType::PROC ||
         next_task == TaskType::MDNS)) {
      dispatcher_ = resolver_->dispatcher_.get();
      job_running_ = false;
      Schedule(false);
      DCHECK(is_running() || is_queued());

      // Check for queue overflow.
      if (dispatcher_->num_queued_jobs() > resolver_->max_queued_jobs_) {
        Job* evicted = static_cast<Job*>(dispatcher_->EvictOldestLowest());
        DCHECK(evicted);
        evicted->OnEvicted();
      }
      return;
    }

    if (start_time_ == base::TimeTicks()) {
      net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_STARTED);
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
  HostCache::Key GenerateCacheKey(bool secure) const {
    HostCache::Key cache_key(hostname_, query_type_, host_resolver_flags_,
                             requested_source_, network_isolation_key_);
    cache_key.secure = secure;
    return cache_key;
  }

  void KillDnsTask() {
    if (dns_task_) {
      if (dispatcher_) {
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
    DCHECK(dispatcher_);
    if (is_queued()) {
      dispatcher_->Cancel(handle_);
      handle_.Reset();
    } else if (num_occupied_job_slots_ > 1) {
      dispatcher_->OnJobFinished();
      --num_occupied_job_slots_;
    } else {
      NOTREACHED();
    }
  }

  void UpdatePriority() {
    if (is_queued() && dispatcher_)
      handle_ = dispatcher_->ChangePriority(handle_, priority());
  }

  // PrioritizedDispatcher::Job:
  void Start() override {
    handle_.Reset();
    ++num_occupied_job_slots_;

    if (num_occupied_job_slots_ >= 2) {
      if (!dns_task_) {
        dispatcher_->OnJobFinished();
        return;
      }
      DCHECK(dns_task_);
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
    DCHECK(dispatcher_);
    DCHECK_EQ(1, num_occupied_job_slots_);
    DCHECK(IsAddressType(query_type_));

    proc_task_ = std::make_unique<ProcTask>(
        hostname_, HostResolver::DnsQueryTypeToAddressFamily(query_type_),
        host_resolver_flags_, resolver_->proc_params_,
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

    if (dns_task_error_ != OK) {
      // If a secure DNS task previously failed and fell back to a ProcTask
      // without issuing an insecure DNS task in between, record what happened
      // to the fallback ProcTask.
      if (is_secure_dns_task_error_) {
        base::UmaHistogramSparse(
            "Net.DNS.SecureDnsTaskFailure.FallbackProcTask.Error",
            std::abs(net_error));
      }

      // This ProcTask was a fallback resolution after a failed insecure
      // DnsTask.
      if (net_error == OK) {
        resolver_->OnFallbackResolve(dns_task_error_);
      }
    }

    if (ContainsIcannNameCollisionIp(addr_list))
      net_error = ERR_ICANN_NAME_COLLISION;

    base::TimeDelta ttl =
        base::TimeDelta::FromSeconds(kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);

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
    base::Optional<HostCache::EntryStaleness> stale_info;
    base::Optional<HostCache::Entry> resolved = resolver_->MaybeServeFromCache(
        host_cache_, GenerateCacheKey(false), cache_usage_,
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
    DCHECK_EQ(secure, !dispatcher_);
    DCHECK_EQ(dispatcher_ ? 1 : 0, num_occupied_job_slots_);
    DCHECK(!resolver_->HaveTestProcOverride());
    // Need to create the task even if we're going to post a failure instead of
    // running it, as a "started" job needs a task to be properly cleaned up.
    dns_task_.reset(new DnsTask(resolver_->dns_client_.get(), hostname_,
                                query_type_, resolve_context_, secure,
                                secure_dns_mode_, this, net_log_, tick_clock_));
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
    DCHECK_EQ(dns_task_->secure(), !dispatcher_);
    DCHECK(!dispatcher_ || num_occupied_job_slots_ >= 1);
    DCHECK(dns_task_);
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

    if (secure_dns_mode_ == SecureDnsMode::kSecure) {
      DCHECK(secure);
      UMA_HISTOGRAM_LONG_TIMES_100(
          "Net.DNS.SecureDnsTask.DnsModeSecure.FailureTime", duration);
    } else if (secure_dns_mode_ == SecureDnsMode::kAutomatic && secure) {
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

    if (duration < base::TimeDelta::FromMilliseconds(10)) {
      base::UmaHistogramSparse(
          secure ? "Net.DNS.SecureDnsTask.ErrorBeforeFallback.Fast"
                 : "Net.DNS.DnsTask.ErrorBeforeFallback.Fast",
          std::abs(failure_results.error()));
    } else {
      base::UmaHistogramSparse(
          secure ? "Net.DNS.SecureDnsTask.ErrorBeforeFallback.Slow"
                 : "Net.DNS.DnsTask.ErrorBeforeFallback.Slow",
          std::abs(failure_results.error()));
    }

    // If one of the fallback tasks doesn't complete the request, store a result
    // to use during request completion.
    base::TimeDelta ttl = failure_results.has_ttl()
                              ? failure_results.ttl()
                              : base::TimeDelta::FromSeconds(0);
    completion_results_.push_back({failure_results, ttl, secure});

    dns_task_error_ = failure_results.error();
    is_secure_dns_task_error_ = secure;
    KillDnsTask();
    RunNextTask();
  }

  // HostResolverManager::DnsTask::Delegate implementation:

  void OnDnsTaskComplete(base::TimeTicks start_time,
                         const HostCache::Entry& results,
                         bool secure) override {
    DCHECK(dns_task_);

    // If a secure DNS task previously failed, record what happened to the
    // fallback insecure DNS task.
    if (dns_task_error_ != OK && is_secure_dns_task_error_) {
      base::UmaHistogramSparse(
          "Net.DNS.SecureDnsTaskFailure.FallbackDnsTask.Error",
          std::abs(results.error()));
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

    base::TimeDelta bounded_ttl = std::max(
        results.ttl(), base::TimeDelta::FromSeconds(kMinimumTTLSeconds));

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

    if (dispatcher_) {
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
          dispatcher_->Cancel(handle_);
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
    DCHECK_EQ(0, host_resolver_flags_ &
                     ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);

    std::vector<DnsQueryType> query_types;
    if (query_type_ == DnsQueryType::UNSPECIFIED) {
      query_types.push_back(DnsQueryType::A);
      query_types.push_back(DnsQueryType::AAAA);
    } else {
      query_types.push_back(query_type_);
    }

    MDnsClient* client = nullptr;
    int rv = resolver_->GetOrCreateMdnsClient(&client);
    mdns_task_ =
        std::make_unique<HostResolverMdnsTask>(client, hostname_, query_types);

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
      CompleteRequestsWithoutCache(results, base::nullopt /* stale_info */);
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
      if (duration < base::TimeDelta::FromMilliseconds(10))
        base::UmaHistogramSparse("Net.DNS.ResolveError.Fast", std::abs(error));
      else
        base::UmaHistogramSparse("Net.DNS.ResolveError.Slow", std::abs(error));
    }

    if (had_non_speculative_request_) {
      UmaHistogramMediumTimes(
          base::StringPrintf("Net.DNS.SecureDnsMode.%s.ResolveTime",
                             SecureDnsModeToString(secure_dns_mode_).c_str()),
          duration);
    }
  }

  void MaybeCacheResult(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool secure) {
    // If the request did not complete, don't cache it.
    if (!results.did_complete())
      return;
    HostCache::Key cache_key = GenerateCacheKey(secure);
    resolver_->CacheResult(host_cache_, cache_key, results, ttl);
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

    if (is_running()) {
      proc_task_ = nullptr;
      KillDnsTask();
      mdns_task_ = nullptr;
      job_running_ = false;

      if (dispatcher_) {
        // Signal dispatcher that a slot has opened.
        DCHECK_EQ(1, num_occupied_job_slots_);
        dispatcher_->OnJobFinished();
      }
    } else if (is_queued()) {
      DCHECK(dispatcher_);
      dispatcher_->Cancel(handle_);
      handle_.Reset();
    }

    if (num_active_requests() == 0) {
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                        OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                      results.error());

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
            results.CopyWithDefaultPort(req->request_host().port()));
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
      base::Optional<HostCache::EntryStaleness> stale_info) {
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

  const std::string hostname_;
  const NetworkIsolationKey network_isolation_key_;
  const DnsQueryType query_type_;
  const HostResolverFlags host_resolver_flags_;
  const HostResolverSource requested_source_;
  const ResolveHostParameters::CacheUsage cache_usage_;
  const SecureDnsMode secure_dns_mode_;
  // TODO(ericorth@chromium.org): Use base::UnownedPtr once available.
  ResolveContext* const resolve_context_;
  // TODO(crbug.com/969847): Consider allowing requests within a single Job to
  // have different HostCaches.
  HostCache* const host_cache_;

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

  // The dispatcher with which this Job is currently registered. Is nullptr if
  // not registered with any dispatcher.
  PrioritizedDispatcher* dispatcher_;

  // Result of DnsTask.
  int dns_task_error_;

  // Whether the error in |dns_task_error_| corresponds to an insecure or
  // secure DnsTask.
  bool is_secure_dns_task_error_;

  const base::TickClock* tick_clock_;
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
  base::Optional<JobMap::iterator> self_iterator_;

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
  dispatcher_.reset(new PrioritizedDispatcher(job_limits));
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
  dns_client_->SetInsecureEnabled(options.insecure_dns_client_enabled);
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

std::unique_ptr<HostResolverManager::CancellableResolveHostRequest>
HostResolverManager::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters,
    ResolveContext* resolve_context,
    HostCache* host_cache) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

  // ResolveContexts must register (via RegisterResolveContext()) before use to
  // ensure cached data is invalidated on network and configuration changes.
  DCHECK(registered_contexts_.HasObserver(resolve_context));

  return std::make_unique<RequestImpl>(
      net_log, host, network_isolation_key, optional_parameters,
      resolve_context, host_cache, weak_ptr_factory_.GetWeakPtr(), tick_clock_);
}

std::unique_ptr<HostResolverManager::CancellableProbeRequest>
HostResolverManager::CreateDohProbeRequest(ResolveContext* context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return std::make_unique<ProbeRequestImpl>(context,
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

void HostResolverManager::SetInsecureDnsClientEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!dns_client_)
    return;

  bool enabled_before = dns_client_->CanUseInsecureDnsTransactions();
  dns_client_->SetInsecureEnabled(enabled);

  if (dns_client_->CanUseInsecureDnsTransactions() != enabled_before)
    AbortInsecureDnsTasks(ERR_NETWORK_CHANGED, false /* fallback_only */);
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

  DnsQueryType effective_query_type;
  HostResolverFlags effective_host_resolver_flags;
  SecureDnsMode effective_secure_dns_mode;
  std::deque<TaskType> tasks;
  base::Optional<HostCache::EntryStaleness> stale_info;
  HostCache::Entry results = ResolveLocally(
      request->request_host().host(), request->network_isolation_key(),
      request->parameters().dns_query_type, request->parameters().source,
      request->host_resolver_flags(),
      request->parameters().secure_dns_mode_override,
      request->parameters().cache_usage, request->source_net_log(),
      request->host_cache(), request->resolve_context(), &effective_query_type,
      &effective_host_resolver_flags, &effective_secure_dns_mode, &tasks,
      &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY ||
      tasks.empty()) {
    if (results.error() == OK && !request->parameters().is_speculative) {
      request->set_results(
          results.CopyWithDefaultPort(request->request_host().port()));
    }
    if (stale_info && !request->parameters().is_speculative)
      request->set_stale_info(std::move(stale_info).value());
    request->set_error_info(results.error(),
                            false /* is_secure_network_error */);
    return HostResolver::SquashErrorCode(results.error());
  }

  CreateAndStartJob(effective_query_type, effective_host_resolver_flags,
                    effective_secure_dns_mode, std::move(tasks), request);
  return ERR_IO_PENDING;
}

HostCache::Entry HostResolverManager::ResolveLocally(
    const std::string& hostname,
    const NetworkIsolationKey& network_isolation_key,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    base::Optional<SecureDnsMode> secure_dns_mode_override,
    ResolveHostParameters::CacheUsage cache_usage,
    const NetLogWithSource& source_net_log,
    HostCache* cache,
    ResolveContext* resolve_context,
    DnsQueryType* out_effective_query_type,
    HostResolverFlags* out_effective_host_resolver_flags,
    SecureDnsMode* out_effective_secure_dns_mode,
    std::deque<TaskType>* out_tasks,
    base::Optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = base::nullopt;

  IPAddress ip_address;
  IPAddress* ip_address_ptr = nullptr;
  if (ip_address.AssignFromIPLiteral(hostname)) {
    ip_address_ptr = &ip_address;
  }

  GetEffectiveParametersForRequest(
      hostname, dns_query_type, source, flags, secure_dns_mode_override,
      cache_usage, ip_address_ptr, source_net_log, resolve_context,
      out_effective_query_type, out_effective_host_resolver_flags,
      out_effective_secure_dns_mode, out_tasks);

  if (!ip_address.IsValid()) {
    // Check that the caller supplied a valid hostname to resolve. For
    // MULTICAST_DNS, we are less restrictive.
    // TODO(ericorth): Control validation based on an explicit flag rather
    // than implicitly based on |source|.
    const bool is_valid_hostname = source == HostResolverSource::MULTICAST_DNS
                                       ? IsValidUnrestrictedDNSDomain(hostname)
                                       : IsValidDNSDomain(hostname);
    if (!is_valid_hostname) {
      return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }
  }

  bool resolve_canonname =
      *out_effective_host_resolver_flags & HOST_RESOLVER_CANONNAME;
  bool default_family_due_to_no_ipv6 =
      *out_effective_host_resolver_flags &
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (hostname.empty() || hostname.size() > kMaxHostLength) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  base::Optional<HostCache::Entry> resolved =
      ResolveAsIP(*out_effective_query_type, resolve_canonname, ip_address_ptr);
  if (resolved)
    return resolved.value();

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  resolved = ServeLocalhost(hostname, *out_effective_query_type,
                            default_family_due_to_no_ipv6);
  if (resolved)
    return resolved.value();

  // Do initial cache lookup.
  if (!out_tasks->empty() &&
      (out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP ||
       out_tasks->front() == TaskType::INSECURE_CACHE_LOOKUP ||
       out_tasks->front() == TaskType::CACHE_LOOKUP)) {
    HostCache::Key key(hostname, *out_effective_query_type,
                       *out_effective_host_resolver_flags, source,
                       network_isolation_key);

    if (out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP)
      key.secure = true;

    bool ignore_secure = false;
    if (out_tasks->front() == TaskType::CACHE_LOOKUP)
      ignore_secure = true;

    out_tasks->pop_front();

    resolved = MaybeServeFromCache(cache, key, cache_usage, ignore_secure,
                                   source_net_log, out_stale_info);
    if (resolved) {
      // |MaybeServeFromCache()| will update |*out_stale_info| as needed.
      DCHECK(out_stale_info->has_value());
      NetLogHostCacheEntry(source_net_log,
                           NetLogEventType::HOST_RESOLVER_IMPL_CACHE_HIT,
                           NetLogEventPhase::NONE, resolved.value());

      return resolved.value();
    }
    DCHECK(!out_stale_info->has_value());
  }

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  resolved = ServeFromHosts(hostname, *out_effective_query_type,
                            default_family_due_to_no_ipv6, *out_tasks);
  if (resolved) {
    NetLogHostCacheEntry(source_net_log,
                         NetLogEventType::HOST_RESOLVER_IMPL_HOSTS_HIT,
                         NetLogEventPhase::NONE, resolved.value());
    return resolved.value();
  }

  return HostCache::Entry(ERR_DNS_CACHE_MISS, HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverManager::CreateAndStartJob(
    DnsQueryType effective_query_type,
    HostResolverFlags effective_host_resolver_flags,
    SecureDnsMode effective_secure_dns_mode,
    std::deque<TaskType> tasks,
    RequestImpl* request) {
  DCHECK(!tasks.empty());
  JobKey key = {
      request->request_host().host(), request->network_isolation_key(),
      effective_query_type,           effective_host_resolver_flags,
      request->parameters().source,   effective_secure_dns_mode,
      request->resolve_context()};

  auto jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    auto new_job = std::make_unique<Job>(
        weak_ptr_factory_.GetWeakPtr(), request->request_host().host(),
        request->network_isolation_key(), effective_query_type,
        effective_host_resolver_flags, request->parameters().source,
        request->parameters().cache_usage, effective_secure_dns_mode,
        request->resolve_context(), request->host_cache(), std::move(tasks),
        request->priority(), proc_task_runner_, request->source_net_log(),
        tick_clock_);
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

base::Optional<HostCache::Entry> HostResolverManager::ResolveAsIP(
    DnsQueryType query_type,
    bool resolve_canonname,
    const IPAddress* ip_address) {
  if (ip_address == nullptr || !IsAddressType(query_type))
    return base::nullopt;

  AddressFamily family = GetAddressFamily(*ip_address);
  if (query_type != DnsQueryType::UNSPECIFIED &&
      query_type != AddressFamilyToDnsQueryType(family)) {
    // Don't return IPv6 addresses for IPv4 queries, and vice versa.
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  AddressList addresses = AddressList::CreateFromIPAddress(*ip_address, 0);
  if (resolve_canonname)
    addresses.SetDefaultCanonicalName();
  return HostCache::Entry(OK, std::move(addresses),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

base::Optional<HostCache::Entry> HostResolverManager::MaybeServeFromCache(
    HostCache* cache,
    const HostCache::Key& key,
    ResolveHostParameters::CacheUsage cache_usage,
    bool ignore_secure,
    const NetLogWithSource& source_net_log,
    base::Optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = base::nullopt;

  if (!cache)
    return base::nullopt;

  if (cache_usage == ResolveHostParameters::CacheUsage::DISALLOWED)
    return base::nullopt;

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
    NetLogHostCacheEntry(source_net_log,
                         NetLogEventType::HOST_RESOLVER_IMPL_CACHE_HIT,
                         NetLogEventPhase::NONE, cache_result->second);
    return cache_result->second;
  }
  return base::nullopt;
}

base::Optional<HostCache::Entry> HostResolverManager::ServeFromHosts(
    base::StringPiece hostname,
    DnsQueryType query_type,
    bool default_family_due_to_no_ipv6,
    const std::deque<TaskType>& tasks) {
  // Don't attempt a HOSTS lookup if there is no DnsConfig or the HOSTS lookup
  // is going to be done next as part of a system lookup.
  if (!dns_client_ || !IsAddressType(query_type) ||
      (!tasks.empty() && tasks.front() == TaskType::PROC))
    return base::nullopt;
  const DnsHosts* hosts = dns_client_->GetHosts();

  if (!hosts || hosts->empty())
    return base::nullopt;

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

  return base::nullopt;
}

base::Optional<HostCache::Entry> HostResolverManager::ServeLocalhost(
    base::StringPiece hostname,
    DnsQueryType query_type,
    bool default_family_due_to_no_ipv6) {
  AddressList resolved_addresses;
  if (!IsAddressType(query_type) ||
      !ResolveLocalHostname(hostname, &resolved_addresses)) {
    return base::nullopt;
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
  if (cache && (entry.error() == OK || ttl > base::TimeDelta()))
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
    const std::string& hostname,
    base::Optional<SecureDnsMode> secure_dns_mode_override) {
  const DnsConfig* config =
      dns_client_ ? dns_client_->GetEffectiveConfig() : nullptr;

  SecureDnsMode secure_dns_mode = SecureDnsMode::kOff;
  if (secure_dns_mode_override) {
    secure_dns_mode = secure_dns_mode_override.value();
  } else if (config) {
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
    const std::string& hostname,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    base::Optional<SecureDnsMode> secure_dns_mode_override,
    ResolveHostParameters::CacheUsage cache_usage,
    ResolveContext* resolve_context,
    SecureDnsMode* out_effective_secure_dns_mode,
    std::deque<TaskType>* out_tasks) {
  DCHECK(out_tasks->empty());
  *out_effective_secure_dns_mode =
      GetEffectiveSecureDnsMode(hostname, secure_dns_mode_override);

  // A cache lookup should generally be performed first. For jobs involving a
  // DnsTask, this task may be replaced.
  bool allow_cache =
      cache_usage != ResolveHostParameters::CacheUsage::DISALLOWED;
  if (allow_cache) {
    if (*out_effective_secure_dns_mode == SecureDnsMode::kSecure) {
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
    } else {
      out_tasks->push_front(TaskType::CACHE_LOOKUP);
    }
  }

  // Determine what type of task a future Job should start.
  bool prioritize_local_lookups =
      cache_usage ==
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  switch (source) {
    case HostResolverSource::ANY:
      // Force address queries with canonname to use ProcTask to counter poor
      // CNAME support in DnsTask. See https://crbug.com/872665
      //
      // Otherwise, default to DnsTask (with allowed fallback to ProcTask for
      // address queries). But if hostname appears to be an MDNS name (ends in
      // *.local), go with ProcTask for address queries and MdnsTask for non-
      // address queries.
      if ((flags & HOST_RESOLVER_CANONNAME) && IsAddressType(dns_query_type)) {
        out_tasks->push_back(TaskType::PROC);
      } else if (!ResemblesMulticastDNSName(hostname)) {
        bool proc_task_allowed =
            IsAddressType(dns_query_type) &&
            *out_effective_secure_dns_mode != SecureDnsMode::kSecure;
        if (dns_client_ && dns_client_->GetEffectiveConfig()) {
          bool insecure_allowed =
              dns_client_->CanUseInsecureDnsTransactions() &&
              !dns_client_->FallbackFromInsecureTransactionPreferred();
          PushDnsTasks(proc_task_allowed, *out_effective_secure_dns_mode,
                       insecure_allowed, allow_cache, prioritize_local_lookups,
                       resolve_context, out_tasks);
        } else if (proc_task_allowed) {
          out_tasks->push_back(TaskType::PROC);
        }
      } else if (IsAddressType(dns_query_type)) {
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
        PushDnsTasks(false /* proc_task_allowed */,
                     *out_effective_secure_dns_mode,
                     dns_client_->CanUseInsecureDnsTransactions(), allow_cache,
                     prioritize_local_lookups, resolve_context, out_tasks);
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
    const std::string& hostname,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    base::Optional<SecureDnsMode> secure_dns_mode_override,
    ResolveHostParameters::CacheUsage cache_usage,
    const IPAddress* ip_address,
    const NetLogWithSource& net_log,
    ResolveContext* resolve_context,
    DnsQueryType* out_effective_type,
    HostResolverFlags* out_effective_flags,
    SecureDnsMode* out_effective_secure_dns_mode,
    std::deque<TaskType>* out_tasks) {
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
      !use_local_ipv6 && ip_address == nullptr && !IsIPv6Reachable(net_log)) {
    *out_effective_type = DnsQueryType::A;
    *out_effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  }

  CreateTaskSequence(hostname, *out_effective_type, source,
                     *out_effective_flags, secure_dns_mode_override,
                     cache_usage, resolve_context,
                     out_effective_secure_dns_mode, out_tasks);
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
      NetLogEventType::HOST_RESOLVER_IMPL_IPV6_REACHABILITY_CHECK, [&] {
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
    base::Optional<DnsConfig> config) {
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
  if (!dns_client_->CanUseSecureDnsTransactions())
    return nullptr;

  return dns_client_->GetTransactionFactory()->CreateDohProbeRunner(
      resolve_context);
}

void HostResolverManager::RequestImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_)
    return;

  job_->CancelRequest(this);
  job_ = nullptr;
  callback_.Reset();

  LogCancelRequest();
}

void HostResolverManager::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(job_);
  job_->ChangeRequestPriority(this, priority);
}

}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_impl.h"

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
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/linked_list.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
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
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_reloader.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mdns_client.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_parameters_callback.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "url/url_canon_ip.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

// Default delay between calls to the system resolver for the same hostname.
// (Can be overridden by field trial.)
const int64_t kDnsDefaultUnresponsiveDelayMs = 6000;

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
const uint8_t kIPv6ProbeAddress[] =
    { 0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88 };

// We use a separate histogram name for each platform to facilitate the
// display of error codes by their symbolic name (since each platform has
// different mappings).
const char kOSErrorsForGetAddrinfoHistogramName[] =
#if defined(OS_WIN)
    "Net.OSErrorsForGetAddrinfo_Win";
#elif defined(OS_MACOSX)
    "Net.OSErrorsForGetAddrinfo_Mac";
#elif defined(OS_LINUX)
    "Net.OSErrorsForGetAddrinfo_Linux";
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    "Net.OSErrorsForGetAddrinfo";
#endif

// Gets a list of the likely error codes that getaddrinfo() can return
// (non-exhaustive). These are the error codes that we will track via
// a histogram.
std::vector<int> GetAllGetAddrinfoOSErrors() {
  int os_errors[] = {
#if defined(OS_WIN)
    // See: http://msdn.microsoft.com/en-us/library/ms738520(VS.85).aspx
    WSA_NOT_ENOUGH_MEMORY,
    WSAEAFNOSUPPORT,
    WSAEINVAL,
    WSAESOCKTNOSUPPORT,
    WSAHOST_NOT_FOUND,
    WSANO_DATA,
    WSANO_RECOVERY,
    WSANOTINITIALISED,
    WSATRY_AGAIN,
    WSATYPE_NOT_FOUND,
    // The following are not in doc, but might be to appearing in results :-(.
    WSA_INVALID_HANDLE,
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#if !defined(OS_FREEBSD)
#if !defined(OS_ANDROID)
    // EAI_ADDRFAMILY has been declared obsolete in Android's and
    // FreeBSD's netdb.h.
    EAI_ADDRFAMILY,
#endif
    // EAI_NODATA has been declared obsolete in FreeBSD's netdb.h.
    EAI_NODATA,
#endif
    EAI_AGAIN,
    EAI_BADFLAGS,
    EAI_FAIL,
    EAI_FAMILY,
    EAI_MEMORY,
    EAI_NONAME,
    EAI_SERVICE,
    EAI_SOCKTYPE,
    EAI_SYSTEM,
#endif
  };

  // Ensure all errors are positive, as histogram only tracks positive values.
  for (size_t i = 0; i < arraysize(os_errors); ++i) {
    os_errors[i] = std::abs(os_errors[i]);
  }

  return base::CustomHistogram::ArrayToCustomEnumRanges(os_errors);
}

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

void UmaAsyncDnsResolveStatus(DnsResolveStatus result) {
  UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ResolveStatus",
                            result,
                            RESOLVE_STATUS_MAX);
}

bool ResemblesNetBIOSName(const std::string& hostname) {
  return (hostname.size() < 16) && (hostname.find('.') == std::string::npos);
}

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const char kSuffix[] = ".local.";
  const size_t kSuffixLen = sizeof(kSuffix) - 1;
  const size_t kSuffixLenTrimmed = kSuffixLen - 1;
  if (hostname.back() == '.') {
    return hostname.size() > kSuffixLen &&
        !hostname.compare(hostname.size() - kSuffixLen, kSuffixLen, kSuffix);
  }
  return hostname.size() > kSuffixLenTrimmed &&
      !hostname.compare(hostname.size() - kSuffixLenTrimmed, kSuffixLenTrimmed,
                        kSuffix, kSuffixLenTrimmed);
}

// A macro to simplify code and readability.
#define DNS_HISTOGRAM_BY_PRIORITY(basename, priority, time)        \
  do {                                                             \
    switch (priority) {                                            \
      case HIGHEST:                                                \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".HIGHEST", time);   \
        break;                                                     \
      case MEDIUM:                                                 \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".MEDIUM", time);    \
        break;                                                     \
      case LOW:                                                    \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".LOW", time);       \
        break;                                                     \
      case LOWEST:                                                 \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".LOWEST", time);    \
        break;                                                     \
      case IDLE:                                                   \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".IDLE", time);      \
        break;                                                     \
      case THROTTLED:                                              \
        UMA_HISTOGRAM_LONG_TIMES_100(basename ".THROTTLED", time); \
        break;                                                     \
    }                                                              \
    UMA_HISTOGRAM_LONG_TIMES_100(basename, time);                  \
  } while (0)

// Record time from Request creation until a valid DNS response.
void RecordTotalTime(bool speculative,
                     bool from_cache,
                     base::TimeDelta duration) {
  if (!speculative) {
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.TotalTime", duration);

    if (!from_cache)
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.TotalTimeNotCached", duration);
  }
}

void RecordTTL(base::TimeDelta ttl) {
  UMA_HISTOGRAM_CUSTOM_TIMES("AsyncDNS.TTL", ttl,
                             base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(1), 100);
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

AddressList EnsurePortOnAddressList(const AddressList& list, uint16_t port) {
  if (list.empty() || list.front().port() == port)
    return list;
  return AddressList::CopyWithPort(list, port);
}

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
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::WILL_BLOCK);
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
    DVLOG(1) << "getifaddrs() failed with errno = " << errno;
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr;
       interface != NULL;
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
std::unique_ptr<base::Value> NetLogProcTaskFailedCallback(
    uint32_t attempt_number,
    int net_error,
    int os_error,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  if (attempt_number)
    dict->SetInteger("attempt_number", attempt_number);

  dict->SetInteger("net_error", net_error);

  if (os_error) {
    dict->SetInteger("os_error", os_error);
#if defined(OS_WIN)
    // Map the error code to a human-readable string.
    LPWSTR error_string = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  0,  // Use the internal message table.
                  os_error,
                  0,  // Use default language.
                  (LPWSTR)&error_string,
                  0,  // Buffer size.
                  0);  // Arguments (unused).
    dict->SetString("os_error_string", base::WideToUTF8(error_string));
    LocalFree(error_string);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    dict->SetString("os_error_string", gai_strerror(os_error));
#endif
  }

  return std::move(dict);
}

// Creates NetLog parameters when the DnsTask failed.
std::unique_ptr<base::Value> NetLogDnsTaskFailedCallback(
    int net_error,
    int dns_error,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("net_error", net_error);
  if (dns_error)
    dict->SetInteger("dns_error", dns_error);
  return std::move(dict);
}

// Creates NetLog parameters containing the information in a RequestInfo object,
// along with the associated NetLogSource. Use NetLogRequestCallback() if the
// request information is not specified via RequestInfo.
std::unique_ptr<base::Value> NetLogRequestInfoCallback(
    const HostResolver::RequestInfo* info,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("host", info->host_port_pair().ToString());
  dict->SetInteger("address_family",
                   static_cast<int>(info->address_family()));
  dict->SetBoolean("allow_cached_response", info->allow_cached_response());
  dict->SetBoolean("is_speculative", info->is_speculative());
  return std::move(dict);
}

// Creates NetLog parameters containing the information of the request. Use
// NetLogRequestInfoCallback if the request is specified via RequestInfo.
std::unique_ptr<base::Value> NetLogRequestCallback(
    const HostPortPair& host,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();

  dict->SetString("host", host.ToString());
  dict->SetInteger("address_family",
                   static_cast<int>(ADDRESS_FAMILY_UNSPECIFIED));
  dict->SetBoolean("allow_cached_response", true);
  dict->SetBoolean("is_speculative", false);
  return std::move(dict);
}

// Creates NetLog parameters for the creation of a HostResolverImpl::Job.
std::unique_ptr<base::Value> NetLogJobCreationCallback(
    const NetLogSource& source,
    const std::string* host,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();
  source.AddToEventParameters(dict.get());
  dict->SetString("host", *host);
  return std::move(dict);
}

// Creates NetLog parameters for HOST_RESOLVER_IMPL_JOB_ATTACH/DETACH events.
std::unique_ptr<base::Value> NetLogJobAttachCallback(
    const NetLogSource& source,
    RequestPriority priority,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  source.AddToEventParameters(dict.get());
  dict->SetString("priority", RequestPriorityToString(priority));
  return std::move(dict);
}

// Creates NetLog parameters for the DNS_CONFIG_CHANGED event.
std::unique_ptr<base::Value> NetLogDnsConfigCallback(
    const DnsConfig* config,
    NetLogCaptureMode /* capture_mode */) {
  return config->ToValue();
}

std::unique_ptr<base::Value> NetLogIPv6AvailableCallback(
    bool ipv6_available,
    bool cached,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean("ipv6_available", ipv6_available);
  dict->SetBoolean("cached", cached);
  return std::move(dict);
}

// The logging routines are defined here because some requests are resolved
// without a Request object.

// Logs when a request has just been started. Overloads for whether or not the
// request information is specified via a RequestInfo object.
void LogStartRequest(const NetLogWithSource& source_net_log,
                     const HostResolver::RequestInfo& info) {
  source_net_log.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST,
                            base::Bind(&NetLogRequestInfoCallback, &info));
}
void LogStartRequest(const NetLogWithSource& source_net_log,
                     const HostPortPair& host) {
  source_net_log.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST,
                            base::BindRepeating(&NetLogRequestCallback, host));
}

// Logs when a request has just completed (before its callback is run).
void LogFinishRequest(const NetLogWithSource& source_net_log, int net_error) {
  source_net_log.EndEventWithNetErrorCode(
      NetLogEventType::HOST_RESOLVER_IMPL_REQUEST, net_error);
}

// Logs when a request has been cancelled.
void LogCancelRequest(const NetLogWithSource& source_net_log) {
  source_net_log.AddEvent(NetLogEventType::CANCELLED);
  source_net_log.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST);
}

//-----------------------------------------------------------------------------

// Keeps track of the highest priority.
class PriorityTracker {
 public:
  explicit PriorityTracker(RequestPriority initial_priority)
      : highest_priority_(initial_priority), total_count_(0) {
    memset(counts_, 0, sizeof(counts_));
  }

  RequestPriority highest_priority() const {
    return highest_priority_;
  }

  size_t total_count() const {
    return total_count_;
  }

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

void MakeNotStale(HostCache::EntryStaleness* stale_info) {
  if (!stale_info)
    return;
  stale_info->expired_by = base::TimeDelta::FromSeconds(-1);
  stale_info->network_changes = 0;
  stale_info->stale_hits = 0;
}

}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(base::StringPiece host,
                          uint16_t port,
                          AddressList* address_list) {
  address_list->clear();

  bool is_local6;
  if (!IsLocalHostname(host, &is_local6))
    return false;

  address_list->push_back(IPEndPoint(IPAddress::IPv6Localhost(), port));
  if (!is_local6) {
    address_list->push_back(IPEndPoint(IPAddress::IPv4Localhost(), port));
  }

  return true;
}

const unsigned HostResolverImpl::kMaximumDnsFailures = 16;

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
class HostResolverImpl::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public base::LinkNode<HostResolverImpl::RequestImpl> {
 public:
  RequestImpl(const NetLogWithSource& source_net_log,
              const HostPortPair& request_host,
              const base::Optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<HostResolverImpl> resolver)
      : source_net_log_(source_net_log),
        request_host_(request_host),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        host_resolver_flags_(ParametersToHostResolverFlags(parameters_)),
        priority_(parameters_.initial_priority),
        job_(nullptr),
        resolver_(resolver),
        complete_(false) {}

  ~RequestImpl() override;

  int Start(CompletionOnceCallback callback) override {
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK(!job_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      DCHECK(job_);
      callback_ = std::move(callback);
    } else {
      DCHECK(!job_);
      complete_ = true;
    }
    resolver_ = nullptr;

    return rv;
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    DCHECK(complete_);
    return address_results_;
  }

  void set_address_results(const AddressList& address_results) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!address_results_);
    DCHECK(!parameters_.is_speculative);

    address_results_ = address_results;
  }

  void ChangeRequestPriority(RequestPriority priority);

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
    DCHECK(!address_results_);
  }

  // Cleans up Job assignment, marks request completed, and calls the completion
  // callback.
  void OnJobCompleted(Job* job, int error) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  Job* job() const { return job_; }

  // NetLog for the source, passed in HostResolver::Resolve.
  const NetLogWithSource& source_net_log() { return source_net_log_; }

  const HostPortPair& request_host() const { return request_host_; }

  const ResolveHostParameters& parameters() const { return parameters_; }

  HostResolverFlags host_resolver_flags() const { return host_resolver_flags_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  bool complete() const { return complete_; }

  base::TimeTicks request_time() const {
    DCHECK(!request_time_.is_null());
    return request_time_;
  }
  void set_request_time(base::TimeTicks request_time) {
    DCHECK(request_time_.is_null());
    DCHECK(!request_time.is_null());
    request_time_ = request_time;
  }

 private:
  const NetLogWithSource source_net_log_;

  const HostPortPair request_host_;
  const ResolveHostParameters parameters_;
  const HostResolverFlags host_resolver_flags_;

  RequestPriority priority_;

  // The resolve job that this request is dependent on.
  Job* job_;
  base::WeakPtr<HostResolverImpl> resolver_;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_;
  base::Optional<AddressList> address_results_;

  base::TimeTicks request_time_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

// Wraps a RequestImpl to implement Request objects from the legacy Resolve()
// API. The wrapped request must not yet have been started.
//
// TODO(crbug.com/821021): Delete this class once all usage has been
// converted to the new CreateRequest() API.
class HostResolverImpl::LegacyRequestImpl : public HostResolver::Request {
 public:
  explicit LegacyRequestImpl(std::unique_ptr<RequestImpl> inner_request)
      : inner_request_(std::move(inner_request)) {
    DCHECK(!inner_request_->job());
    DCHECK(!inner_request_->complete());
  }

  ~LegacyRequestImpl() override {}

  void ChangeRequestPriority(RequestPriority priority) override {
    inner_request_->ChangeRequestPriority(priority);
  }

  int Start() {
    return inner_request_->Start(base::BindOnce(
        &LegacyRequestImpl::LegacyApiCallback, base::Unretained(this)));
  }

  // Do not call to assign the callback until we are running an async job (after
  // Start() returns ERR_IO_PENDING) and before completion.  Until then, the
  // legacy HostResolverImpl::Resolve() needs to hang onto |callback| to ensure
  // it stays alive for the duration of the method call, as some callers may be
  // binding objects, eg the AddressList, with the callback.
  void AssignCallback(CompletionOnceCallback callback,
                      AddressList* addresses_result_ptr) {
    DCHECK(callback);
    DCHECK(addresses_result_ptr);
    DCHECK(inner_request_->job());
    DCHECK(!inner_request_->complete());

    callback_ = std::move(callback);
    addresses_result_ptr_ = addresses_result_ptr;
  }

  const RequestImpl& inner_request() const { return *inner_request_; }

 private:
  // Result callback to bridge results handled entirely via ResolveHostRequest
  // to legacy API styles where AddressList was a separate method out parameter.
  void LegacyApiCallback(int error) {
    // Must call AssignCallback() before async results.
    DCHECK(callback_);

    if (error == OK && !inner_request_->parameters().is_speculative) {
      // Legacy API does not allow non-address results (eg TXT), so AddressList
      // is always expected to be present on OK.
      DCHECK(inner_request_->GetAddressResults());
      *addresses_result_ptr_ = inner_request_->GetAddressResults().value();
    }
    addresses_result_ptr_ = nullptr;
    std::move(callback_).Run(error);
  }

  const std::unique_ptr<RequestImpl> inner_request_;

  CompletionOnceCallback callback_;
  // This is a caller-provided pointer and should not be used once |callback_|
  // is invoked.
  AddressList* addresses_result_ptr_;

  DISALLOW_COPY_AND_ASSIGN(LegacyRequestImpl);
};

//------------------------------------------------------------------------------

// Calls HostResolverProc in TaskScheduler. Performs retries if necessary.
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
class HostResolverImpl::ProcTask {
 public:
  typedef base::OnceCallback<void(int net_error, const AddressList& addr_list)>
      Callback;

  ProcTask(const Key& key,
           const ProcTaskParams& params,
           Callback callback,
           scoped_refptr<base::TaskRunner> proc_task_runner,
           const NetLogWithSource& job_net_log,
           const base::TickClock* tick_clock)
      : key_(key),
        params_(params),
        callback_(std::move(callback)),
        network_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        proc_task_runner_(std::move(proc_task_runner)),
        attempt_number_(0),
        net_log_(job_net_log),
        tick_clock_(tick_clock),
        weak_ptr_factory_(this) {
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
        base::BindOnce(&ProcTask::DoLookup, key_, params_.resolver_proc,
                       network_task_runner_, std::move(completion_callback)));

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_STARTED,
                      NetLog::IntCallback("attempt_number", attempt_number_));

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

  // WARNING: This code runs in TaskScheduler with CONTINUE_ON_SHUTDOWN. The
  // shutdown code cannot wait for it to finish, so this code must be very
  // careful about using other objects (like MessageLoops, Singletons, etc).
  // During shutdown these objects may no longer exist.
  static void DoLookup(
      Key key,
      scoped_refptr<HostResolverProc> resolver_proc,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      AttemptCompletionCallback completion_callback) {
    AddressList results;
    int os_error = 0;
    int error =
        resolver_proc->Resolve(key.hostname, key.address_family,
                               key.host_resolver_flags, &results, &os_error);

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
    TRACE_EVENT0(kNetTracingCategory, "ProcTask::OnLookupComplete");

    // If results are empty, we should return an error.
    bool empty_list_on_ok = (error == OK && results.empty());
    if (empty_list_on_ok)
      error = ERR_NAME_NOT_RESOLVED;

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker threads.
    // So do it here on the IO thread instead.
    if (error != OK && NetworkChangeNotifier::IsOffline())
      error = ERR_INTERNET_DISCONNECTED;

    RecordAttemptHistograms(start_time, attempt_number, error, os_error,
                            tick_clock);

    if (!proc_task) {
      RecordDiscardedAttemptHistograms(attempt_number);
      return;
    }

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

    RecordTaskHistograms(start_time, error, os_error, attempt_number);

    NetLogParametersCallback net_log_callback;
    NetLogParametersCallback attempt_net_log_callback;
    if (error != OK) {
      net_log_callback = base::BindRepeating(&NetLogProcTaskFailedCallback, 0,
                                             error, os_error);
      attempt_net_log_callback = base::BindRepeating(
          &NetLogProcTaskFailedCallback, attempt_number, error, os_error);
    } else {
      net_log_callback = results.CreateNetLogCallback();
      attempt_net_log_callback =
          NetLog::IntCallback("attempt_number", attempt_number);
    }
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK,
                      net_log_callback);
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_FINISHED,
                      attempt_net_log_callback);

    std::move(callback_).Run(error, results);
  }

  void RecordTaskHistograms(const base::TimeTicks& start_time,
                            const int error,
                            const int os_error,
                            const uint32_t attempt_number) const {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (error == OK) {
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ProcTask.SuccessTime", duration);
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptFirstSuccess", attempt_number, 100);
    } else {
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ProcTask.FailureTime", duration);
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptFirstFailure", attempt_number, 100);
    }

    UMA_HISTOGRAM_CUSTOM_ENUMERATION(kOSErrorsForGetAddrinfoHistogramName,
                                     std::abs(os_error),
                                     GetAllGetAddrinfoOSErrors());
  }

  static void RecordAttemptHistograms(const base::TimeTicks& start_time,
                                      const uint32_t attempt_number,
                                      const int error,
                                      const int os_error,
                                      const base::TickClock* tick_clock) {
    base::TimeDelta duration = tick_clock->NowTicks() - start_time;
    if (error == OK) {
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptSuccess", attempt_number, 100);
      UMA_HISTOGRAM_LONG_TIMES_100("DNS.AttemptSuccessDuration", duration);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptFailure", attempt_number, 100);
      UMA_HISTOGRAM_LONG_TIMES_100("DNS.AttemptFailDuration", duration);
    }
  }

  static void RecordDiscardedAttemptHistograms(const uint32_t attempt_number) {
    // Count those attempts which completed after the job was already canceled
    // OR after the job was already completed by an earlier attempt (so
    // cancelled in effect).
    UMA_HISTOGRAM_ENUMERATION("DNS.AttemptDiscarded", attempt_number, 100);
  }

  Key key_;

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
  base::WeakPtrFactory<ProcTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProcTask);
};

//-----------------------------------------------------------------------------

// Resolves the hostname using DnsTransaction, which is a full implementation of
// a DNS stub resolver. One DnsTransaction is created for each resolution
// needed, which for AF_UNSPEC resolutions includes both A and AAAA. The
// transactions are scheduled separately and started separately.
//
// TODO(szym): This could be moved to separate source file as well.
class HostResolverImpl::DnsTask : public base::SupportsWeakPtr<DnsTask> {
 public:
  class Delegate {
   public:
    virtual void OnDnsTaskComplete(base::TimeTicks start_time,
                                   int net_error,
                                   const AddressList& addr_list,
                                   base::TimeDelta ttl) = 0;

    // Called when the first of two jobs succeeds.  If the first completed
    // transaction fails, this is not called.  Also not called when the DnsTask
    // only needs to run one transaction.
    virtual void OnFirstDnsTransactionComplete() = 0;

    virtual URLRequestContext* url_request_context() = 0;
    virtual RequestPriority priority() const = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  DnsTask(DnsClient* client,
          const Key& key,
          Delegate* delegate,
          const NetLogWithSource& job_net_log,
          const base::TickClock* tick_clock)
      : client_(client),
        key_(key),
        delegate_(delegate),
        net_log_(job_net_log),
        num_completed_transactions_(0),
        tick_clock_(tick_clock),
        task_start_time_(tick_clock_->NowTicks()) {
    DCHECK(client);
    DCHECK(delegate_);
  }

  bool needs_two_transactions() const {
    return key_.address_family == ADDRESS_FAMILY_UNSPECIFIED;
  }

  bool needs_another_transaction() const {
    return needs_two_transactions() && !transaction_aaaa_;
  }

  void StartFirstTransaction() {
    DCHECK_EQ(0u, num_completed_transactions_);
    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK);
    if (key_.address_family == ADDRESS_FAMILY_IPV6) {
      StartAAAA();
    } else {
      StartA();
    }
  }

  void StartSecondTransaction() {
    DCHECK(needs_two_transactions());
    StartAAAA();
  }

  base::TimeDelta ttl() { return ttl_; }

 private:
  void StartA() {
    DCHECK(!transaction_a_);
    DCHECK_NE(ADDRESS_FAMILY_IPV6, key_.address_family);
    transaction_a_ = CreateTransaction(ADDRESS_FAMILY_IPV4);
    transaction_a_->Start();
  }

  void StartAAAA() {
    DCHECK(!transaction_aaaa_);
    DCHECK_NE(ADDRESS_FAMILY_IPV4, key_.address_family);
    transaction_aaaa_ = CreateTransaction(ADDRESS_FAMILY_IPV6);
    transaction_aaaa_->Start();
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(AddressFamily family) {
    DCHECK_NE(ADDRESS_FAMILY_UNSPECIFIED, family);
    std::unique_ptr<DnsTransaction> trans =
        client_->GetTransactionFactory()->CreateTransaction(
            key_.hostname,
            family == ADDRESS_FAMILY_IPV6 ? dns_protocol::kTypeAAAA
                                          : dns_protocol::kTypeA,
            base::BindOnce(&DnsTask::OnTransactionComplete,
                           base::Unretained(this), tick_clock_->NowTicks()),
            net_log_);
    trans->SetRequestContext(delegate_->url_request_context());
    trans->SetRequestPriority(delegate_->priority());
    return trans;
  }

  void OnTransactionComplete(const base::TimeTicks& start_time,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response) {
    DCHECK(transaction);
    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                             response->IsValid())) {
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TransactionFailure", duration);
      OnFailure(net_error, DnsResponse::DNS_PARSE_OK);
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TransactionSuccess", duration);
    switch (transaction->GetType()) {
      case dns_protocol::kTypeA:
        UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TransactionSuccess_A", duration);
        break;
      case dns_protocol::kTypeAAAA:
        UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.TransactionSuccess_AAAA",
                                     duration);
        break;
    }

    AddressList addr_list;
    base::TimeDelta ttl;
    DnsResponse::Result result = response->ParseToAddressList(&addr_list, &ttl);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ParseToAddressList",
                              result,
                              DnsResponse::DNS_PARSE_RESULT_MAX);
    if (result != DnsResponse::DNS_PARSE_OK) {
      // Fail even if the other query succeeds.
      OnFailure(ERR_DNS_MALFORMED_RESPONSE, result);
      return;
    }

    ++num_completed_transactions_;
    if (num_completed_transactions_ == 1) {
      ttl_ = ttl;
    } else {
      ttl_ = std::min(ttl_, ttl);
    }

    if (transaction->GetType() == dns_protocol::kTypeA) {
      DCHECK_EQ(transaction_a_.get(), transaction);
      // Place IPv4 addresses after IPv6.
      addr_list_.insert(addr_list_.end(), addr_list.begin(), addr_list.end());
    } else {
      DCHECK_EQ(transaction_aaaa_.get(), transaction);
      // Place IPv6 addresses before IPv4.
      addr_list_.insert(addr_list_.begin(), addr_list.begin(), addr_list.end());
    }

    // If requested via HOST_RESOLVER_CANONNAME, store the canonical name from
    // the response. Prefer the name from the AAAA response. Only look at name
    // if there is at least one address record.
    if ((key_.host_resolver_flags & HOST_RESOLVER_CANONNAME) != 0 &&
        !addr_list.empty() &&
        (transaction->GetType() == dns_protocol::kTypeAAAA ||
         addr_list_.canonical_name().empty())) {
      addr_list_.set_canonical_name(addr_list.canonical_name());
    }

    if (needs_two_transactions() && num_completed_transactions_ == 1) {
      // No need to repeat the suffix search.
      key_.hostname = transaction->GetHostname();
      delegate_->OnFirstDnsTransactionComplete();
      return;
    }

    if (addr_list_.empty()) {
      // TODO(szym): Don't fallback to ProcTask in this case.
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK);
      return;
    }

    // If there are multiple addresses, and at least one is IPv6, need to sort
    // them.  Note that IPv6 addresses are always put before IPv4 ones, so it's
    // sufficient to just check the family of the first address.
    if (addr_list_.size() > 1 &&
        addr_list_[0].GetFamily() == ADDRESS_FAMILY_IPV6) {
      // Sort addresses if needed.  Sort could complete synchronously.
      client_->GetAddressSorter()->Sort(
          addr_list_, base::BindOnce(&DnsTask::OnSortComplete, AsWeakPtr(),
                                     tick_clock_->NowTicks()));
    } else {
      OnSuccess(addr_list_);
    }
  }

  void OnSortComplete(base::TimeTicks start_time,
                      bool success,
                      const AddressList& addr_list) {
    if (!success) {
      UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.SortFailure",
                                   tick_clock_->NowTicks() - start_time);
      OnFailure(ERR_DNS_SORT_ERROR, DnsResponse::DNS_PARSE_OK);
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.SortSuccess",
                                 tick_clock_->NowTicks() - start_time);

    // AddressSorter prunes unusable destinations.
    if (addr_list.empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK);
      return;
    }

    OnSuccess(addr_list);
  }

  void OnFailure(int net_error, DnsResponse::Result result) {
    DCHECK_NE(OK, net_error);
    net_log_.EndEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK,
        base::Bind(&NetLogDnsTaskFailedCallback, net_error, result));
    base::TimeDelta ttl = ttl_ < base::TimeDelta::FromSeconds(
                                     std::numeric_limits<uint32_t>::max()) &&
                                  num_completed_transactions_ > 0
                              ? ttl_
                              : base::TimeDelta::FromSeconds(0);
    delegate_->OnDnsTaskComplete(task_start_time_, net_error, AddressList(),
                                 ttl);
  }

  void OnSuccess(const AddressList& addr_list) {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK,
                      addr_list.CreateNetLogCallback());
    delegate_->OnDnsTaskComplete(task_start_time_, OK, addr_list, ttl_);
  }

  DnsClient* client_;
  Key key_;

  // The listener to the results of this DnsTask.
  Delegate* delegate_;
  const NetLogWithSource net_log_;

  std::unique_ptr<DnsTransaction> transaction_a_;
  std::unique_ptr<DnsTransaction> transaction_aaaa_;

  unsigned num_completed_transactions_;

  // These are updated as each transaction completes.
  base::TimeDelta ttl_;
  // IPv6 addresses must appear first in the list.
  AddressList addr_list_;

  const base::TickClock* tick_clock_;
  base::TimeTicks task_start_time_;

  DISALLOW_COPY_AND_ASSIGN(DnsTask);
};

//-----------------------------------------------------------------------------

// Aggregates all Requests for the same Key. Dispatched via PriorityDispatch.
class HostResolverImpl::Job : public PrioritizedDispatcher::Job,
                              public HostResolverImpl::DnsTask::Delegate {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(const base::WeakPtr<HostResolverImpl>& resolver,
      const Key& key,
      RequestPriority priority,
      scoped_refptr<base::TaskRunner> proc_task_runner,
      const NetLogWithSource& source_net_log,
      const base::TickClock* tick_clock)
      : resolver_(resolver),
        key_(key),
        priority_tracker_(priority),
        proc_task_runner_(std::move(proc_task_runner)),
        had_non_speculative_request_(false),
        num_occupied_job_slots_(0),
        dns_task_error_(OK),
        tick_clock_(tick_clock),
        creation_time_(tick_clock_->NowTicks()),
        priority_change_time_(creation_time_),
        net_log_(
            NetLogWithSource::Make(source_net_log.net_log(),
                                   NetLogSourceType::HOST_RESOLVER_IMPL_JOB)) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_CREATE_JOB);

    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                        base::Bind(&NetLogJobCreationCallback,
                                   source_net_log.source(), &key_.hostname));
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
      LogCancelRequest(req->source_net_log());
      req->OnJobCancelled(this);
    }
  }

  // Add this job to the dispatcher.  If "at_head" is true, adds at the front
  // of the queue.
  void Schedule(bool at_head) {
    DCHECK(!is_queued());
    PrioritizedDispatcher::Handle handle;
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
    DCHECK_EQ(key_.hostname, request->request_host().host());

    request->AssignJob(this);

    priority_tracker_.Add(request->priority());

    request->source_net_log().AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_ATTACH,
        net_log_.source().ToEventParametersCallback());

    net_log_.AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_ATTACH,
        base::Bind(&NetLogJobAttachCallback, request->source_net_log().source(),
                   priority()));

    if (!request->parameters().is_speculative)
      had_non_speculative_request_ = true;

    requests_.Append(request);

    UpdatePriority();
  }

  void ChangeRequestPriority(RequestImpl* req, RequestPriority priority) {
    DCHECK_EQ(key_.hostname, req->request_host().host());

    priority_tracker_.Remove(req->priority());
    req->set_priority(priority);
    priority_tracker_.Add(req->priority());
    UpdatePriority();
  }

  // Detach cancelled request. If it was the last active Request, also finishes
  // this Job.
  void CancelRequest(RequestImpl* request) {
    DCHECK_EQ(key_.hostname, request->request_host().host());
    DCHECK(!requests_.empty());

    LogCancelRequest(request->source_net_log());

    priority_tracker_.Remove(request->priority());
    net_log_.AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_DETACH,
        base::Bind(&NetLogJobAttachCallback, request->source_net_log().source(),
                   priority()));

    if (num_active_requests() > 0) {
      UpdatePriority();
      request->RemoveFromList();
    } else {
      // If we were called from a Request's callback within CompleteRequests,
      // that Request could not have been cancelled, so num_active_requests()
      // could not be 0. Therefore, we are not in CompleteRequests().
      CompleteRequestsWithError(OK /* cancelled */);
    }
  }

  // Called from AbortAllInProgressJobs. Completes all requests and destroys
  // the job. This currently assumes the abort is due to a network change.
  // TODO This should not delete |this|.
  void Abort() {
    DCHECK(is_running());
    CompleteRequestsWithError(ERR_NETWORK_CHANGED);
  }

  // If DnsTask present, abort it and fall back to ProcTask.
  void AbortDnsTask() {
    if (dns_task_) {
      KillDnsTask();
      dns_task_error_ = OK;
      StartProcTask();
    }
  }

  // Called by HostResolverImpl when this job is evicted due to queue overflow.
  // Completes all requests and destroys the job.
  void OnEvicted() {
    DCHECK(!is_running());
    DCHECK(is_queued());
    handle_.Reset();

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_EVICTED);

    // This signals to CompleteRequests that this job never ran.
    CompleteRequestsWithError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    AddressList addr_list;
    if (resolver_->ServeFromHosts(
            key(), requests_.head()->value()->request_host().port(),
            &addr_list)) {
      // This will destroy the Job.
      CompleteRequests(
          MakeCacheEntry(OK, addr_list, HostCache::Entry::SOURCE_HOSTS),
          base::TimeDelta(), true /* allow_cache */);
      return true;
    }
    return false;
  }

  const Key& key() const { return key_; }

  bool is_queued() const {
    return !handle_.is_null();
  }

  bool is_running() const {
    return is_dns_running() || is_mdns_running() || is_proc_running();
  }

 private:
  void KillDnsTask() {
    if (dns_task_) {
      ReduceToOneJobSlot();
      dns_task_.reset();
    }
  }

  // Reduce the number of job slots occupied and queued in the dispatcher
  // to one. If the second Job slot is queued in the dispatcher, cancels the
  // queued job. Otherwise, the second Job has been started by the
  // PrioritizedDispatcher, so signals it is complete.
  void ReduceToOneJobSlot() {
    DCHECK_GE(num_occupied_job_slots_, 1u);
    if (is_queued()) {
      resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    } else if (num_occupied_job_slots_ > 1) {
      resolver_->dispatcher_->OnJobFinished();
      --num_occupied_job_slots_;
    }
    DCHECK_EQ(1u, num_occupied_job_slots_);
  }

  // MakeCacheEntry() and MakeCacheEntryWithTTL() are helpers to build a
  // HostCache::Entry(). The address list is omited from the cache entry
  // for errors.
  HostCache::Entry MakeCacheEntry(int net_error,
                                  const AddressList& addr_list,
                                  HostCache::Entry::Source source) const {
    return HostCache::Entry(
        net_error,
        net_error == OK ? MakeAddressListForRequest(addr_list) : AddressList(),
        source);
  }

  HostCache::Entry MakeCacheEntryWithTTL(int net_error,
                                         const AddressList& addr_list,
                                         HostCache::Entry::Source source,
                                         base::TimeDelta ttl) const {
    return HostCache::Entry(
        net_error,
        net_error == OK ? MakeAddressListForRequest(addr_list) : AddressList(),
        source, ttl);
  }

  AddressList MakeAddressListForRequest(const AddressList& list) const {
    if (requests_.empty())
      return list;
    return AddressList::CopyWithPort(
        list, requests_.head()->value()->request_host().port());
  }

  void UpdatePriority() {
    if (is_queued()) {
      if (priority() != static_cast<RequestPriority>(handle_.priority()))
        priority_change_time_ = tick_clock_->NowTicks();
      handle_ = resolver_->dispatcher_->ChangePriority(handle_, priority());
    }
  }

  // PriorityDispatch::Job:
  void Start() override {
    DCHECK_LE(num_occupied_job_slots_, 1u);

    handle_.Reset();
    ++num_occupied_job_slots_;

    if (num_occupied_job_slots_ == 2) {
      StartSecondDnsTransaction();
      return;
    }

    DCHECK(!is_running());

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_STARTED);

    start_time_ = tick_clock_->NowTicks();
    base::TimeDelta queue_time = start_time_ - creation_time_;
    base::TimeDelta queue_time_after_change =
        start_time_ - priority_change_time_;

    DNS_HISTOGRAM_BY_PRIORITY("Net.DNS.JobQueueTime", priority(), queue_time);
    DNS_HISTOGRAM_BY_PRIORITY("Net.DNS.JobQueueTimeAfterChange", priority(),
                              queue_time_after_change);

    switch (key_.host_resolver_source) {
      case HostResolverSource::ANY:
        if (resolver_->HaveDnsConfig() &&
            !ResemblesMulticastDNSName(key_.hostname)) {
          StartDnsTask();
        } else {
          StartProcTask();
        }
        break;
      case HostResolverSource::SYSTEM:
        StartProcTask();
        break;
      case HostResolverSource::DNS:
        // DNS source should not be requested unless the resolver is configured
        // to handle it.
        DCHECK(resolver_->HaveDnsConfig());

        StartDnsTask();
        break;
      case HostResolverSource::MULTICAST_DNS:
        StartMdnsTask();
        break;
    }

    // Caution: Job::Start must not complete synchronously.
  }

  // TODO(szym): Since DnsTransaction does not consume threads, we can increase
  // the limits on |dispatcher_|. But in order to keep the number of
  // TaskScheduler threads low, we will need to use an "inner"
  // PrioritizedDispatcher with tighter limits.
  void StartProcTask() {
    DCHECK(!is_running());
    proc_task_ = std::make_unique<ProcTask>(
        key_, resolver_->proc_params_,
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
    DCHECK(is_proc_running());

    if (dns_task_error_ != OK) {
      base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
      if (net_error == OK) {
        UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.FallbackSuccess", duration);
        if ((dns_task_error_ == ERR_NAME_NOT_RESOLVED) &&
            ResemblesNetBIOSName(key_.hostname)) {
          UmaAsyncDnsResolveStatus(RESOLVE_STATUS_SUSPECT_NETBIOS);
        } else {
          UmaAsyncDnsResolveStatus(RESOLVE_STATUS_PROC_SUCCESS);
        }
        base::UmaHistogramSparse("Net.DNS.DnsTask.Errors",
                                 std::abs(dns_task_error_));
        resolver_->OnDnsTaskResolve(dns_task_error_);
      } else {
        UMA_HISTOGRAM_LONG_TIMES_100("AsyncDNS.FallbackFail", duration);
        UmaAsyncDnsResolveStatus(RESOLVE_STATUS_FAIL);
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
        MakeCacheEntry(net_error, addr_list, HostCache::Entry::SOURCE_UNKNOWN),
        ttl, true /* allow_cache */);
  }

  void StartDnsTask() {
    DCHECK(resolver_->HaveDnsConfig());
    dns_task_.reset(new DnsTask(resolver_->dns_client_.get(), key_, this,
                                net_log_, tick_clock_));

    dns_task_->StartFirstTransaction();
    // Schedule a second transaction, if needed.
    if (dns_task_->needs_two_transactions())
      Schedule(true);
  }

  void StartSecondDnsTransaction() {
    DCHECK(dns_task_->needs_two_transactions());
    dns_task_->StartSecondTransaction();
  }

  // Called if DnsTask fails. It is posted from StartDnsTask, so Job may be
  // deleted before this callback. In this case dns_task is deleted as well,
  // so we use it as indicator whether Job is still valid.
  void OnDnsTaskFailure(const base::WeakPtr<DnsTask>& dns_task,
                        base::TimeDelta duration,
                        int net_error) {
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.DnsTask.FailureTime", duration);

    if (!dns_task)
      return;

    if (duration < base::TimeDelta::FromMilliseconds(10)) {
      base::UmaHistogramSparse("Net.DNS.DnsTask.ErrorBeforeFallback.Fast",
                               std::abs(net_error));
    } else {
      base::UmaHistogramSparse("Net.DNS.DnsTask.ErrorBeforeFallback.Slow",
                               std::abs(net_error));
    }
    dns_task_error_ = net_error;

    // TODO(szym): Run ServeFromHosts now if nsswitch.conf says so.
    // http://crbug.com/117655

    // TODO(szym): Some net errors indicate lack of connectivity. Starting
    // ProcTask in that case is a waste of time.
    if (resolver_->fallback_to_proctask_) {
      KillDnsTask();
      StartProcTask();
    } else {
      UmaAsyncDnsResolveStatus(RESOLVE_STATUS_FAIL);
      // If the ttl is max, we didn't get one from the record, so set it to 0
      base::TimeDelta ttl =
          dns_task->ttl() < base::TimeDelta::FromSeconds(
                                std::numeric_limits<uint32_t>::max())
              ? dns_task->ttl()
              : base::TimeDelta::FromSeconds(0);
      CompleteRequests(
          HostCache::Entry(net_error, AddressList(),
                           HostCache::Entry::Source::SOURCE_UNKNOWN, ttl),
          ttl, true /* allow_cache */);
    }
  }

  // HostResolverImpl::DnsTask::Delegate implementation:

  void OnDnsTaskComplete(base::TimeTicks start_time,
                         int net_error,
                         const AddressList& addr_list,
                         base::TimeDelta ttl) override {
    DCHECK(is_dns_running());

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (net_error != OK) {
      OnDnsTaskFailure(dns_task_->AsWeakPtr(), duration, net_error);
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.DnsTask.SuccessTime", duration);

    UmaAsyncDnsResolveStatus(RESOLVE_STATUS_DNS_SUCCESS);
    RecordTTL(ttl);

    resolver_->OnDnsTaskResolve(OK);

    base::TimeDelta bounded_ttl =
        std::max(ttl, base::TimeDelta::FromSeconds(kMinimumTTLSeconds));

    if (ContainsIcannNameCollisionIp(addr_list)) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
    } else {
      CompleteRequests(MakeCacheEntryWithTTL(net_error, addr_list,
                                             HostCache::Entry::SOURCE_DNS, ttl),
                       bounded_ttl, true /* allow_cache */);
    }
  }

  void OnFirstDnsTransactionComplete() override {
    DCHECK(dns_task_->needs_two_transactions());
    DCHECK_EQ(dns_task_->needs_another_transaction(), is_queued());
    // No longer need to occupy two dispatcher slots.
    ReduceToOneJobSlot();

    // We already have a job slot at the dispatcher, so if the second
    // transaction hasn't started, reuse it now instead of waiting in the queue
    // for the second slot.
    if (dns_task_->needs_another_transaction())
      dns_task_->StartSecondTransaction();
  }

  void StartMdnsTask() {
    DCHECK(!is_running());

    // No flags are supported for MDNS except
    // HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6 (which is not actually an
    // input flag).
    DCHECK_EQ(0, key_.host_resolver_flags &
                     ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);

    std::vector<HostResolver::DnsQueryType> query_types;
    switch (key_.address_family) {
      case ADDRESS_FAMILY_UNSPECIFIED:
        query_types.push_back(HostResolver::DnsQueryType::A);
        query_types.push_back(HostResolver::DnsQueryType::AAAA);
        break;
      case ADDRESS_FAMILY_IPV4:
        query_types.push_back(HostResolver::DnsQueryType::A);
        break;
      case ADDRESS_FAMILY_IPV6:
        query_types.push_back(HostResolver::DnsQueryType::AAAA);
        break;
    }

    mdns_task_ = std::make_unique<HostResolverMdnsTask>(
        resolver_->GetOrCreateMdnsClient(), key_.hostname, query_types);
    mdns_task_->Start(
        base::BindOnce(&Job::OnMdnsTaskComplete, base::Unretained(this)));
  }

  void OnMdnsTaskComplete(int error) {
    DCHECK(is_mdns_running());
    // TODO(crbug.com/846423): Consider adding MDNS-specific logging.

    if (error != OK) {
      CompleteRequestsWithError(error);
    } else if (ContainsIcannNameCollisionIp(mdns_task_->result_addresses())) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
    } else {
      // MDNS uses a separate cache, so skip saving result to cache.
      // TODO(crbug.com/846423): Consider merging caches.
      CompleteRequestsWithoutCache(error, mdns_task_->result_addresses());
    }
  }

  URLRequestContext* url_request_context() override {
    return resolver_->url_request_context_;
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
        switch (key_.address_family) {
          case ADDRESS_FAMILY_IPV4:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.IPV4",
                                         duration);
            break;
          case ADDRESS_FAMILY_IPV6:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.IPV6",
                                         duration);
            break;
          case ADDRESS_FAMILY_UNSPECIFIED:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.UNSPEC",
                                         duration);
            break;
        }
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
        switch (key_.address_family) {
          case ADDRESS_FAMILY_IPV4:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.IPV4",
                                         duration);
            break;
          case ADDRESS_FAMILY_IPV6:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.IPV6",
                                         duration);
            break;
          case ADDRESS_FAMILY_UNSPECIFIED:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.UNSPEC",
                                         duration);
            break;
        }
      } else {
        category = RESOLVE_SPECULATIVE_FAIL;
      }
    }
    DCHECK_LT(static_cast<int>(category),
              static_cast<int>(RESOLVE_MAX));  // Be sure it was set.
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.ResolveCategory", category, RESOLVE_MAX);

    if (category == RESOLVE_FAIL || category == RESOLVE_ABORT) {
      if (duration < base::TimeDelta::FromMilliseconds(10))
        base::UmaHistogramSparse("Net.DNS.ResolveError.Fast", std::abs(error));
      else
        base::UmaHistogramSparse("Net.DNS.ResolveError.Slow", std::abs(error));
    }
  }

  // Performs Job's last rites. Completes all Requests. Deletes this.
  //
  // If not |allow_cache|, result will not be stored in the host cache, even if
  // result would otherwise allow doing so.
  void CompleteRequests(const HostCache::Entry& entry,
                        base::TimeDelta ttl,
                        bool allow_cache) {
    CHECK(resolver_.get());

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, if the job was owned by |jobs_|, the job
    // deletes itself when CompleteRequests is done.
    std::unique_ptr<Job> self_deleter = resolver_->RemoveJob(this);

    if (is_running()) {
      proc_task_ = nullptr;
      KillDnsTask();
      mdns_task_ = nullptr;

      // Signal dispatcher that a slot has opened.
      resolver_->dispatcher_->OnJobFinished();
    } else if (is_queued()) {
      resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    }

    if (num_active_requests() == 0) {
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                        OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                      entry.error());

    DCHECK(!requests_.empty());

    if (entry.error() == OK || entry.error() == ERR_ICANN_NAME_COLLISION) {
      // Record this histogram here, when we know the system has a valid DNS
      // configuration.
      UMA_HISTOGRAM_BOOLEAN("AsyncDNS.HaveDnsConfig",
                            resolver_->received_dns_config_);
    }

    bool did_complete = (entry.error() != ERR_NETWORK_CHANGED) &&
                        (entry.error() != ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
    if (did_complete && allow_cache)
      resolver_->CacheResult(key_, entry, ttl);

    RecordJobHistograms(entry.error());

    // Complete all of the requests that were attached to the job and
    // detach them.
    while (!requests_.empty()) {
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      DCHECK_EQ(this, req->job());
      // Update the net log and notify registered observers.
      LogFinishRequest(req->source_net_log(), entry.error());
      if (did_complete) {
        // Record effective total time from creation to completion.
        RecordTotalTime(req->parameters().is_speculative,
                        false /* from_cache */,
                        tick_clock_->NowTicks() - req->request_time());
      }
      if (entry.error() == OK && !req->parameters().is_speculative) {
        req->set_address_results(EnsurePortOnAddressList(
            entry.addresses(), req->request_host().port()));
      }
      req->OnJobCompleted(this, entry.error());

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_.get())
        return;
    }
  }

  void CompleteRequestsWithoutCache(int error, const AddressList& addresses) {
    CompleteRequests(
        MakeCacheEntry(error, addresses, HostCache::Entry::SOURCE_UNKNOWN),
        base::TimeDelta(), false /* allow_cache */);
  }

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error) {
    CompleteRequests(HostCache::Entry(net_error, AddressList(),
                                      HostCache::Entry::SOURCE_UNKNOWN),
                     base::TimeDelta(), true /* allow_cache */);
  }

  RequestPriority priority() const override {
    return priority_tracker_.highest_priority();
  }

  // Number of non-canceled requests in |requests_|.
  size_t num_active_requests() const {
    return priority_tracker_.total_count();
  }

  bool is_dns_running() const { return !!dns_task_; }

  bool is_mdns_running() const { return !!mdns_task_; }

  bool is_proc_running() const { return !!proc_task_; }

  base::WeakPtr<HostResolverImpl> resolver_;

  Key key_;

  // Tracks the highest priority across |requests_|.
  PriorityTracker priority_tracker_;

  // Task runner used for HostResolverProc.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  bool had_non_speculative_request_;

  // Number of slots occupied by this Job in resolver's PrioritizedDispatcher.
  unsigned num_occupied_job_slots_;

  // Result of DnsTask.
  int dns_task_error_;

  const base::TickClock* tick_clock_;
  const base::TimeTicks creation_time_;
  base::TimeTicks priority_change_time_;
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

  // A handle used in |HostResolverImpl::dispatcher_|.
  PrioritizedDispatcher::Handle handle_;
};

//-----------------------------------------------------------------------------

HostResolverImpl::ProcTaskParams::ProcTaskParams(
    HostResolverProc* resolver_proc,
    size_t max_retry_attempts)
    : resolver_proc(resolver_proc),
      max_retry_attempts(max_retry_attempts),
      unresponsive_delay(
          base::TimeDelta::FromMilliseconds(kDnsDefaultUnresponsiveDelayMs)),
      retry_factor(2) {
  // Maximum of 4 retry attempts for host resolution.
  static const size_t kDefaultMaxRetryAttempts = 4u;
  if (max_retry_attempts == HostResolver::kDefaultRetryAttempts)
    max_retry_attempts = kDefaultMaxRetryAttempts;
}

HostResolverImpl::ProcTaskParams::ProcTaskParams(const ProcTaskParams& other) =
    default;

HostResolverImpl::ProcTaskParams::~ProcTaskParams() = default;

HostResolverImpl::HostResolverImpl(const Options& options, NetLog* net_log)
    : max_queued_jobs_(0),
      proc_params_(NULL, options.max_retry_attempts),
      net_log_(net_log),
      received_dns_config_(false),
      num_dns_failures_(0),
      assume_ipv6_failure_on_wifi_(false),
      use_local_ipv6_(false),
      last_ipv6_probe_result_(true),
      additional_resolver_flags_(0),
      fallback_to_proctask_(true),
      url_request_context_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this),
      probe_weak_ptr_factory_(this) {
  if (options.enable_caching)
    cache_ = HostCache::CreateDefaultCache();

  PrioritizedDispatcher::Limits job_limits = options.GetDispatcherLimits();
  dispatcher_.reset(new PrioritizedDispatcher(job_limits));
  max_queued_jobs_ = job_limits.total_jobs * 100u;

  DCHECK_GE(dispatcher_->num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

  proc_task_runner_ = base::CreateTaskRunnerWithTraits(
      {base::MayBlock(), priority_mode.Get(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
#if (defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD) && \
    !defined(OS_ANDROID)
  EnsureDnsReloaderInit();
#endif

  OnConnectionTypeChanged(NetworkChangeNotifier::GetConnectionType());

  {
    DnsConfig dns_config = GetBaseDnsConfig();
    received_dns_config_ = dns_config.IsValid();
    // Conservatively assume local IPv6 is needed when DnsConfig is not valid.
    use_local_ipv6_ = !dns_config.IsValid() || dns_config.use_local_ipv6;
  }

  fallback_to_proctask_ = !ConfigureAsyncDnsNoFallbackFieldTrial();
}

HostResolverImpl::~HostResolverImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Prevent the dispatcher from starting new jobs.
  dispatcher_->SetLimitsToZero();
  // It's now safe for Jobs to call KillDnsTask on destruction, because
  // OnJobComplete will not start any new jobs.
  jobs_.clear();

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  NetworkChangeNotifier::RemoveDNSObserver(this);
}

void HostResolverImpl::SetDnsClient(std::unique_ptr<DnsClient> dns_client) {
  // DnsClient and config must be updated before aborting DnsTasks, since doing
  // so may start new jobs.
  dns_client_ = std::move(dns_client);
  if (dns_client_ && !dns_client_->GetConfig() &&
      num_dns_failures_ < kMaximumDnsFailures) {
    DnsConfig dns_config = GetBaseDnsConfig();
    DnsConfig overridden_config =
        dns_config_overrides_.ApplyOverrides(dns_config);
    dns_client_->SetConfig(overridden_config);
    num_dns_failures_ = 0;
    if (dns_client_->GetConfig())
      UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", true);
  }

  AbortDnsTasks();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverImpl::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(net_log, host, optional_parameters,
                                       weak_ptr_factory_.GetWeakPtr());
}

int HostResolverImpl::Resolve(const RequestInfo& info,
                              RequestPriority priority,
                              AddressList* addresses,
                              CompletionOnceCallback callback,
                              std::unique_ptr<Request>* out_req,
                              const NetLogWithSource& source_net_log) {
  DCHECK(addresses);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);
  DCHECK(out_req);

  auto request = std::make_unique<RequestImpl>(
      source_net_log, info.host_port_pair(),
      RequestInfoToResolveHostParameters(info, priority),
      weak_ptr_factory_.GetWeakPtr());
  auto wrapped_request =
      std::make_unique<LegacyRequestImpl>(std::move(request));

  int rv = wrapped_request->Start();

  if (rv == OK && !info.is_speculative()) {
    DCHECK(wrapped_request->inner_request().GetAddressResults());
    *addresses = wrapped_request->inner_request().GetAddressResults().value();
  } else if (rv == ERR_IO_PENDING) {
    wrapped_request->AssignCallback(std::move(callback), addresses);
    *out_req = std::move(wrapped_request);
  }

  return rv;
}

int HostResolverImpl::ResolveFromCache(const RequestInfo& info,
                                       AddressList* addresses,
                                       const NetLogWithSource& source_net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(addresses);

  // Update the net log and notify registered observers.
  LogStartRequest(source_net_log, info);

  Key key;
  int rv = ResolveLocally(
      info.host_port_pair(), AddressFamilyToDnsQueryType(info.address_family()),
      FlagsToSource(info.host_resolver_flags()), info.host_resolver_flags(),
      info.allow_cached_response(), false /* allow_stale */,
      nullptr /* stale_info */, source_net_log, addresses, &key);

  LogFinishRequest(source_net_log, rv);
  return rv;
}

int HostResolverImpl::ResolveStaleFromCache(
    const RequestInfo& info,
    AddressList* addresses,
    HostCache::EntryStaleness* stale_info,
    const NetLogWithSource& source_net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(addresses);
  DCHECK(stale_info);

  // Update the net log and notify registered observers.
  LogStartRequest(source_net_log, info);

  Key key;
  int rv = ResolveLocally(
      info.host_port_pair(), AddressFamilyToDnsQueryType(info.address_family()),
      FlagsToSource(info.host_resolver_flags()), info.host_resolver_flags(),
      info.allow_cached_response(), true /* allow_stale */, stale_info,
      source_net_log, addresses, &key);
  LogFinishRequest(source_net_log, rv);
  return rv;
}

void HostResolverImpl::SetDnsClientEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(ENABLE_BUILT_IN_DNS)
  if (enabled && !dns_client_) {
    SetDnsClient(DnsClient::CreateClient(net_log_));
  } else if (!enabled && dns_client_) {
    SetDnsClient(std::unique_ptr<DnsClient>());
  }
#endif
}

HostCache* HostResolverImpl::GetHostCache() {
  return cache_.get();
}

bool HostResolverImpl::HasCached(base::StringPiece hostname,
                                 HostCache::Entry::Source* source_out,
                                 HostCache::EntryStaleness* stale_out) const {
  if (!cache_)
    return false;

  return cache_->HasEntry(hostname, source_out, stale_out);
}

std::unique_ptr<base::Value> HostResolverImpl::GetDnsConfigAsValue() const {
  // Check if async DNS is disabled.
  if (!dns_client_.get())
    return nullptr;

  // Check if async DNS is enabled, but we currently have no configuration
  // for it.
  const DnsConfig* dns_config = dns_client_->GetConfig();
  if (!dns_config)
    return std::make_unique<base::DictionaryValue>();

  return dns_config->ToValue();
}

size_t HostResolverImpl::LastRestoredCacheSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return cache_ ? cache_->last_restore_size() : 0;
}

size_t HostResolverImpl::CacheSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return cache_ ? cache_->size() : 0;
}

void HostResolverImpl::SetNoIPv6OnWifi(bool no_ipv6_on_wifi) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  assume_ipv6_failure_on_wifi_ = no_ipv6_on_wifi;
}

bool HostResolverImpl::GetNoIPv6OnWifi() {
  return assume_ipv6_failure_on_wifi_;
}

void HostResolverImpl::SetDnsConfigOverrides(
    const DnsConfigOverrides& overrides) {
  if (dns_config_overrides_ == overrides)
    return;

  dns_config_overrides_ = overrides;
  if (dns_client_.get() && dns_client_->GetConfig())
    UpdateDNSConfig(true);
}

void HostResolverImpl::SetRequestContext(URLRequestContext* context) {
  if (context != url_request_context_) {
    url_request_context_ = context;
  }
}

const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
HostResolverImpl::GetDnsOverHttpsServersForTesting() const {
  if (!dns_config_overrides_.dns_over_https_servers ||
      dns_config_overrides_.dns_over_https_servers.value().empty()) {
    return nullptr;
  }
  return &dns_config_overrides_.dns_over_https_servers.value();
}

void HostResolverImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  cache_->set_tick_clock_for_testing(tick_clock);
}

void HostResolverImpl::SetMaxQueuedJobsForTesting(size_t value) {
  DCHECK_EQ(0u, dispatcher_->num_queued_jobs());
  DCHECK_GE(value, 0u);
  max_queued_jobs_ = value;
}

void HostResolverImpl::SetHaveOnlyLoopbackAddresses(bool result) {
  if (result) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
}

void HostResolverImpl::SetMdnsSocketFactoryForTesting(
    std::unique_ptr<MDnsSocketFactory> socket_factory) {
  DCHECK(!mdns_client_);
  mdns_socket_factory_ = std::move(socket_factory);
}

void HostResolverImpl::SetMdnsClientForTesting(
    std::unique_ptr<MDnsClient> client) {
  mdns_client_ = std::move(client);
}

void HostResolverImpl::SetBaseDnsConfigForTesting(
    const DnsConfig& base_config) {
  test_base_config_ = base_config;
  UpdateDNSConfig(true);
}

void HostResolverImpl::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  proc_task_runner_ = std::move(task_runner);
}

int HostResolverImpl::Resolve(RequestImpl* request) {
  // Request should not yet have a scheduled Job.
  DCHECK(!request->job());
  // Request may only be resolved once.
  DCHECK(!request->complete());
  // MDNS requests do not support skipping cache.
  // TODO(crbug.com/846423): Either add support for skipping the MDNS cache, or
  // merge to use the normal host cache for MDNS requests.
  DCHECK(request->parameters().source != HostResolverSource::MULTICAST_DNS ||
         request->parameters().allow_cached_response);

  request->set_request_time(tick_clock_->NowTicks());

  LogStartRequest(request->source_net_log(), request->request_host());

  AddressList addresses;
  Key key;
  int rv = ResolveLocally(
      request->request_host(), request->parameters().dns_query_type,
      request->parameters().source, request->host_resolver_flags(),
      request->parameters().allow_cached_response, false /* allow_stale */,
      nullptr /* stale_info */, request->source_net_log(), &addresses, &key);
  if (rv == OK && !request->parameters().is_speculative) {
    request->set_address_results(
        EnsurePortOnAddressList(addresses, request->request_host().port()));
  }
  if (rv != ERR_DNS_CACHE_MISS) {
    LogFinishRequest(request->source_net_log(), rv);
    RecordTotalTime(request->parameters().is_speculative, true /* from_cache */,
                    base::TimeDelta());
    return rv;
  }

  rv = CreateAndStartJob(key, request);
  // At this point, expect only async or errors.
  DCHECK_NE(OK, rv);

  return rv;
}

int HostResolverImpl::ResolveLocally(const HostPortPair& host,
                                     DnsQueryType dns_query_type,
                                     HostResolverSource source,
                                     HostResolverFlags flags,
                                     bool allow_cache,
                                     bool allow_stale,
                                     HostCache::EntryStaleness* stale_info,
                                     const NetLogWithSource& source_net_log,
                                     AddressList* addresses,
                                     Key* key) {
  IPAddress ip_address;
  IPAddress* ip_address_ptr = nullptr;
  if (ip_address.AssignFromIPLiteral(host.host())) {
    ip_address_ptr = &ip_address;
  } else {
    // Check that the caller supplied a valid hostname to resolve.
    if (!IsValidDNSDomain(host.host()))
      return ERR_NAME_NOT_RESOLVED;
  }

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  *key = GetEffectiveKeyForRequest(host.host(), dns_query_type, source, flags,
                                   ip_address_ptr, source_net_log);

  DCHECK(allow_stale == !!stale_info);
  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (host.host().empty() || host.host().size() > kMaxHostLength) {
    MakeNotStale(stale_info);
    return ERR_NAME_NOT_RESOLVED;
  }

  int net_error = ERR_UNEXPECTED;
  if (ResolveAsIP(*key, host.port(), ip_address_ptr, &net_error, addresses)) {
    MakeNotStale(stale_info);
    return net_error;
  }

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  if (ServeLocalhost(*key, host.port(), addresses)) {
    MakeNotStale(stale_info);
    return OK;
  }

  if (allow_cache && ServeFromCache(*key, host.port(), &net_error, addresses,
                                    allow_stale, stale_info)) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_CACHE_HIT,
                            addresses->CreateNetLogCallback());
    // |ServeFromCache()| will set |*stale_info| as needed.
    return net_error;
  }

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  if (ServeFromHosts(*key, host.port(), addresses)) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_HOSTS_HIT,
                            addresses->CreateNetLogCallback());
    MakeNotStale(stale_info);
    return OK;
  }

  return ERR_DNS_CACHE_MISS;
}

int HostResolverImpl::CreateAndStartJob(const Key& key, RequestImpl* request) {
  auto jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    auto new_job = std::make_unique<Job>(
        weak_ptr_factory_.GetWeakPtr(), key, request->priority(),
        proc_task_runner_, request->source_net_log(), tick_clock_);
    job = new_job.get();
    new_job->Schedule(false);

    // Check for queue overflow.
    if (dispatcher_->num_queued_jobs() > max_queued_jobs_) {
      Job* evicted = static_cast<Job*>(dispatcher_->EvictOldestLowest());
      DCHECK(evicted);
      evicted->OnEvicted();
      if (evicted == new_job.get()) {
        LogFinishRequest(request->source_net_log(),
                         ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
        return ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;
      }
    }
    jobs_[key] = std::move(new_job);
  } else {
    job = jobit->second.get();
  }

  // Can't complete synchronously. Attach request and job to each other.
  job->AddRequest(request);
  return ERR_IO_PENDING;
}

bool HostResolverImpl::ResolveAsIP(const Key& key,
                                   uint16_t host_port,
                                   const IPAddress* ip_address,
                                   int* net_error,
                                   AddressList* addresses) {
  DCHECK(addresses);
  DCHECK(net_error);
  if (ip_address == nullptr)
    return false;

  *net_error = OK;
  AddressFamily family = GetAddressFamily(*ip_address);
  if (key.address_family != ADDRESS_FAMILY_UNSPECIFIED &&
      key.address_family != family) {
    // Don't return IPv6 addresses for IPv4 queries, and vice versa.
    *net_error = ERR_NAME_NOT_RESOLVED;
  } else {
    *addresses = AddressList::CreateFromIPAddress(*ip_address, host_port);
    if (key.host_resolver_flags & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
  }
  return true;
}

bool HostResolverImpl::ServeFromCache(const Key& key,
                                      uint16_t host_port,
                                      int* net_error,
                                      AddressList* addresses,
                                      bool allow_stale,
                                      HostCache::EntryStaleness* stale_info) {
  DCHECK(addresses);
  DCHECK(net_error);
  DCHECK(allow_stale == !!stale_info);
  if (!cache_.get())
    return false;

  const HostCache::Entry* cache_entry;
  if (allow_stale)
    cache_entry = cache_->LookupStale(key, tick_clock_->NowTicks(), stale_info);
  else
    cache_entry = cache_->Lookup(key, tick_clock_->NowTicks());
  if (!cache_entry)
    return false;

  *net_error = cache_entry->error();
  if (*net_error == OK) {
    if (cache_entry->has_ttl())
      RecordTTL(cache_entry->ttl());
    *addresses = EnsurePortOnAddressList(cache_entry->addresses(), host_port);
  }
  return true;
}

bool HostResolverImpl::ServeFromHosts(const Key& key,
                                      uint16_t host_port,
                                      AddressList* addresses) {
  DCHECK(addresses);
  if (!HaveDnsConfig())
    return false;
  addresses->clear();

  // HOSTS lookups are case-insensitive.
  std::string hostname = base::ToLowerASCII(key.hostname);

  const DnsHosts& hosts = dns_client_->GetConfig()->hosts;

  // If |address_family| is ADDRESS_FAMILY_UNSPECIFIED other implementations
  // (glibc and c-ares) return the first matching line. We have more
  // flexibility, but lose implicit ordering.
  // We prefer IPv6 because "happy eyeballs" will fall back to IPv4 if
  // necessary.
  if (key.address_family == ADDRESS_FAMILY_IPV6 ||
      key.address_family == ADDRESS_FAMILY_UNSPECIFIED) {
    auto it = hosts.find(DnsHostsKey(hostname, ADDRESS_FAMILY_IPV6));
    if (it != hosts.end())
      addresses->push_back(IPEndPoint(it->second, host_port));
  }

  if (key.address_family == ADDRESS_FAMILY_IPV4 ||
      key.address_family == ADDRESS_FAMILY_UNSPECIFIED) {
    auto it = hosts.find(DnsHostsKey(hostname, ADDRESS_FAMILY_IPV4));
    if (it != hosts.end())
      addresses->push_back(IPEndPoint(it->second, host_port));
  }

  // If got only loopback addresses and the family was restricted, resolve
  // again, without restrictions. See SystemHostResolverCall for rationale.
  if ((key.host_resolver_flags &
          HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) &&
      IsAllIPv4Loopback(*addresses)) {
    Key new_key(key);
    new_key.address_family = ADDRESS_FAMILY_UNSPECIFIED;
    new_key.host_resolver_flags &=
        ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    return ServeFromHosts(new_key, host_port, addresses);
  }
  return !addresses->empty();
}

bool HostResolverImpl::ServeLocalhost(const Key& key,
                                      uint16_t host_port,
                                      AddressList* addresses) {
  AddressList resolved_addresses;
  if (!ResolveLocalHostname(key.hostname, host_port, &resolved_addresses))
    return false;

  addresses->clear();

  for (const auto& address : resolved_addresses) {
    // Include the address if:
    // - caller didn't specify an address family, or
    // - caller specifically asked for the address family of this address, or
    // - this is an IPv6 address and caller specifically asked for IPv4 due
    //   to lack of detected IPv6 support. (See SystemHostResolverCall for
    //   rationale).
    if (key.address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        key.address_family == address.GetFamily() ||
        (address.GetFamily() == ADDRESS_FAMILY_IPV6 &&
         key.address_family == ADDRESS_FAMILY_IPV4 &&
         (key.host_resolver_flags &
          HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6))) {
      addresses->push_back(address);
    }
  }

  return true;
}

void HostResolverImpl::CacheResult(const Key& key,
                                   const HostCache::Entry& entry,
                                   base::TimeDelta ttl) {
  // Don't cache an error unless it has a positive TTL.
  if (cache_.get() && (entry.error() == OK || ttl > base::TimeDelta()))
    cache_->Set(key, entry, tick_clock_->NowTicks(), ttl);
}

std::unique_ptr<HostResolverImpl::Job> HostResolverImpl::RemoveJob(Job* job) {
  DCHECK(job);
  std::unique_ptr<Job> retval;
  auto it = jobs_.find(job->key());
  if (it != jobs_.end() && it->second.get() == job) {
    it->second.swap(retval);
    jobs_.erase(it);
  }
  return retval;
}

HostResolverImpl::Key HostResolverImpl::GetEffectiveKeyForRequest(
    const std::string& hostname,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    const IPAddress* ip_address,
    const NetLogWithSource& net_log) {
  HostResolverFlags effective_flags = flags | additional_resolver_flags_;

  AddressFamily effective_address_family =
      DnsQueryTypeToAddressFamily(dns_query_type);

  if (effective_address_family == ADDRESS_FAMILY_UNSPECIFIED &&
      // When resolving IPv4 literals, there's no need to probe for IPv6.
      // When resolving IPv6 literals, there's no benefit to artificially
      // limiting our resolution based on a probe.  Prior logic ensures
      // that this query is UNSPECIFIED (see effective_address_family
      // check above) so the code requesting the resolution should be amenable
      // to receiving a IPv6 resolution.
      !use_local_ipv6_ && ip_address == nullptr && !IsIPv6Reachable(net_log)) {
    effective_address_family = ADDRESS_FAMILY_IPV4;
    effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  }

  return Key(hostname, effective_address_family, effective_flags, source);
}

bool HostResolverImpl::IsIPv6Reachable(const NetLogWithSource& net_log) {
  // Don't bother checking if the device is on WiFi and IPv6 is assumed to not
  // work on WiFi.
  if (assume_ipv6_failure_on_wifi_ &&
      NetworkChangeNotifier::GetConnectionType() ==
          NetworkChangeNotifier::CONNECTION_WIFI) {
    return false;
  }

  // Cache the result for kIPv6ProbePeriodMs (measured from after
  // IsGloballyReachable() completes).
  bool cached = true;
  if ((tick_clock_->NowTicks() - last_ipv6_probe_time_).InMilliseconds() >
      kIPv6ProbePeriodMs) {
    last_ipv6_probe_result_ =
        IsGloballyReachable(IPAddress(kIPv6ProbeAddress), net_log);
    last_ipv6_probe_time_ = tick_clock_->NowTicks();
    cached = false;
  }
  net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_IPV6_REACHABILITY_CHECK,
                   base::Bind(&NetLogIPv6AvailableCallback,
                              last_ipv6_probe_result_, cached));
  return last_ipv6_probe_result_;
}

bool HostResolverImpl::IsGloballyReachable(const IPAddress& dest,
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

void HostResolverImpl::RunLoopbackProbeJob() {
  // Run this asynchronously as it can take 40-100ms and should not block
  // initialization.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HaveOnlyLoopbackAddresses),
      base::BindOnce(&HostResolverImpl::SetHaveOnlyLoopbackAddresses,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HostResolverImpl::AbortAllInProgressJobs() {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from |jobs_|.
  std::vector<std::unique_ptr<Job>> jobs_to_abort;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    Job* job = it->second.get();
    if (job->is_running()) {
      jobs_to_abort.push_back(std::move(it->second));
      jobs_.erase(it++);
    } else {
      DCHECK(job->is_queued());
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
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  // Then Abort them.
  for (size_t i = 0; self.get() && i < jobs_to_abort.size(); ++i) {
    jobs_to_abort[i]->Abort();
  }

  if (self)
    dispatcher_->SetLimits(limits);
}

void HostResolverImpl::AbortDnsTasks() {
  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job if the DnsConfig just changed.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->AbortDnsTask();
  dispatcher_->SetLimits(limits);
}

void HostResolverImpl::TryServingAllJobsFromHosts() {
  if (!HaveDnsConfig())
    return;

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  for (auto it = jobs_.begin(); self.get() && it != jobs_.end();) {
    Job* job = it->second.get();
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
  }
}

void HostResolverImpl::OnIPAddressChanged() {
  last_ipv6_probe_time_ = base::TimeTicks();
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  if (cache_.get())
    cache_->OnNetworkChange();
#if (defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

void HostResolverImpl::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  proc_params_.unresponsive_delay =
      GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
          "DnsUnresponsiveDelayMsByConnectionType",
          base::TimeDelta::FromMilliseconds(kDnsDefaultUnresponsiveDelayMs),
          type);
}

void HostResolverImpl::OnInitialDNSConfigRead() {
  UpdateDNSConfig(false);
}

void HostResolverImpl::OnDNSChanged() {
  // Ignore changes if we're using a test config.
  if (test_base_config_)
    return;

  UpdateDNSConfig(true);
}

DnsConfig HostResolverImpl::GetBaseDnsConfig() const {
  DnsConfig dns_config;
  if (test_base_config_)
    dns_config = test_base_config_.value();
  else
    NetworkChangeNotifier::GetDnsConfig(&dns_config);
  return dns_config;
}

void HostResolverImpl::UpdateDNSConfig(bool config_changed) {
  DnsConfig dns_config = GetBaseDnsConfig();

  if (net_log_) {
    net_log_->AddGlobalEntry(NetLogEventType::DNS_CONFIG_CHANGED,
                             base::Bind(&NetLogDnsConfigCallback, &dns_config));
  }

  // TODO(szym): Remove once http://crbug.com/137914 is resolved.
  received_dns_config_ = dns_config.IsValid();

  dns_config = dns_config_overrides_.ApplyOverrides(dns_config);

  // Conservatively assume local IPv6 is needed when DnsConfig is not valid.
  use_local_ipv6_ = !dns_config.IsValid() || dns_config.use_local_ipv6;

  num_dns_failures_ = 0;

  // We want a new DnsSession in place, before we Abort running Jobs, so that
  // the newly started jobs use the new config.
  if (dns_client_.get()) {
    // Make sure that if the update is an initial read, not a change, there
    // wasn't already a DnsConfig or it's the same one.
    DCHECK(config_changed || !dns_client_->GetConfig() ||
           dns_client_->GetConfig()->Equals(dns_config));
    dns_client_->SetConfig(dns_config);
    if (dns_client_->GetConfig())
      UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", true);
  }

  if (config_changed) {
    // If the DNS server has changed, existing cached info could be wrong so we
    // have to expire our internal cache :( Note that OS level DNS caches, such
    // as NSCD's cache should be dropped automatically by the OS when
    // resolv.conf changes so we don't need to do anything to clear that cache.
    if (cache_.get())
      cache_->OnNetworkChange();

    // Life check to bail once |this| is deleted.
    base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

    // Existing jobs will have been sent to the original server so they need to
    // be aborted.
    AbortAllInProgressJobs();

    // |this| may be deleted inside AbortAllInProgressJobs().
    if (self.get())
      TryServingAllJobsFromHosts();
  }
}

bool HostResolverImpl::HaveDnsConfig() const {
  // Use DnsClient only if it's fully configured and there is no override by
  // ScopedDefaultHostResolverProc.
  // The alternative is to use NetworkChangeNotifier to override DnsConfig,
  // but that would introduce construction order requirements for NCN and SDHRP.
  return dns_client_ && dns_client_->GetConfig() &&
         (proc_params_.resolver_proc || !HostResolverProc::GetDefault());
}

void HostResolverImpl::OnDnsTaskResolve(int net_error) {
  DCHECK(dns_client_);
  if (net_error == OK) {
    num_dns_failures_ = 0;
    return;
  }
  ++num_dns_failures_;
  if (num_dns_failures_ < kMaximumDnsFailures)
    return;

  // Disable DnsClient until the next DNS change.  Must be done before aborting
  // DnsTasks, since doing so may start new jobs.
  dns_client_->SetConfig(DnsConfig());

  // Switch jobs with active DnsTasks over to using ProcTasks.
  AbortDnsTasks();

  UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", false);
  base::UmaHistogramSparse("AsyncDNS.DnsClientDisabledReason",
                           std::abs(net_error));
}

MDnsClient* HostResolverImpl::GetOrCreateMdnsClient() {
#if BUILDFLAG(ENABLE_MDNS)
  if (!mdns_client_) {
    if (!mdns_socket_factory_)
      mdns_socket_factory_ = std::make_unique<MDnsSocketFactoryImpl>(net_log_);

    mdns_client_ = MDnsClient::CreateDefault();
    mdns_client_->StartListening(mdns_socket_factory_.get());
  }

  DCHECK(mdns_client_->IsListening());
  return mdns_client_.get();
#else
  // Should not request MDNS resoltuion unless MDNS is enabled.
  NOTREACHED();
  return nullptr;
#endif
}

HostResolverImpl::RequestImpl::~RequestImpl() {
  if (job_)
    job_->CancelRequest(this);
}

void HostResolverImpl::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  job_->ChangeRequestPriority(this, priority);
}

}  // namespace net

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_SYSTEM_TASK_H_
#define NET_DNS_HOST_RESOLVER_SYSTEM_TASK_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/task/task_runner.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_query_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

class HostResolverCache;

using SystemDnsResultsCallback = base::OnceCallback<
    void(const AddressList& addr_list, int os_error, int net_error)>;

// Calls SystemHostResolverCall() (or in some tests, HostResolverProc::Resolve)
// in ThreadPool. So EnsureSystemHostResolverCallReady() must be called before
// using this class.
//
// Performs retries if specified by HostResolverSystemTask::Params.
//
// Whenever we try to resolve the host, we post a delayed task to check if host
// resolution (OnLookupComplete) is completed or not. If the original attempt
// hasn't completed, then we start another attempt for host resolution. We take
// the results from the first attempt that finishes and ignore the results from
// all other attempts.
//
// This class is designed to be used not just by HostResolverManager, but by
// general consumers.
//
// It should only be used on the main thread to ensure that hooks (see
// SetSystemHostResolverOverride()) only ever run on the main thread.
class NET_EXPORT HostResolverSystemTask {
 public:
  // Parameters for customizing HostResolverSystemTask behavior.
  //
  // |resolver_proc| is used to override resolution in tests; it must be
  // thread-safe since it may be run from multiple worker threads. If
  // |resolver_proc| is NULL then the default host resolver procedure is
  // to call SystemHostResolverCall().
  //
  // For each attempt, we could start another attempt if host is not resolved
  // within |unresponsive_delay| time. We keep attempting to resolve the host
  // for |max_retry_attempts|. For every retry attempt, we grow the
  // |unresponsive_delay| by the |retry_factor| amount (that is retry interval
  // is multiplied by the retry factor each time). Once we have retried
  // |max_retry_attempts|, we give up on additional attempts.
  struct NET_EXPORT_PRIVATE Params {
    // Default delay between calls to the system resolver for the same hostname.
    // (Can be overridden by field trial.)
    static constexpr base::TimeDelta kDnsDefaultUnresponsiveDelay =
        base::Seconds(6);

    // Set |max_system_retry_attempts| to this to select a default retry value.
    static constexpr size_t kDefaultRetryAttempts = -1;

    // Sets up defaults.
    Params(scoped_refptr<HostResolverProc> resolver_proc,
           size_t max_retry_attempts);

    Params(const Params& other);

    ~Params();

    // The procedure to use for resolving host names. This will be NULL, except
    // in the case of some-tests which inject custom host resolving behaviors.
    scoped_refptr<HostResolverProc> resolver_proc;

    // Maximum number retry attempts to resolve the hostname.
    // Pass HostResolver::Options::kDefaultRetryAttempts to choose a default
    // value.
    size_t max_retry_attempts;

    // This is the limit after which we make another attempt to resolve the host
    // if the worker thread has not responded yet.
    base::TimeDelta unresponsive_delay = kDnsDefaultUnresponsiveDelay;

    // Factor to grow |unresponsive_delay| when we re-re-try.
    uint32_t retry_factor = 2;
  };

  struct CacheParams {
    CacheParams(HostResolverCache& cache,
                NetworkAnonymizationKey network_anonymization_key);
    CacheParams(const CacheParams&);
    CacheParams& operator=(const CacheParams&) = default;
    CacheParams(CacheParams&&);
    CacheParams& operator=(CacheParams&&) = default;
    ~CacheParams();

    base::raw_ref<HostResolverCache> cache;
    NetworkAnonymizationKey network_anonymization_key;
  };

  static std::unique_ptr<HostResolverSystemTask> Create(
      std::string hostname,
      AddressFamily address_family,
      HostResolverFlags flags,
      const Params& params = Params(nullptr, 0),
      const NetLogWithSource& job_net_log = NetLogWithSource(),
      handles::NetworkHandle network = handles::kInvalidNetworkHandle,
      std::optional<CacheParams> cache_params = std::nullopt);

  // Same as above but resolves the result of GetHostName() (the machine's own
  // hostname).
  static std::unique_ptr<HostResolverSystemTask> CreateForOwnHostname(
      AddressFamily address_family,
      HostResolverFlags flags,
      const Params& params = Params(nullptr, 0),
      const NetLogWithSource& job_net_log = NetLogWithSource(),
      handles::NetworkHandle network = handles::kInvalidNetworkHandle);

  // If `hostname` is std::nullopt, resolves the result of GetHostName().
  // Prefer using the above 2 static functions for constructing a
  // HostResolverSystemTask.
  HostResolverSystemTask(
      std::optional<std::string> hostname,
      AddressFamily address_family,
      HostResolverFlags flags,
      const Params& params = Params(nullptr, 0),
      const NetLogWithSource& job_net_log = NetLogWithSource(),
      handles::NetworkHandle network = handles::kInvalidNetworkHandle,
      std::optional<CacheParams> cache_params = std::nullopt);

  HostResolverSystemTask(const HostResolverSystemTask&) = delete;
  HostResolverSystemTask& operator=(const HostResolverSystemTask&) = delete;

  // Cancels this HostResolverSystemTask. Any outstanding resolve attempts
  // cannot be cancelled, but they will post back to the current thread before
  // checking their WeakPtrs to find that this task is cancelled.
  ~HostResolverSystemTask();

  // Starts the resolution task. This can only be called once per
  // HostResolverSystemTask. `results_cb` will not be invoked synchronously and
  // can own `this`.
  void Start(SystemDnsResultsCallback results_cb);

  bool was_completed() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return results_cb_.is_null();
  }

 private:
  void StartLookupAttempt();

  // Callback for when DoLookup() completes.
  void OnLookupComplete(const uint32_t attempt_number,
                        const AddressList& results,
                        const int os_error,
                        int error);

  void MaybeCacheResults(const AddressList& address_list);
  void CacheEndpoints(std::string domain_name,
                      std::vector<IPEndPoint> endpoints,
                      DnsQueryType query_type);
  void CacheAlias(std::string domain_name,
                  DnsQueryType query_type,
                  std::string target_name);

  // If `hostname_` is std::nullopt, this class should resolve the result of
  // net::GetHostName() (the machine's own hostname).
  const std::optional<std::string> hostname_;
  const AddressFamily address_family_;
  const HostResolverFlags flags_;

  // Holds an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  Params params_;

  // The listener to the results of this HostResolverSystemTask.
  SystemDnsResultsCallback results_cb_;

  // Keeps track of the number of attempts we have made so far to resolve the
  // host. Whenever we start an attempt to resolve the host, we increase this
  // number.
  uint32_t attempt_number_ = 0;

  NetLogWithSource net_log_;

  // Network to perform DNS lookups for.
  const handles::NetworkHandle network_;

  std::optional<CacheParams> cache_params_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to loop back from the blocking lookup attempt tasks as well as from
  // delayed retry tasks. Invalidate WeakPtrs on completion and cancellation to
  // cancel handling of such posted tasks.
  base::WeakPtrFactory<HostResolverSystemTask> weak_ptr_factory_{this};
};

// Ensures any necessary initialization occurs such that
// SystemHostResolverCall() can be called on other threads.
NET_EXPORT void EnsureSystemHostResolverCallReady();

// Resolves `host` to an address list, using the system's default host resolver.
// (i.e. this calls out to getaddrinfo()). If successful returns OK and fills
// `addrlist` with a list of socket addresses. Otherwise returns a
// network error code, and fills `os_error` with a more specific error if it
// was non-NULL.
// `network` is an optional parameter, when specified (!=
// handles::kInvalidNetworkHandle) the lookup will be performed specifically for
// `network`.
//
// This should NOT be called in a sandboxed process.
NET_EXPORT_PRIVATE int SystemHostResolverCall(
    const std::string& host,
    AddressFamily address_family,
    HostResolverFlags host_resolver_flags,
    AddressList* addrlist,
    int* os_error,
    handles::NetworkHandle network = handles::kInvalidNetworkHandle);

// Sets the task runner that system DNS resolution will run on, which is mostly
// useful for tests and fuzzers that need reproducibilty of failures.
NET_EXPORT_PRIVATE void SetSystemDnsResolutionTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner);

// The following will be used to override the behavior of
// HostResolverSystemTask. This override will be called instead of posting
// SystemHostResolverCall() to a worker thread. The override will only be
// invoked on the main thread.
// The override should never invoke `results_cb` synchronously.
NET_EXPORT void SetSystemDnsResolverOverride(
    base::RepeatingCallback<void(const std::optional<std::string>& host,
                                 AddressFamily address_family,
                                 HostResolverFlags host_resolver_flags,
                                 SystemDnsResultsCallback results_cb,
                                 handles::NetworkHandle network)> dns_override);

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_SYSTEM_TASK_H_

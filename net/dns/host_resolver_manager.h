// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class AddressList;
class DnsClient;
class HostPortPair;
class IPAddress;
class MDnsClient;
class MDnsSocketFactory;
class NetLog;
class NetLogWithSource;
class NetworkIsolationKey;
class URLRequestContext;

// Scheduler and controller of host resolution requests. Because of the global
// nature of host resolutions, this class is generally expected to be singleton
// within the browser and only be interacted with through per-context
// ContextHostResolver objects (which are themselves generally interacted with
// though the HostResolver interface).
//
// For each hostname that is requested, HostResolver creates a
// HostResolverManager::Job. When this job gets dispatched it creates a task
// (ProcTask for the system resolver or DnsTask for the async resolver) which
// resolves the hostname. If requests for that same host are made during the
// job's lifetime, they are attached to the existing job rather than creating a
// new one. This avoids doing parallel resolves for the same host.
//
// The way these classes fit together is illustrated by:
//
//
//            +----------- HostResolverManager ----------+
//            |                    |                     |
//           Job                  Job                   Job
//    (for host1, fam1)    (for host2, fam2)     (for hostx, famx)
//       /    |   |            /   |   |             /   |   |
//   Request ... Request  Request ... Request   Request ... Request
//  (port1)     (port2)  (port3)      (port4)  (port5)      (portX)
//
// When a HostResolverManager::Job finishes, the callbacks of each waiting
// request are run on the origin thread.
//
// Thread safety: This class is not threadsafe, and must only be called
// from one thread!
//
// The HostResolverManager enforces limits on the maximum number of concurrent
// threads using PrioritizedDispatcher::Limits.
//
// Jobs are ordered in the queue based on their priority and order of arrival.
class NET_EXPORT HostResolverManager
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::ConnectionTypeObserver,
      public SystemDnsConfigChangeNotifier::Observer {
 public:
  using MdnsListener = HostResolver::MdnsListener;
  using ResolveHostParameters = HostResolver::ResolveHostParameters;
  using SecureDnsMode = DnsConfig::SecureDnsMode;

  // A request that allows explicit cancellation before destruction. Enables
  // callers (e.g. ContextHostResolver) to implement cancellation of requests on
  // the callers' destruction.
  class CancellableRequest {
   public:
    CancellableRequest() = default;
    CancellableRequest(const CancellableRequest&) = delete;
    CancellableRequest& operator=(const CancellableRequest&) = delete;
    virtual ~CancellableRequest() = default;

    // If running asynchronously, silently cancels the request as if destroyed.
    // Callbacks will never be invoked. Noop if request is already complete or
    // never started.
    virtual void Cancel() = 0;
  };

  // CancellableRequest versions of different request types.
  class CancellableResolveHostRequest
      : public CancellableRequest,
        public HostResolver::ResolveHostRequest {};
  class CancellableProbeRequest : public CancellableRequest,
                                  public HostResolver::ProbeRequest {};

  // Creates a HostResolver as specified by |options|. Blocking tasks are run in
  // ThreadPool.
  //
  // If Options.enable_caching is true, a cache is created using
  // HostCache::CreateDefaultCache(). Otherwise no cache is used.
  //
  // Options.GetDispatcherLimits() determines the maximum number of jobs that
  // the resolver will run at once. This upper-bounds the total number of
  // outstanding DNS transactions (not counting retransmissions and retries).
  //
  // |net_log| and |system_dns_config_notifier|, if non-null, must remain valid
  // for the life of the HostResolverManager.
  HostResolverManager(const HostResolver::ManagerOptions& options,
                      SystemDnsConfigChangeNotifier* system_dns_config_notifier,
                      NetLog* net_log);

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  ~HostResolverManager() override;

  // If |host_cache| is non-null, its HostCache::Invalidator must have already
  // been added (via AddHostCacheInvalidator()). If |optional_parameters|
  // specifies any cache usage other than LOCAL_ONLY, there must be a 1:1
  // correspondence between |request_context| and |host_cache|, and both should
  // come from the same ContextHostResolver.
  std::unique_ptr<CancellableResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters,
      URLRequestContext* request_context,
      HostCache* host_cache);
  // |request_context| is the context to use for the probes, and it is expected
  // to be the context of the calling ContextHostResolver.
  std::unique_ptr<CancellableProbeRequest> CreateDohProbeRequest(
      URLRequestContext* request_context);
  std::unique_ptr<MdnsListener> CreateMdnsListener(const HostPortPair& host,
                                                   DnsQueryType query_type);

  // Enables or disables the built-in asynchronous DnsClient. If enabled, by
  // default (when no |ResolveHostParameters::source| is specified), the
  // DnsClient will be used for resolves and, in case of failure, resolution
  // will fallback to the system resolver (HostResolverProc from
  // ProcTaskParams). If the DnsClient is not pre-configured with a valid
  // DnsConfig, a new config is fetched from NetworkChangeNotifier.
  //
  // Setting to |true| has no effect if |ENABLE_BUILT_IN_DNS| not defined.
  virtual void SetInsecureDnsClientEnabled(bool enabled);

  std::unique_ptr<base::Value> GetDnsConfigAsValue() const;

  // Sets overriding configuration that will replace or add to configuration
  // read from the system for DnsClient resolution.
  void SetDnsConfigOverrides(DnsConfigOverrides overrides);

  // Support for invalidating HostCaches on changes to network or DNS
  // configuration. HostCaches should register/deregister invalidators here
  // rather than attempting to listen for relevant network change signals
  // themselves because HostResolverManager needs to coordinate invalidations
  // with in-progress resolves and because some invalidations are triggered by
  // changes to manager properties/configuration rather than pure network
  // changes.
  //
  // Note: Invalidation handling must not call back into HostResolverManager as
  // the invalidation is expected to be handled atomically with other clearing
  // and aborting actions.
  void AddHostCacheInvalidator(HostCache::Invalidator* invalidator);
  void RemoveHostCacheInvalidator(const HostCache::Invalidator* invalidator);

  void set_proc_params_for_test(const ProcTaskParams& proc_params) {
    proc_params_ = proc_params;
  }

  void InvalidateCachesForTesting() { InvalidateCaches(); }

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Configures maximum number of Jobs in the queue. Exposed for testing.
  // Only allowed when the queue is empty.
  void SetMaxQueuedJobsForTesting(size_t value);

  void SetMdnsSocketFactoryForTesting(
      std::unique_ptr<MDnsSocketFactory> socket_factory);
  void SetMdnsClientForTesting(std::unique_ptr<MDnsClient> client);

  // To simulate modifications it would have received if |dns_client| had been
  // in place before calling this, DnsConfig will be set with the configuration
  // from the previous DnsClient being replaced (including system config if
  // |dns_client| does not already contain a system config). This means tests do
  // not normally need to worry about ordering between setting a test client and
  // setting DnsConfig.
  void SetDnsClientForTesting(std::unique_ptr<DnsClient> dns_client);

  // Sets the last IPv6 probe result for testing. Uses the standard timeout
  // duration, so it's up to the test fixture to ensure it doesn't expire by
  // mocking time, if expiration would pose a problem.
  void SetLastIPv6ProbeResultForTesting(bool last_ipv6_probe_result);

  // Allows the tests to catch slots leaking out of the dispatcher.  One
  // HostResolverManager::Job could occupy multiple PrioritizedDispatcher job
  // slots.
  size_t num_running_dispatcher_jobs_for_tests() const {
    return dispatcher_->num_running_jobs();
  }

  size_t num_jobs_for_testing() const { return jobs_.size(); }

  bool check_ipv6_on_wifi_for_testing() const { return check_ipv6_on_wifi_; }

 protected:
  // Callback from HaveOnlyLoopbackAddresses probe.
  void SetHaveOnlyLoopbackAddresses(bool result);

  // Sets the task runner used for HostResolverProc tasks.
  void SetTaskRunnerForTesting(scoped_refptr<base::TaskRunner> task_runner);

 private:
  friend class HostResolverManagerTest;
  friend class HostResolverManagerDnsTest;
  class Job;
  struct JobKey;
  class ProcTask;
  class LoopbackProbeJob;
  class DnsTask;
  class RequestImpl;
  class ProbeRequestImpl;
  using JobMap = std::map<JobKey, std::unique_ptr<Job>>;

  // Task types that a Job might run.
  enum class TaskType {
    PROC,
    DNS,
    SECURE_DNS,
    MDNS,
    CACHE_LOOKUP,
    INSECURE_CACHE_LOOKUP,
    SECURE_CACHE_LOOKUP,
  };

  // Attempts host resolution for |request|. Generally only expected to be
  // called from RequestImpl::Start().
  int Resolve(RequestImpl* request);

  // Attempts host resolution using fast local sources: IP literal resolution,
  // cache lookup, HOSTS lookup (if enabled), and localhost. Returns results
  // with error() OK if successful, ERR_NAME_NOT_RESOLVED if input is invalid,
  // or ERR_DNS_CACHE_MISS if the host could not be resolved using local
  // sources.
  //
  // On ERR_DNS_CACHE_MISS and OK, effective request parameters are written to
  // |out_effective_query_type|, |out_effective_host_resolver_flags|, and
  // |out_effective_secure_dns_mode|. |out_tasks| contains the tentative
  // sequence of tasks that a future job should run.
  //
  // If results are returned from the host cache, |out_stale_info| will be
  // filled in with information on how stale or fresh the result is. Otherwise,
  // |out_stale_info| will be set to |base::nullopt|.
  //
  // If |cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED|, then
  // stale cache entries can be returned.
  HostCache::Entry ResolveLocally(
      const std::string& hostname,
      const NetworkIsolationKey& network_isolation_key,
      DnsQueryType requested_address_family,
      HostResolverSource source,
      HostResolverFlags flags,
      base::Optional<SecureDnsMode> secure_dns_mode_override,
      ResolveHostParameters::CacheUsage cache_usage,
      const NetLogWithSource& request_net_log,
      HostCache* cache,
      DnsQueryType* out_effective_query_type,
      HostResolverFlags* out_effective_host_resolver_flags,
      DnsConfig::SecureDnsMode* out_effective_secure_dns_mode,
      std::deque<TaskType>* out_tasks,
      base::Optional<HostCache::EntryStaleness>* out_stale_info);

  // Creates and starts a Job to asynchronously attempt to resolve
  // |request|.
  void CreateAndStartJob(DnsQueryType effective_query_type,
                         HostResolverFlags effective_host_resolver_flags,
                         DnsConfig::SecureDnsMode effective_secure_dns_mode,
                         std::deque<TaskType> tasks,
                         RequestImpl* request);

  // Tries to resolve |key| and its possible IP address representation,
  // |ip_address|. Returns a results entry iff the input can be resolved.
  base::Optional<HostCache::Entry> ResolveAsIP(DnsQueryType query_type,
                                               bool resolve_canonname,
                                               const IPAddress* ip_address);

  // Returns the result iff |cache_usage| permits cache lookups and a positive
  // match is found for |key| in |cache|. |out_stale_info| must be non-null, and
  // will be filled in with details of the entry's staleness if an entry is
  // returned, otherwise it will be set to |base::nullopt|.
  base::Optional<HostCache::Entry> MaybeServeFromCache(
      HostCache* cache,
      const HostCache::Key& key,
      ResolveHostParameters::CacheUsage cache_usage,
      bool ignore_secure,
      const NetLogWithSource& source_net_log,
      base::Optional<HostCache::EntryStaleness>* out_stale_info);

  // Iff we have a DnsClient with a valid DnsConfig and we're not about to
  // attempt a system lookup, then try to resolve the query using the HOSTS
  // file.
  base::Optional<HostCache::Entry> ServeFromHosts(
      base::StringPiece hostname,
      DnsQueryType query_type,
      bool default_family_due_to_no_ipv6,
      const std::deque<TaskType>& tasks);

  // Iff |key| is for a localhost name (RFC 6761) and address DNS query type,
  // returns a results entry with the loopback IP.
  base::Optional<HostCache::Entry> ServeLocalhost(
      base::StringPiece hostname,
      DnsQueryType query_type,
      bool default_family_due_to_no_ipv6);

  // Returns the secure dns mode to use for a job, taking into account the
  // global DnsConfig mode and any per-request override. Requests matching DoH
  // server hostnames are downgraded to off mode to avoid infinite loops.
  SecureDnsMode GetEffectiveSecureDnsMode(
      const std::string& hostname,
      base::Optional<SecureDnsMode> secure_dns_mode_override);

  // Returns true if a catch-all DNS block has been set for unit tests. No
  // DnsTasks should be issued in this case.
  bool HaveTestProcOverride();

  // Helper method to add DnsTasks and related tasks based on the SecureDnsMode
  // and fallback parameters. If |prioritize_local_lookups| is true, then we
  // may push an insecure cache lookup ahead of a secure DnsTask.
  void PushDnsTasks(bool proc_task_allowed,
                    SecureDnsMode secure_dns_mode,
                    bool insecure_tasks_allowed,
                    bool allow_cache,
                    bool prioritize_local_lookups,
                    std::deque<TaskType>* out_tasks);

  // Initialized the sequence of tasks to run to resolve a request. The sequence
  // may be adjusted later and not all tasks need to be run.
  void CreateTaskSequence(
      const std::string& hostname,
      DnsQueryType dns_query_type,
      HostResolverSource source,
      HostResolverFlags flags,
      base::Optional<SecureDnsMode> secure_dns_mode_override,
      ResolveHostParameters::CacheUsage cache_usage,
      DnsConfig::SecureDnsMode* out_effective_secure_dns_mode,
      std::deque<TaskType>* out_tasks);

  // Determines "effective" request parameters using manager properties and IPv6
  // reachability.
  void GetEffectiveParametersForRequest(
      const std::string& hostname,
      DnsQueryType dns_query_type,
      HostResolverSource source,
      HostResolverFlags flags,
      base::Optional<SecureDnsMode> secure_dns_mode_override,
      ResolveHostParameters::CacheUsage cache_usage,
      const IPAddress* ip_address,
      const NetLogWithSource& net_log,
      DnsQueryType* out_effective_type,
      HostResolverFlags* out_effective_flags,
      DnsConfig::SecureDnsMode* out_effective_secure_dns_mode,
      std::deque<TaskType>* out_tasks);

  // Probes IPv6 support and returns true if IPv6 support is enabled.
  // Results are cached, i.e. when called repeatedly this method returns result
  // from the first probe for some time before probing again.
  bool IsIPv6Reachable(const NetLogWithSource& net_log);

  // Sets |last_ipv6_probe_result_| and updates |last_ipv6_probe_time_|.
  void SetLastIPv6ProbeResult(bool last_ipv6_probe_result);

  // Attempts to connect a UDP socket to |dest|:53. Virtual for testing.
  virtual bool IsGloballyReachable(const IPAddress& dest,
                                   const NetLogWithSource& net_log);

  // Asynchronously checks if only loopback IPs are available.
  virtual void RunLoopbackProbeJob();

  // Records the result in cache if cache is present.
  void CacheResult(HostCache* cache,
                   const HostCache::Key& key,
                   const HostCache::Entry& entry,
                   base::TimeDelta ttl);

  // Record time from Request creation until a valid DNS response.
  void RecordTotalTime(bool speculative,
                       bool from_cache,
                       DnsConfig::SecureDnsMode secure_dns_mode,
                       base::TimeDelta duration) const;

  // Removes |job_it| from |jobs_| and return.
  std::unique_ptr<Job> RemoveJob(JobMap::iterator job_it);

  // Aborts both scheduled and running jobs with ERR_NETWORK_CHANGED and
  // notifies their requests. Aborts only running jobs if |in_progress_only| is
  // true. Might start new jobs.
  void AbortAllJobs(bool in_progress_only);

  // Aborts all in progress insecure DnsTasks. In-progress jobs will fall back
  // to ProcTasks if able and otherwise abort with |error|. Might start new
  // jobs, if any jobs were taking up two dispatcher slots.
  //
  // If |fallback_only|, insecure DnsTasks will only abort if they can fallback
  // to ProcTask.
  void AbortInsecureDnsTasks(int error, bool fallback_only);

  // Attempts to serve each Job in |jobs_| from the HOSTS file if we have
  // a DnsClient with a valid DnsConfig.
  void TryServingAllJobsFromHosts();

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // SystemDnsConfigChangeNotifier::Observer:
  void OnSystemDnsConfigChanged(base::Optional<DnsConfig> config) override;

  void UpdateJobsForChangedConfig();

  // Called on successful resolve after falling back to ProcTask after a failed
  // DnsTask resolve.
  void OnFallbackResolve(int dns_task_error);

  int GetOrCreateMdnsClient(MDnsClient** out_client);

  void InvalidateCaches();

  // Currently only allows one probe to be started at a time. Must be cancelled
  // before starting another.
  void ActivateDohProbes(URLRequestContext* url_request_context);
  void CancelDohProbes();

  // Used for multicast DNS tasks. Created on first use using
  // GetOrCreateMndsClient().
  std::unique_ptr<MDnsSocketFactory> mdns_socket_factory_;
  std::unique_ptr<MDnsClient> mdns_client_;

  // Map from HostCache::Key to a Job.
  JobMap jobs_;

  // Starts Jobs according to their priority and the configured limits.
  std::unique_ptr<PrioritizedDispatcher> dispatcher_;

  // Limit on the maximum number of jobs queued in |dispatcher_|.
  size_t max_queued_jobs_;

  // Parameters for ProcTask.
  ProcTaskParams proc_params_;

  NetLog* net_log_;

  // If present, used by DnsTask and ServeFromHosts to resolve requests.
  std::unique_ptr<DnsClient> dns_client_;

  SystemDnsConfigChangeNotifier* system_dns_config_notifier_;

  // False if IPv6 should not be attempted and assumed unreachable when on a
  // WiFi connection. See https://crbug.com/696569 for further context.
  bool check_ipv6_on_wifi_;

  base::TimeTicks last_ipv6_probe_time_;
  bool last_ipv6_probe_result_;

  // Any resolver flags that should be added to a request by default.
  HostResolverFlags additional_resolver_flags_;

  // Allow fallback to ProcTask if DnsTask fails.
  bool allow_fallback_to_proctask_;

  // Task runner used for DNS lookups using the system resolver. Normally a
  // ThreadPool task runner, but can be overridden for tests.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  // Shared tick clock, overridden for testing.
  const base::TickClock* tick_clock_;

  // For HostCache invalidation notifications.
  base::ObserverList<HostCache::Invalidator,
                     true /* check_empty */,
                     false /* allow_reentrancy */>
      host_cache_invalidators_;
  bool invalidation_in_progress_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<HostResolverManager> weak_ptr_factory_{this};

  base::WeakPtrFactory<HostResolverManager> probe_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostResolverManager);
};

// Resolves a local hostname (such as "localhost" or "localhost6") into
// IP endpoints (with port 0). Returns true if |host| is a local
// hostname and false otherwise. Special IPv6 names (e.g. "localhost6")
// will resolve to an IPv6 address only, whereas other names will
// resolve to both IPv4 and IPv6.
// This function is only exposed so it can be unit-tested.
// TODO(tfarina): It would be better to change the tests so this function
// gets exercised indirectly through HostResolverManager.
NET_EXPORT_PRIVATE bool ResolveLocalHostname(base::StringPiece host,
                                             AddressList* address_list);

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_H_

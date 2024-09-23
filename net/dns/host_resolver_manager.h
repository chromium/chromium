// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class DnsClient;
class DnsProbeRunner;
class IPAddress;
class MDnsClient;
class ClientSocketFactory;
class MDnsSocketFactory;
class NetLog;

// Scheduler and controller of host resolution requests. Because of the global
// nature of host resolutions, this class is generally expected to be singleton
// within the browser and only be interacted with through per-context
// ContextHostResolver objects (which are themselves generally interacted with
// though the HostResolver interface).
//
// For each hostname that is requested, HostResolver creates a
// HostResolverManager::Job. When this job gets dispatched it creates a task
// (HostResolverSystemTask for the system resolver or DnsTask for the async
// resolver) which resolves the hostname. If requests for that same host are
// made during the job's lifetime, they are attached to the existing job rather
// than creating a new one. This avoids doing parallel resolves for the same
// host.
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
  using PassKey = base::PassKey<HostResolverManager>;

  struct NET_EXPORT_PRIVATE JobKey;
  class NET_EXPORT_PRIVATE Job;
  class NET_EXPORT_PRIVATE RequestImpl;
  class NET_EXPORT_PRIVATE ServiceEndpointRequestImpl;

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

  HostResolverManager(const HostResolverManager&) = delete;
  HostResolverManager& operator=(const HostResolverManager&) = delete;

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  ~HostResolverManager() override;

  // Same as constructor above, but binds the HostResolverManager to
  // `target_network`: all DNS requests will be performed for `target_network`
  // only, requests will fail if `target_network` disconnects. Only
  // HostResolvers bound to the same network will be able to use this.
  // Only implemented for Android.
  static std::unique_ptr<HostResolverManager>
  CreateNetworkBoundHostResolverManager(
      const HostResolver::ManagerOptions& options,
      handles::NetworkHandle target_network,
      NetLog* net_log);

  // Creates a host resolution request. `resolve_context` must have already been
  // added via RegisterResolveContext(). If `optional_parameters` specifies any
  // cache usage other than LOCAL_ONLY, `resolve_context` should have a valid
  // `host_cache()` coming from a ContextHostResolver that owns
  // `resolve_context`.
  std::unique_ptr<HostResolver::ResolveHostRequest> CreateRequest(
      absl::variant<url::SchemeHostPort, HostPortPair> host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters,
      ResolveContext* resolve_context);
  std::unique_ptr<HostResolver::ResolveHostRequest> CreateRequest(
      HostResolver::Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters,
      ResolveContext* resolve_context);
  // |resolve_context| is the context to use for the probes, and it is expected
  // to be the context of the calling ContextHostResolver.
  std::unique_ptr<HostResolver::ProbeRequest> CreateDohProbeRequest(
      ResolveContext* resolve_context);
  std::unique_ptr<MdnsListener> CreateMdnsListener(const HostPortPair& host,
                                                   DnsQueryType query_type);

  // Creates a service endpoint resolution request.
  std::unique_ptr<HostResolver::ServiceEndpointRequest>
  CreateServiceEndpointRequest(
      url::SchemeHostPort scheme_host_port,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters,
      ResolveContext* resolve_context);

  // Enables or disables the built-in asynchronous DnsClient. If enabled, by
  // default (when no |ResolveHostParameters::source| is specified), the
  // DnsClient will be used for resolves and, in case of failure, resolution
  // will fallback to the system resolver (in tests, HostResolverProc from
  // HostResolverSystemTask::Params). If the DnsClient is not pre-configured
  // with a valid DnsConfig, a new config is fetched from NetworkChangeNotifier.
  //
  // Setting to |true| has no effect if |ENABLE_BUILT_IN_DNS| not defined.
  virtual void SetInsecureDnsClientEnabled(bool enabled,
                                           bool additional_dns_types_enabled);

  base::Value::Dict GetDnsConfigAsValue() const;

  // Sets overriding configuration that will replace or add to configuration
  // read from the system for DnsClient resolution.
  void SetDnsConfigOverrides(DnsConfigOverrides overrides);

  void SetIPv6ReachabilityOverride(bool reachability_override);

  // Support for invalidating cached per-context data on changes to network or
  // DNS configuration. ContextHostResolvers should register/deregister
  // themselves here rather than attempting to listen for relevant network
  // change signals themselves because HostResolverManager needs to coordinate
  // invalidations with in-progress resolves and because some invalidations are
  // triggered by changes to manager properties/configuration rather than pure
  // network changes.
  //
  // Note: Invalidation handling must not call back into HostResolverManager as
  // the invalidation is expected to be handled atomically with other clearing
  // and aborting actions.
  void RegisterResolveContext(ResolveContext* context);
  void DeregisterResolveContext(const ResolveContext* context);

  void set_host_resolver_system_params_for_test(
      const HostResolverSystemTask::Params& host_resolver_system_params) {
    host_resolver_system_params_ = host_resolver_system_params;
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

  // Explicitly disable the system resolver even if tests have set a catch-all
  // DNS block. See `ForceSystemResolverDueToTestOverride`.
  void DisableSystemResolverForTesting() {
    system_resolver_disabled_for_testing_ = true;
  }

  // Sets the last IPv6 probe result for testing. Uses the standard timeout
  // duration, so it's up to the test fixture to ensure it doesn't expire by
  // mocking time, if expiration would pose a problem.
  void SetLastIPv6ProbeResultForTesting(bool last_ipv6_probe_result);

  void ResetIPv6ProbeTimeForTesting() {
    last_ipv6_probe_time_ = base::TimeTicks();
  }

  // Allows the tests to catch slots leaking out of the dispatcher.  One
  // HostResolverManager::Job could occupy multiple PrioritizedDispatcher job
  // slots.
  size_t num_running_dispatcher_jobs_for_tests() const {
    return dispatcher_->num_running_jobs();
  }

  size_t num_jobs_for_testing() const { return jobs_.size(); }

  bool check_ipv6_on_wifi_for_testing() const { return check_ipv6_on_wifi_; }

  handles::NetworkHandle target_network_for_testing() const {
    return target_network_;
  }

  const HostResolver::HttpsSvcbOptions& https_svcb_options_for_testing() const {
    return https_svcb_options_;
  }

  // Public to be called from std::make_unique. Not to be called directly.
  HostResolverManager(base::PassKey<HostResolverManager>,
                      const HostResolver::ManagerOptions& options,
                      SystemDnsConfigChangeNotifier* system_dns_config_notifier,
                      handles::NetworkHandle target_network,
                      NetLog* net_log);

 protected:
  // Callback from HaveOnlyLoopbackAddresses probe.
  void SetHaveOnlyLoopbackAddresses(bool result);

 private:
  friend class HostResolverManagerTest;
  friend class HostResolverManagerDnsTest;
  class LoopbackProbeJob;
  class ProbeRequestImpl;
  using JobMap = std::map<JobKey, std::unique_ptr<Job>>;

  // Task types that a Job might run.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused
  enum class TaskType {
    SYSTEM = 0,
    DNS = 1,
    SECURE_DNS = 2,
    MDNS = 3,
    CACHE_LOOKUP = 4,
    INSECURE_CACHE_LOOKUP = 5,
    SECURE_CACHE_LOOKUP = 6,
    CONFIG_PRESET = 7,
    NAT64 = 8,
    HOSTS = 9,

    kMaxValue = HOSTS,
  };

  // Returns true if the task is local, synchronous, and instantaneous.
  static bool IsLocalTask(TaskType task);

  // Initializes a job key and an IP address using manager properties and IPv6
  // reachability. These job key and IP address are used to call
  // ResolveLocally() and CreateAndStartJob().
  void InitializeJobKeyAndIPAddress(
      const NetworkAnonymizationKey& network_anonymization_key,
      const ResolveHostParameters& parameters,
      const NetLogWithSource& source_net_log,
      JobKey& out_job_key,
      IPAddress& out_ip_address);

  // Attempts host resolution using fast local sources: IP literal resolution,
  // cache lookup, HOSTS lookup (if enabled), and localhost. Returns results
  // with error() OK if successful, ERR_NAME_NOT_RESOLVED if input is invalid,
  // or ERR_DNS_CACHE_MISS if the host could not be resolved using local
  // sources.
  //
  // On ERR_DNS_CACHE_MISS and OK, |out_tasks| contains the tentative
  // sequence of tasks that a future job should run.
  //
  // If results are returned from the host cache, |out_stale_info| will be
  // filled in with information on how stale or fresh the result is. Otherwise,
  // |out_stale_info| will be set to |std::nullopt|.
  //
  // If |cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED|, then
  // stale cache entries can be returned.
  HostCache::Entry ResolveLocally(
      bool only_ipv6_reachable,
      const JobKey& job_key,
      const IPAddress& ip_address,
      ResolveHostParameters::CacheUsage cache_usage,
      SecureDnsPolicy secure_dns_policy,
      HostResolverSource source,
      const NetLogWithSource& source_net_log,
      HostCache* cache,
      std::deque<TaskType>* out_tasks,
      std::optional<HostCache::EntryStaleness>* out_stale_info);

  // Creates and starts a Job to asynchronously attempt to resolve
  // |request|.
  void CreateAndStartJob(JobKey key,
                         std::deque<TaskType> tasks,
                         RequestImpl* request);

  HostResolverManager::Job* AddJobWithoutRequest(
      JobKey key,
      ResolveHostParameters::CacheUsage cache_usage,
      HostCache* host_cache,
      std::deque<TaskType> tasks,
      RequestPriority priority,
      const NetLogWithSource& source_net_log);

  // Similar to CreateAndStartJob(), but for a ServiceEndpointRequest.
  void CreateAndStartJobForServiceEndpointRequest(
      JobKey key,
      std::deque<TaskType> tasks,
      ServiceEndpointRequestImpl* request);

  // Resolves the IP literal hostname represented by `ip_address`.
  HostCache::Entry ResolveAsIP(DnsQueryTypeSet query_types,
                               bool resolve_canonname,
                               const IPAddress& ip_address);

  // Returns the result iff |cache_usage| permits cache lookups and a positive
  // match is found for |key| in |cache|. |out_stale_info| must be non-null, and
  // will be filled in with details of the entry's staleness if an entry is
  // returned, otherwise it will be set to |std::nullopt|.
  std::optional<HostCache::Entry> MaybeServeFromCache(
      HostCache* cache,
      const HostCache::Key& key,
      ResolveHostParameters::CacheUsage cache_usage,
      bool ignore_secure,
      const NetLogWithSource& source_net_log,
      std::optional<HostCache::EntryStaleness>* out_stale_info);

  // Returns any preset resolution result from the active DoH configuration that
  // matches |key.host|.
  std::optional<HostCache::Entry> MaybeReadFromConfig(const JobKey& key);

  // Populates the secure cache with an optimal entry that supersedes the
  // bootstrap result.
  void StartBootstrapFollowup(JobKey key,
                              HostCache* host_cache,
                              const NetLogWithSource& source_net_log);

  // Iff we have a DnsClient with a valid DnsConfig and we're not about to
  // attempt a system lookup, then try to resolve the query using the HOSTS
  // file.
  std::optional<HostCache::Entry> ServeFromHosts(
      std::string_view hostname,
      DnsQueryTypeSet query_types,
      bool default_family_due_to_no_ipv6,
      const std::deque<TaskType>& tasks);

  // Iff |key| is for a localhost name (RFC 6761) and address DNS query type,
  // returns a results entry with the loopback IP.
  std::optional<HostCache::Entry> ServeLocalhost(
      std::string_view hostname,
      DnsQueryTypeSet query_types,
      bool default_family_due_to_no_ipv6);

  // Returns the secure dns mode to use for a job, taking into account the
  // global DnsConfig mode and any per-request policy.
  SecureDnsMode GetEffectiveSecureDnsMode(SecureDnsPolicy secure_dns_policy);

  // Returns true when a catch-all DNS block has been set for tests, unless
  // `SetDisableSystemResolverForTesting` has been called to explicitly disable
  // that safety net. DnsTasks should never be issued when this returns true.
  bool ShouldForceSystemResolverDueToTestOverride() const;

  // Helper method to add DnsTasks and related tasks based on the SecureDnsMode
  // and fallback parameters. If |prioritize_local_lookups| is true, then we
  // may push an insecure cache lookup ahead of a secure DnsTask.
  void PushDnsTasks(bool system_task_allowed,
                    SecureDnsMode secure_dns_mode,
                    bool insecure_tasks_allowed,
                    bool allow_cache,
                    bool prioritize_local_lookups,
                    ResolveContext* resolve_context,
                    std::deque<TaskType>* out_tasks);

  // Initialized the sequence of tasks to run to resolve a request. The sequence
  // may be adjusted later and not all tasks need to be run.
  void CreateTaskSequence(const JobKey& job_key,
                          ResolveHostParameters::CacheUsage cache_usage,
                          SecureDnsPolicy secure_dns_policy,
                          std::deque<TaskType>* out_tasks);

  // Schedules probes to check IPv6 support. Returns OK if probe results are
  // already cached, and ERR_IO_PENDING when a probe is scheduled to be
  // completed asynchronously. When called repeatedly this method returns OK to
  // confirm that results have been cached.
  int StartIPv6ReachabilityCheck(const NetLogWithSource& net_log,
                                 ClientSocketFactory* client_socket_factory,
                                 CompletionOnceCallback callback);

  void FinishIPv6ReachabilityCheck(CompletionOnceCallback callback, int rv);

  // Sets |last_ipv6_probe_result_| and updates |last_ipv6_probe_time_|.
  void SetLastIPv6ProbeResult(bool last_ipv6_probe_result);

  // Attempts to connect a UDP socket to |dest|:53. Virtual for testing. Returns
  // the value of the attempted socket connection and the reachability check. If
  // the return value from the connection is not ERR_IO_PENDING, callers must
  // handle the results of the reachability check themselves. Otherwise the
  // result of the reachability check will be set when `callback` is run.
  // Returns OK if the reachability check succeeded, ERR_FAILED if it failed,
  // ERR_IO_PENDING if it will be asynchronous.
  virtual int StartGloballyReachableCheck(
      const IPAddress& dest,
      const NetLogWithSource& net_log,
      ClientSocketFactory* client_socket_factory,
      CompletionOnceCallback callback);

  bool FinishGloballyReachableCheck(DatagramClientSocket* socket, int rv);

  void RunFinishGloballyReachableCheck(
      scoped_refptr<base::RefCountedData<std::unique_ptr<DatagramClientSocket>>>
          socket,
      CompletionOnceCallback callback,
      int rv);

  // Asynchronously checks if only loopback IPs are available.
  virtual void RunLoopbackProbeJob();

  // Records the result in cache if cache is present.
  void CacheResult(HostCache* cache,
                   const HostCache::Key& key,
                   const HostCache::Entry& entry,
                   base::TimeDelta ttl);

  // Removes |job_it| from |jobs_| and return.
  std::unique_ptr<Job> RemoveJob(JobMap::iterator job_it);

  // Removes Jobs for this context.
  void RemoveAllJobs(const ResolveContext* context);

  // Aborts all jobs (both scheduled and running) which are not targeting a
  // specific network with ERR_NETWORK_CHANGED and notifies their requests.
  // Aborts only running jobs if `in_progress_only` is true. Might start new
  // jobs.
  void AbortJobsWithoutTargetNetwork(bool in_progress_only);

  // Aborts all in progress insecure DnsTasks. In-progress jobs will fall back
  // to HostResolverSystemTasks if able and otherwise abort with |error|. Might
  // start new jobs, if any jobs were taking up two dispatcher slots.
  //
  // If |fallback_only|, insecure DnsTasks will only abort if they can fallback
  // to HostResolverSystemTasks.
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
  void OnSystemDnsConfigChanged(std::optional<DnsConfig> config) override;

  void UpdateJobsForChangedConfig();

  // Called on successful resolve after falling back to HostResolverSystemTask
  // after a failed DnsTask resolve.
  void OnFallbackResolve(int dns_task_error);

  int GetOrCreateMdnsClient(MDnsClient** out_client);

  // |network_change| indicates whether or not the invalidation was triggered
  // by a network connection change.
  void InvalidateCaches(bool network_change = false);

  void UpdateConnectionType(NetworkChangeNotifier::ConnectionType type);

  bool IsBoundToNetwork() const {
    return target_network_ != handles::kInvalidNetworkHandle;
  }

  // Returns |nullptr| if DoH probes are currently not allowed (due to
  // configuration or current connection state).
  std::unique_ptr<DnsProbeRunner> CreateDohProbeRunner(
      ResolveContext* resolve_context);

  // Used for multicast DNS tasks. Created on first use using
  // GetOrCreateMndsClient().
  std::unique_ptr<MDnsSocketFactory> mdns_socket_factory_;
  std::unique_ptr<MDnsClient> mdns_client_;

  // Map from HostCache::Key to a Job.
  JobMap jobs_;

  // Starts Jobs according to their priority and the configured limits.
  std::unique_ptr<PrioritizedDispatcher> dispatcher_;

  // Limit on the maximum number of jobs queued in |dispatcher_|.
  size_t max_queued_jobs_ = 0;

  // Parameters for HostResolverSystemTask.
  HostResolverSystemTask::Params host_resolver_system_params_;

  raw_ptr<NetLog> net_log_;

  // If present, used by DnsTask and ServeFromHosts to resolve requests.
  std::unique_ptr<DnsClient> dns_client_;

  raw_ptr<SystemDnsConfigChangeNotifier> system_dns_config_notifier_;

  handles::NetworkHandle target_network_;

  // False if IPv6 should not be attempted and assumed unreachable when on a
  // WiFi connection. See https://crbug.com/696569 for further context.
  bool check_ipv6_on_wifi_;

  base::TimeTicks last_ipv6_probe_time_;
  bool last_ipv6_probe_result_ = true;
  bool probing_ipv6_ = false;

  // When true, query AAAA even when the globally reachable check failed.
  bool ipv6_reachability_override_ = false;

  // Any resolver flags that should be added to a request by default.
  HostResolverFlags additional_resolver_flags_ = 0;

  // Allow fallback to HostResolverSystemTask if DnsTask fails.
  bool allow_fallback_to_systemtask_ = true;

  // Shared tick clock, overridden for testing.
  raw_ptr<const base::TickClock> tick_clock_;

  // When true, ignore the catch-all DNS block if it exists.
  bool system_resolver_disabled_for_testing_ = false;

  // For per-context cache invalidation notifications.
  base::ObserverList<ResolveContext,
                     true /* check_empty */,
                     false /* allow_reentrancy */>
      registered_contexts_;
  bool invalidation_in_progress_ = false;

  // An experimental flag for features::kUseDnsHttpsSvcb.
  HostResolver::HttpsSvcbOptions https_svcb_options_;

  std::vector<CompletionOnceCallback> ipv6_request_callbacks_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<HostResolverManager> weak_ptr_factory_{this};

  base::WeakPtrFactory<HostResolverManager> probe_weak_ptr_factory_{this};
};

// Resolves a local hostname (such as "localhost" or "vhost.localhost") into
// IP endpoints (with port 0). Returns true if |host| is a local
// hostname and false otherwise. Names will resolve to both IPv4 and IPv6.
// This function is only exposed so it can be unit-tested.
// TODO(tfarina): It would be better to change the tests so this function
// gets exercised indirectly through HostResolverManager.
NET_EXPORT_PRIVATE bool ResolveLocalHostname(
    std::string_view host,
    std::vector<IPEndPoint>* address_list);

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_H_

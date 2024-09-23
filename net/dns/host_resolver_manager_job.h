// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_JOB_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_JOB_H_

#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/dns/dns_task_results_manager.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

class ResolveContext;
class HostResolverMdnsTask;
class HostResolverNat64Task;

// Key used to identify a HostResolverManager::Job.
struct HostResolverManager::JobKey {
  JobKey(HostResolver::Host host, ResolveContext* resolve_context);
  ~JobKey();

  JobKey(const JobKey& other);
  JobKey& operator=(const JobKey& other);

  bool operator<(const JobKey& other) const;
  bool operator==(const JobKey& other) const;

  HostResolver::Host host;
  NetworkAnonymizationKey network_anonymization_key;
  DnsQueryTypeSet query_types;
  HostResolverFlags flags;
  HostResolverSource source;
  SecureDnsMode secure_dns_mode;
  base::WeakPtr<ResolveContext> resolve_context;

  HostCache::Key ToCacheKey(bool secure) const;

  handles::NetworkHandle GetTargetNetwork() const;
};

// Aggregates all Requests for the same Key. Dispatched via
// PrioritizedDispatcher.
class HostResolverManager::Job : public PrioritizedDispatcher::Job,
                                 public HostResolverDnsTask::Delegate,
                                 public DnsTaskResultsManager::Delegate {
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
      const HostResolver::HttpsSvcbOptions& https_svcb_options);
  ~Job() override;

  // Add this job to the dispatcher.  If "at_head" is true, adds at the front
  // of the queue.
  void Schedule(bool at_head);

  void AddRequest(RequestImpl* request);

  void ChangeRequestPriority(RequestImpl* req, RequestPriority priority);

  // Detach cancelled request. If it was the last active Request, also finishes
  // this Job.
  void CancelRequest(RequestImpl* request);

  void AddServiceEndpointRequest(ServiceEndpointRequestImpl* request);

  // Similar to CancelRequest(), if `request` was the last active one, finishes
  // this job.
  void CancelServiceEndpointRequest(ServiceEndpointRequestImpl* request);

  // Similar to ChangeRequestPriority(), but for a ServiceEndpointRequest.
  void ChangeServiceEndpointRequestPriority(ServiceEndpointRequestImpl* request,
                                            RequestPriority priority);

  // Called from AbortJobsWithoutTargetNetwork(). Completes all requests and
  // destroys the job. This currently assumes the abort is due to a network
  // change.
  // TODO This should not delete |this|.
  void Abort();

  // Gets a closure that will abort an insecure DnsTask (see
  // AbortInsecureDnsTask()) iff |this| is still valid. Useful if aborting a
  // list of Jobs as some may be cancelled while aborting others.
  base::OnceClosure GetAbortInsecureDnsTaskClosure(int error,
                                                   bool fallback_only);

  // Aborts or removes any current/future insecure DnsTasks if a
  // HostResolverSystemTask is available for fallback. If no fallback is
  // available and |fallback_only| is false, a job that is currently running an
  // insecure DnsTask will be completed with |error|.
  void AbortInsecureDnsTask(int error, bool fallback_only);

  // Called by HostResolverManager when this job is evicted due to queue
  // overflow. Completes all requests and destroys the job. The job could have
  // waiting requests that will receive completion callbacks, so cleanup
  // asynchronously to avoid reentrancy.
  void OnEvicted();

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts();

  void OnAddedToJobMap(JobMap::iterator iterator);

  void OnRemovedFromJobMap();

  void RunNextTask();

  const JobKey& key() const { return key_; }

  bool is_queued() const { return !handle_.is_null(); }

  bool is_running() const { return job_running_; }

  bool HasTargetNetwork() const {
    return key_.GetTargetNetwork() != handles::kInvalidNetworkHandle;
  }

  DnsTaskResultsManager* dns_task_results_manager() const {
    return dns_task_results_manager_.get();
  }

 private:
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
      if (highest_priority_ < req_priority) {
        highest_priority_ = req_priority;
      }
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
      if (total_count_ == 0) {
        DCHECK_EQ(MINIMUM_PRIORITY, highest_priority_);
      }
    }

   private:
    RequestPriority highest_priority_;
    size_t total_count_ = 0;
    size_t counts_[NUM_PRIORITIES] = {};
  };

  base::Value::Dict NetLogJobCreationParams(const NetLogSource& source);

  void Finish();

  void KillDnsTask();

  // Reduce the number of job slots occupied and queued in the dispatcher by
  // one. If the next Job slot is queued in the dispatcher, cancels the queued
  // job. Otherwise, the next Job has been started by the PrioritizedDispatcher,
  // so signals it is complete.
  void ReduceByOneJobSlot();

  // Common helper methods for adding and canceling a request.
  void AddRequestCommon(RequestPriority request_priority,
                        const NetLogWithSource& request_net_log,
                        bool is_speculative);
  void CancelRequestCommon(RequestPriority request_priority,
                           const NetLogWithSource& request_net_log);

  void UpdatePriority();

  // PrioritizedDispatcher::Job:
  void Start() override;

  // TODO(szym): Since DnsTransaction does not consume threads, we can increase
  // the limits on |dispatcher_|. But in order to keep the number of
  // ThreadPool threads low, we will need to use an "inner"
  // PrioritizedDispatcher with tighter limits.
  void StartSystemTask();
  // Called by HostResolverSystemTask when it completes.
  void OnSystemTaskComplete(base::TimeTicks start_time,
                            const AddressList& addr_list,
                            int /*os_error*/,
                            int net_error);

  void InsecureCacheLookup();

  void StartDnsTask(bool secure);
  void StartNextDnsTransaction();
  // Called if DnsTask fails. It is posted from StartDnsTask, so Job may be
  // deleted before this callback. In this case dns_task is deleted as well,
  // so we use it as indicator whether Job is still valid.
  void OnDnsTaskFailure(const base::WeakPtr<HostResolverDnsTask>& dns_task,
                        base::TimeDelta duration,
                        bool allow_fallback,
                        const HostCache::Entry& failure_results,
                        bool secure);
  // HostResolverDnsTask::Delegate implementation:
  void OnDnsTaskComplete(base::TimeTicks start_time,
                         bool allow_fallback,
                         HostCache::Entry results,
                         bool secure) override;
  void OnIntermediateTransactionsComplete(
      std::optional<HostResolverDnsTask::SingleTransactionResults>
          single_transaction_results) override;
  void AddTransactionTimeQueued(base::TimeDelta time_queued) override;

  // DnsTaskResultsManager::Delegate implementation:
  void OnServiceEndpointsUpdated() override;

  void StartMdnsTask();
  void OnMdnsTaskComplete();
  void OnMdnsImmediateFailure(int rv);

  void StartNat64Task();
  void OnNat64TaskComplete();

  void RecordJobHistograms(const HostCache::Entry& results,
                           std::optional<TaskType> task_type);

  void MaybeCacheResult(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool secure);

  // Performs Job's last rites. Completes all Requests. Deletes this.
  //
  // If not |allow_cache|, result will not be stored in the host cache, even if
  // result would otherwise allow doing so. Update the key to reflect |secure|,
  // which indicates whether or not the result was obtained securely.
  void CompleteRequests(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool allow_cache,
                        bool secure,
                        std::optional<TaskType> task_type);

  void CompleteRequestsWithoutCache(
      const HostCache::Entry& results,
      std::optional<HostCache::EntryStaleness> stale_info,
      TaskType task_type);

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error,
                                 std::optional<TaskType> task_type);

  RequestPriority priority() const override;

  // Number of non-canceled requests in |requests_|.
  size_t num_active_requests() const { return priority_tracker_.total_count(); }

  base::WeakPtr<HostResolverManager> resolver_;

  const JobKey key_;
  const ResolveHostParameters::CacheUsage cache_usage_;
  // TODO(crbug.com/41462480): Consider allowing requests within a single Job to
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
  std::unique_ptr<HostResolverDnsTask> dns_task_;

  // Resolves the host using MDnsClient.
  std::unique_ptr<HostResolverMdnsTask> mdns_task_;

  // Perform NAT64 address synthesis to a given IPv4 literal.
  std::unique_ptr<HostResolverNat64Task> nat64_task_;

  // All Requests waiting for the result of this Job. Some can be canceled.
  base::LinkedList<RequestImpl> requests_;

  // All ServiceEndpointRequests waiting for the result of this Job. Some can
  // be canceled.
  base::LinkedList<ServiceEndpointRequestImpl> service_endpoint_requests_;

  // Builds and updates intermediate service endpoints while executing
  // a DnsTransaction.
  std::unique_ptr<DnsTaskResultsManager> dns_task_results_manager_;

  // A handle used for |dispatcher_|.
  PrioritizedDispatcher::Handle handle_;

  // Iterator to |this| in the JobMap. |nullopt| if not owned by the JobMap.
  std::optional<JobMap::iterator> self_iterator_;

  base::TimeDelta total_transaction_time_queued_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_JOB_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/origin.h"

namespace base {
class Process;
}  // namespace base

namespace content {

class RenderProcessHost;
class SiteInstance;
class TrustedSignalsCacheImpl;

// Base class of per-StoragePartition manager of auction bidder and seller
// worklet processes. This provides limiting and sharing of worker processes.
//
// AuctionProcessManager managers two types of processes, idle processes, and
// non-idle processes.
//
// Idle processes are owned directly by AuctionProcessManager::idle_processes_,
// and have no associated ProcessHandle -- they have a WorkletProcess only. On
// process crash or idle timeout, they tell the AuctionProcessManager to destroy
// them.
//
// Non-idle processes have been handed out to one or more live ProcessHandles,
// and are tracked in one of the AuctionProcessManager's ProcessMap with
// raw pointers. When the last ProcessHandle releases a reference to the
// WorkletProcess, it's destroyed, and informs the AuctionProcessManager to
// remove it from the map. On process crash, it may also be removed from the
// map, to prevent reuse, even though consumers may still own references to it.
class CONTENT_EXPORT AuctionProcessManager {
 public:
  // The maximum number of bidder processes. Once this number is reached, no
  // processes will be created for bidder worklets, though new bidder worklet
  // requests can receive pre-existing processes.
  static const size_t kMaxBidderProcesses;

  // The maximum number of seller processes. Once this number is reached, no
  // processes will be created for seller worklets, though new seller worklet
  // requests can receive pre-existing processes. Distinct from
  // kMaxBidderProcesses because sellers behave a bit differently - they're
  // alive for the length of the auction. Also, if a putative entire shared
  // process limit were consumed by seller worklets, no more auctions could run,
  // since bidder worklets couldn't load to make bids.
  static const size_t kMaxSellerProcesses;

  // The two worklet types. Sellers and bidders never share processes, primarily
  // to make accounting simpler. They also currently issue requests with
  // different NIKs, so safest to keep them separate, anyways.
  enum class WorkletType {
    kBidder,
    kSeller,
  };

  // Outcome of RequestWorkletService.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestWorkletServiceOutcome {
    kHitProcessLimit = 0,
    kUsedSharedProcess = 1,
    kUsedExistingDedicatedProcess = 2,
    kCreatedNewDedicatedProcess = 3,
    kUsedIdleProcess = 4,
    kMaxValue = kUsedIdleProcess
  };

  class ProcessHandle;

  // Refcounted class that creates / holds Mojo Remote for an
  // AuctionWorkletService. Only public so it can be used by ProcessHandle and
  // by test classes.
  class CONTENT_EXPORT WorkletProcess : public base::RefCounted<WorkletProcess>,
                                        public RenderProcessHostObserver {
   public:
    // The Mojo pipe and related data passed in when attaching a process to a
    // WorkletProcess. To make mocking easier, and given the laundry list of
    // parameters, WorkletProcesses are created without a service pipe. The
    // AuctionWorkletProcessManager subclass then immediately passes in a
    // ProcessContext to the WorkletProcess.
    struct CONTENT_EXPORT ProcessContext {
      explicit ProcessContext(
          mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService>
              service,
          RenderProcessHost* render_process_host = nullptr);
      ProcessContext(ProcessContext&&);
      ~ProcessContext();

      mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService>
          service;

      // May only be non-null when the WorkletProcess was created with a
      // non-null ServiceInstance.
      raw_ptr<RenderProcessHost> render_process_host;
    };

    // `is_idle` indicates whether the process will be immediately used. If not,
    // a timer is started, and if it triggers before ActivateAndBindIfUnbound()
    // is invoked, the AuctionProcessManager is told to delete process.
    //
    // `is_bound_to_origin` indicates if the process may only be used for the
    // specified worklet_type and origin, or if it may be used for other values
    // if needed. Only newly created idle processes may not be bound to an
    // origin.
    WorkletProcess(
        AuctionProcessManager* auction_process_manager,
        scoped_refptr<SiteInstance> site_instance,
        WorkletType worklet_type,
        const url::Origin& origin,
        bool uses_shared_process,
        bool is_idle,
        bool is_bound_to_origin);

    auction_worklet::mojom::AuctionWorkletService* GetService();

    WorkletType worklet_type() const { return worklet_type_; }

    const url::Origin& origin() const { return origin_; }

    bool is_bound_to_origin() const { return is_bound_to_origin_; }

    RenderProcessHost* render_process_host() const {
      return render_process_host_;
    }

    std::optional<base::ProcessId> GetPid(
        base::OnceCallback<void(base::ProcessId)> callback);

    bool HasPid() const;

    void OnLaunchedWithProcess(const base::Process& process);

    // Sets the worklet type and origin to these values, without
    // binding this process to the values.
    // This function may only be called on an unbound process
    // (i.e. `is_bound_to_origin` is false).
    void ReassignWorkletTypeAndOrigin(WorkletType worklet_type,
                                      const url::Origin& origin);

    // Sets this process non-idle. Binds the worklet type and origin to these
    // values if this process was not already bound to an origin and type. This
    // function should only be called on an idle process.
    void ActivateAndBindIfUnbound(WorkletType worklet_type,
                                  const url::Origin& origin);

    SiteInstance* site_instance() { return site_instance_.get(); }

    // Returns a weak pointer so that tests can hold onto a pointer to the
    // WorkletProcess without affecting lifetimes.
    base::WeakPtr<WorkletProcess> GetWeakPtrForTesting() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    friend class base::RefCounted<WorkletProcess>;
    friend class DedicatedAuctionProcessManager;
    friend class InRendererAuctionProcessManager;

    // Used to set the Mojo service. Called immediately after construction.
    void SetService(ProcessContext service_context);

    // From RenderProcessHostObserver:
    void RenderProcessReady(RenderProcessHost* host) override;

    void RenderProcessHostDestroyed(RenderProcessHost* host) override;

    void RemoveFromProcessManager(bool on_destruction);

    ~WorkletProcess() override;

    // Must be called when setting `is_bound_to_origin_` to true, if
    // SetService() has already been invoked. At that point, the origin the
    // service is bound to will not change, so this method can pass an
    // origin-bound TrustedSignalsCache Mojo pipe to `service_`, if the cache is
    // enabled.
    void OnBoundToOrigin();

    raw_ptr<RenderProcessHost> render_process_host_;

    // SiteInstance representing the worklet. Used only by
    // InRendererAuctionProcessManager.
    scoped_refptr<SiteInstance> site_instance_;

    WorkletType worklet_type_;
    url::Origin origin_;
    const base::TimeTicks start_time_;
    bool uses_shared_process_;

    std::optional<base::ProcessId> pid_;
    std::vector<base::OnceCallback<void(base::ProcessId)>> waiting_for_pid_;

    // nulled out once OnWorkletProcessUnusable() called.
    raw_ptr<AuctionProcessManager> auction_process_manager_;

    mojo::Remote<auction_worklet::mojom::AuctionWorkletService> service_;

    // Whether the process is idle or not. If idle, it is owned directly by the
    // AuctionProcessManager. If not, it is held by one or more
    // ProcessHandles as scoped_refptrs.
    bool is_idle_;

    // Whether the origin and worklet type are bound to this process. If this
    // worklet has ever been used, or if it's a renderer process, the origin and
    // type must be bound. Once bound, a WorkletProcess may never become
    // unbound. ReassignWorkletTypeAndOrigin() may only be called on an unbound
    // process.
    bool is_bound_to_origin_;

    // When a process is set idle, this timer will start to delete it after a
    // fixed time to prevent holding onto unnecessary unused processes for too
    // long. The timer will be cancelled if the process is set non-idle.
    base::OneShotTimer remove_idle_process_from_manager_timer_;

    base::WeakPtrFactory<WorkletProcess> weak_ptr_factory_{this};
  };

  // Class that tracks a request for an auction worklet process, and manages
  // lifetime of the returned process once the request receives a process.
  // Destroying the handle will abort a pending request and release any process
  // it is keeping alive, so consumers should destroy these as soon as a process
  // is no longer needed.
  //
  // A single process can be referenced by multiple handles.
  class CONTENT_EXPORT ProcessHandle {
   public:
    ProcessHandle();
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ~ProcessHandle();

    // Returns a non-null pointer once a ProcessHandle has been assigned a
    // process. The pipe, however, may get broken if the process exits.
    auction_worklet::mojom::AuctionWorkletService* GetService();

    // Returns any RenderProcessHost being used to host this process, or
    // nullptr.
    RenderProcessHost* GetRenderProcessHostForTesting();

    WorkletType worklet_type() const { return worklet_type_; }

    const url::Origin& origin() const { return origin_; }

    // Returns the underlying process assignment at this level.
    // Meant for reference-equality testing.
    const scoped_refptr<WorkletProcess>& worklet_process_for_testing() const {
      return worklet_process_;
    }

    const scoped_refptr<SiteInstance>& site_instance_for_testing() const {
      return site_instance_;
    }

    // Looks up which PID (from browser's perspective) this process is running
    // in. If it's available immediately, it's returned. If not, nullopt is
    // returned and |callback| will be invoked when it's available. Should not
    // be called if the process hasn't been assigned yet.
    std::optional<base::ProcessId> GetPid(
        base::OnceCallback<void(base::ProcessId)> callback);

    // Tests can call this function to configure this ProcessHandle's worklet
    // process's PID to this process.
    void OnBaseProcessLaunchedForTesting(const base::Process& process) const;

   private:
    friend class AuctionProcessManager;
    friend class InRendererAuctionProcessManager;
    friend class DedicatedAuctionProcessManager;

    // Assigns `worklet_process` to `this`. If `callback_` is non-null, queues a
    // task to invoke it asynchronously, and GetService() will return nullptr
    // until its invoked, so the consumer sees a consistent picture of the
    // world. Destroying the Handle will cancel the pending callback.
    void AssignProcess(scoped_refptr<WorkletProcess> worklet_process);

    void InvokeCallback();

    base::OnceClosure callback_;
    url::Origin origin_;
    WorkletType worklet_type_;

    // SiteInstance representing the worklet. Used only by
    // InRendererAuctionProcessManager.
    scoped_refptr<SiteInstance> site_instance_;

    // Associated AuctionProcessManager. Set when a process is requested,
    // cleared once a process is assigned (synchronously or asynchronously),
    // since the AuctionProcessManager doesn't track Handles after they've been
    // assigned processes - it tracks processes instead, at that point.
    raw_ptr<AuctionProcessManager> manager_ = nullptr;

    scoped_refptr<WorkletProcess> worklet_process_;

    // Entry in the corresponding PendingRequestQueue if the handle has yet to
    // be assigned a process.
    std::list<raw_ptr<ProcessHandle, CtnExperimental>>::iterator
        queued_request_;

    base::WeakPtrFactory<ProcessHandle> weak_ptr_factory_{this};
  };

  AuctionProcessManager(const AuctionProcessManager&) = delete;
  AuctionProcessManager& operator=(const AuctionProcessManager&) = delete;
  virtual ~AuctionProcessManager();

  // Requests a worklet service instance for a worklet with the specified
  // properties.
  //
  // If a process is synchronously assigned to the ProcessHandle, returns true
  // and the service pointer can immediately be retrieved from `process_handle`.
  // `callback` will not be invoked. Otherwise, returns false and will invoke
  // `callback` when the service pointer can be retrieved from `process_handle`.
  //
  // Auctions must request (and get) a service for their `kSeller` worklet
  // before requesting any `kBidder` worklets to avoid deadlock.
  //
  // `frame_site_instance` must be the SiteInstance of the frame that requested
  // the auction. It's only examined by InRendererAuctionProcessManager.
  //
  // Passed in ProcessHandles must be destroyed before the AuctionProcessManager
  // is. ProcessHandles may not be reused.
  //
  // While `callback` is being invoked, it is fine to call into the
  // AuctionProcessManager to request more WorkletServices, or even to delete
  // the AuctionProcessManager, since nothing but the callback invocation is on
  // the call stack.
  [[nodiscard]] bool RequestWorkletService(
      WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> frame_site_instance,
      ProcessHandle* process_handle,
      base::OnceClosure callback);

  // Start an anticipatory process for an origin if
  // 1) we have not yet started one for that buyer or seller origin and
  // 2) we cannot use a shared process and
  // 3) we have not yet reached the quota for the number of processes.
  // An anticipatory process is a process for which we do not yet need
  // a worklet; however, we anticipate that we will need a
  // worklet for this origin later. This process will be owned by this
  // AuctionProcessManger until it is needed.
  void MaybeStartAnticipatoryProcess(const url::Origin& origin,
                                     SiteInstance* frame_site_instance,
                                     WorkletType worklet_type);

  size_t GetPendingBidderRequestsForTesting() const {
    return pending_bidder_request_queue_.size();
  }
  size_t GetPendingSellerRequestsForTesting() const {
    return pending_seller_request_queue_.size();
  }
  // Returns the count of non-idle bidder processes.
  size_t GetBidderProcessCountForTesting() const {
    return bidder_processes_.size();
  }
  // Returns the count of non-idle seller processes.
  size_t GetSellerProcessCountForTesting() const {
    return seller_processes_.size();
  }
  // Returns the count of idle processes, including for both bidders and
  // sellers.
  size_t GetIdleProcessCountForTesting() const {
    return idle_processes_.size();
  }

 protected:
  // `trusted_signals_cache` must outlive the AuctionProcessManager. Passing in
  // a null cache means that there's no in-process KVv2 cache, because the
  // feature is disabled.
  explicit AuctionProcessManager(
      TrustedSignalsCacheImpl* trusted_signals_cache);

  // Launches the actual process. The process will be kept-alive and
  // watched by the returned WorkletProcess.
  virtual scoped_refptr<WorkletProcess> LaunchProcess(
      WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> site_instance,
      bool is_idle) = 0;

  // Hook called when a new process is assigned at the end of
  // TryCreateOrGetProcessForHandle. This function is used for testing.
  virtual void OnNewProcessAssigned(const ProcessHandle* process_handle) {}

  // Used to compute the value of `site_instance_` field of ProcessHandle.
  // A subclass can return nullptr if it is not using SiteInstance to place
  // worklets in appropriate renderers, but some other mechanism implementing a
  // policy that's at least as strong as site isolation would be.
  virtual scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) = 0;

  // Tries to see if a shared process can be used for this, which will bypass
  // the normal accounting logic and just use it. If it returns true, the
  // process got assigned synchronously. There is no async case.
  //
  // `process_handle` will be already filled.
  virtual bool TryUseSharedProcess(ProcessHandle* process_handle) = 0;

 private:
  // Contains ProcessHandles which have not yet been assigned processes.
  // Processes requested the earliest are at the start of the list, so processes
  // can be assigned in FIFO order as process slots become available. A list is
  // used to allow removal of cancelled requests, or requests that are assigned
  // processes out of order (which happens in the case of bidder worklets when a
  // bidder further up the queue with a matching owner receives a process).
  // ProcessHandles are owned by consumers, and destroyed when they no longer
  // need to keep their processes alive.
  using PendingRequestQueue =
      std::list<raw_ptr<ProcessHandle, CtnExperimental>>;

  // Contains ProcessHandles for bidder or seller requests which have not yet
  // been assigned processes, indexed by origin. When the request in the
  // PendingRequestQueue is assigned a process, all requests that can use the
  // same process are assigned the same process. This map is used to manage that
  // without searching through the entire queue.
  using PendingRequestMap =
      std::map<url::Origin, std::set<raw_ptr<ProcessHandle, SetExperimental>>>;

  // Contains running processes. Worklet processes are refcounted, and
  // automatically remove themselves from this list when destroyed.
  using ProcessMap =
      std::map<url::Origin, raw_ptr<WorkletProcess, CtnExperimental>>;

  RequestWorkletServiceOutcome RequestWorkletServiceInternal(
      WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> frame_site_instance,
      ProcessHandle* process_handle);

  // Tries to reuse an existing process for `process_handle` or create a new
  // one. `process_handle`'s WorkletType and Origin must be populated. Respects
  // the bidder and seller limits.
  RequestWorkletServiceOutcome TryCreateOrGetProcessForHandle(
      ProcessHandle* process_handle);

  // Attempts to get an idle process from `idle_processes_`
  // to use with the handle.
  bool TryToUseIdleProcessForHandle(ProcessHandle* process_handle);

  // Invoked by ProcessHandle's destructor, if it has previously been passed to
  // RequestWorkletService(). Checks if a new seller worklet can be created.
  void OnProcessHandleDestroyed(ProcessHandle* process_handle);

  // Removes `process_handle` from the `pending_bidder_requests_` or
  // `pending_seller_requests_`, as appropriate. `process_handle` must be in one
  // of those maps.
  void RemovePendingProcessHandle(ProcessHandle* process_handle);

  // Invoked when WorkletProcess can no longer handle new requests, either
  // because it was destroyed or because the underlying process died. Updates
  // the corresponding ProcessMap, and checks if a new bidder process should be
  // started.
  void OnWorkletProcessUnusable(WorkletProcess* worklet_process);

  // Callback to call after an idle process times out so that we can
  // release our hold of it.
  void ReleaseIdleProcess(WorkletProcess* worklet_process);

  // Helpers to access the maps of the corresponding worklet type.
  PendingRequestQueue* GetPendingRequestQueue(WorkletType worklet_type);
  PendingRequestMap* GetPendingRequestMap(WorkletType worklet_type);
  ProcessMap* Processes(WorkletType worklet_type);

  // Returns true if there's an available slot for an active process of the
  // specified worklet type.
  bool HasAvailableProcessSlotForActiveProcess(WorkletType worklet_type) const;

  // Returns true if there's an available slot for an idle process of the
  // specified worklet type.
  bool HasAvailableProcessSlotForIdleProcess(
      WorkletType worklet_type,
      size_t num_idle_processes_of_type) const;

  raw_ptr<TrustedSignalsCacheImpl> trusted_signals_cache_;

  PendingRequestQueue pending_bidder_request_queue_;
  PendingRequestQueue pending_seller_request_queue_;

  PendingRequestMap pending_bidder_requests_;
  PendingRequestMap pending_seller_requests_;

  ProcessMap bidder_processes_;
  ProcessMap seller_processes_;

  // Idle processes sorted by creation time. These are processes that
  // are not being actively used as a worklet but are on stand-by in case they
  // are needed.
  std::vector<scoped_refptr<WorkletProcess>> idle_processes_;

  base::WeakPtrFactory<AuctionProcessManager> weak_ptr_factory_{this};
};

// An implementation of AuctionProcessManager that places worklet execution into
// dedicated utility processes, isolated by domain and role.
class CONTENT_EXPORT DedicatedAuctionProcessManager
    : public AuctionProcessManager {
 public:
  explicit DedicatedAuctionProcessManager(
      TrustedSignalsCacheImpl* trusted_signals_cache);
  ~DedicatedAuctionProcessManager() override;

 protected:
  // Virtual for testing. Takes `worklet_process` so that test classes can get
  // WeakPtrs to it for creation order tracking.
  virtual WorkletProcess::ProcessContext CreateProcessInternal(
      WorkletProcess& worklet_process);

 private:
  // AuctionProcessManager implementation:
  scoped_refptr<WorkletProcess> LaunchProcess(
      WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> site_instance,
      bool is_idle) override;
  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override;
  bool TryUseSharedProcess(ProcessHandle* process_handle) override;
};

// An alternative implementation of AuctionProcessManager that places worklet
// execution into regular renderer processes (rather than worklet-only utility
// processes) following the site isolation policy.
class CONTENT_EXPORT InRendererAuctionProcessManager
    : public AuctionProcessManager {
 public:
  explicit InRendererAuctionProcessManager(
      TrustedSignalsCacheImpl* trusted_signals_cache);
  ~InRendererAuctionProcessManager() override;

 protected:
  // Virtual for testing. Takes `worklet_process` so that test classes can get
  // WeakPtrs to it for creation order tracking.
  virtual WorkletProcess::ProcessContext CreateProcessInternal(
      WorkletProcess& worklet_process);

 private:
  // AuctionProcessManager implementation:
  scoped_refptr<WorkletProcess> LaunchProcess(
      WorkletType worklet_type,
      const url::Origin& origin,
      scoped_refptr<SiteInstance> site_instance,
      bool is_idle) override;
  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override;
  bool TryUseSharedProcess(ProcessHandle* process_handle) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_

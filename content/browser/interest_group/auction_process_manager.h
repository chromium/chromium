// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/origin.h"

namespace content {

// Per-StoragePartition manager of auction bidder and seller worklet processes.
// Limits the total number of worklet process at once. Processes for the same
// origin will be vended to multiple consumers.
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

  // Refcounted class that creates / holds Mojo Remote for an
  // AuctionWorkletService. Only public so it can be used by ProcessHandle.
  class WorkletProcess;

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

   private:
    friend AuctionProcessManager;

    // Assigns `worklet_process` to `this`. If `callback_` is non-null, queues a
    // task to invoke it asynchronously, and GetService() will return nullptr
    // until its invoked, so the consumer sees a consistent picture of the
    // world. Destroying the Handle will cancel the pending callback.
    void AssignProcess(scoped_refptr<WorkletProcess> worklet_process);

    void InvokeCallback();

    base::OnceClosure callback_;
    url::Origin origin_;
    WorkletType worklet_type_;

    // Associated AuctionProcessManager. Set when a process is requested,
    // cleared once a process is assigned (synchronously or asynchronously),
    // since the AuctionProcessManager doesn't track Handles after they've been
    // assigned processes - it tracks processes instead, at that point.
    raw_ptr<AuctionProcessManager> manager_ = nullptr;

    scoped_refptr<WorkletProcess> worklet_process_;

    // Entry in the corresponding PendingRequestQueue if the handle has yet to
    // be assigned a process.
    std::list<ProcessHandle*>::iterator queued_request_;

    base::WeakPtrFactory<ProcessHandle> weak_ptr_factory_{this};
  };

  AuctionProcessManager();
  AuctionProcessManager(const AuctionProcessManager&) = delete;
  AuctionProcessManager& operator=(const AuctionProcessManager&) = delete;
  virtual ~AuctionProcessManager();

  // Requests a worklet service instance for a worklet with the specified
  // properties.
  //
  // If a process is synchronously assigned to the ProcessHandle, returns true
  // and the service pointer can immediately be retrieved from `handle`.
  // `callback` will not be invoked. Otherwise, returns false and will invoke
  // `callback` when the service pointer can be retrieved from `handle`.
  //
  // Auctions must request (and get) a service for their `kSeller` worklet
  // before requesting any `kBidder` worklets to avoid deadlock.
  //
  // Passed in ProcessHandles must be destroyed before the AuctionProcessManager
  // is. ProcessHandles may not be reused.
  //
  // While `callback` is being invoked, it is fine to call into the
  // AuctionProcessManager to request more WorkletServices, or even to delete
  // the AuctionProcessManager, since nothing but the callback invocation is on
  // the call stack.
  [[nodiscard]] bool RequestWorkletService(WorkletType worklet_type,
                                           const url::Origin& origin,
                                           ProcessHandle* process_handle,
                                           base::OnceClosure callback);

  size_t GetPendingBidderRequestsForTesting() const {
    return pending_bidder_request_queue_.size();
  }
  size_t GetPendingSellerRequestsForTesting() const {
    return pending_seller_request_queue_.size();
  }
  size_t GetBidderProcessCountForTesting() const {
    return bidder_processes_.size();
  }
  size_t GetSellerProcessCountForTesting() const {
    return seller_processes_.size();
  }

 protected:
  // Launches the actual process. Virtual so tests can override it.
  virtual void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name);

  // Returns the display name to use for a process. Separate method so it can be
  // used in tests.
  static std::string ComputeDisplayName(WorkletType worklet_type,
                                        const url::Origin& origin);

 private:
  // Contains ProcessHandles which have not yet been assigned processes.
  // Processes requested the earliest are at the start of the list, so processes
  // can be assigned in FIFO order as process slots become available. A list is
  // used to allow removal of cancelled requests, or requests that are assigned
  // processes out of order (which happens in the case of bidder worklets when a
  // bidder further up the queue with a matching owner receives a process).
  // ProcessHandles are owned by consumers, and destroyed when they no longer
  // need to keep their processes alive.
  using PendingRequestQueue = std::list<ProcessHandle*>;

  // Contains ProcessHandles for bidder or seller requests which have not yet
  // been assigned processes, indexed by origin. When the request in the
  // PendingRequestQueue is assigned a process, all requests that can use the
  // same process are assigned the same process. This map is used to manage that
  // without searching through the entire queue.
  using PendingRequestMap = std::map<url::Origin, std::set<ProcessHandle*>>;

  // Contains running processes. Worklet processes are refcounted, and
  // automatically remove themselves from this list when destroyed.
  using ProcessMap = std::map<url::Origin, WorkletProcess*>;

  // Tries to reuse an existing process for `process_handle` or create a new
  // one. `process_handle`'s WorkletType and Origin must be populated. Respects
  // the bidder and seller limits.
  bool TryCreateOrGetProcessForHandle(ProcessHandle* process_handle);

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

  // Helpers to access the maps of the corresponding worklet type.
  PendingRequestQueue* GetPendingRequestQueue(WorkletType worklet_type);
  PendingRequestMap* GetPendingRequestMap(WorkletType worklet_type);
  ProcessMap* Processes(WorkletType worklet_type);

  // Returns true if there's an available slot of the specified worklet type.
  bool HasAvailableProcessSlot(WorkletType worklet_type) const;

  PendingRequestQueue pending_bidder_request_queue_;
  PendingRequestQueue pending_seller_request_queue_;

  PendingRequestMap pending_bidder_requests_;
  PendingRequestMap pending_seller_requests_;

  ProcessMap bidder_processes_;
  ProcessMap seller_processes_;

  base::WeakPtrFactory<AuctionProcessManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_PROCESS_MANAGER_H_

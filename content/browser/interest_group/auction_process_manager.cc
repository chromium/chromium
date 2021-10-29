// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/public/browser/service_process_host.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/origin.h"

namespace content {

constexpr size_t AuctionProcessManager::kMaxBidderProcesses = 10;
constexpr size_t AuctionProcessManager::kMaxActiveSellerWorklets = 3;

class AuctionProcessManager::WorkletProcess
    : public base::RefCounted<WorkletProcess> {
 public:
  WorkletProcess(AuctionProcessManager* auction_process_manager,
                 WorkletType worklet_type,
                 const url::Origin& origin)
      : worklet_type_(worklet_type),
        origin_(origin),
        auction_process_manager_(auction_process_manager) {
    DCHECK(auction_process_manager);
  }

  auction_worklet::mojom::AuctionWorkletService* GetService() {
    if (!service_.is_bound() || !service_.is_connected()) {
      service_.reset();
      auction_process_manager_->LaunchProcess(
          service_.BindNewPipeAndPassReceiver(),
          ComputeDisplayName(worklet_type_, origin_));
    }
    return service_.get();
  }

  WorkletType worklet_type() const { return worklet_type_; }
  const url::Origin origin() const { return origin_; }

 private:
  friend class base::RefCounted<WorkletProcess>;

  ~WorkletProcess() {
    auction_process_manager_->OnWorkletProcessDestroyed(this);
  }

  const WorkletType worklet_type_;
  const url::Origin origin_;
  AuctionProcessManager* const auction_process_manager_;

  mojo::Remote<auction_worklet::mojom::AuctionWorkletService> service_;
};

AuctionProcessManager::ProcessHandle::ProcessHandle() = default;

AuctionProcessManager::ProcessHandle::~ProcessHandle() {
  if (manager_)
    manager_->OnProcessHandleDestroyed(this);
}

auction_worklet::mojom::AuctionWorkletService*
AuctionProcessManager::ProcessHandle::GetService() {
  if (!worklet_process_ || callback_)
    return nullptr;
  return worklet_process_->GetService();
}

void AuctionProcessManager::ProcessHandle::AssignProcess(
    scoped_refptr<WorkletProcess> worklet_process) {
  worklet_process_ = std::move(worklet_process);
  if (callback_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessHandle::InvokeCallback,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void AuctionProcessManager::ProcessHandle::InvokeCallback() {
  DCHECK(callback_);
  std::move(callback_).Run();
}

AuctionProcessManager::AuctionProcessManager() = default;

AuctionProcessManager::~AuctionProcessManager() {
  DCHECK(pending_bidder_request_queue_.empty());
  DCHECK(pending_seller_request_queue_.empty());
  DCHECK(pending_bidder_requests_.empty());
  DCHECK(bidder_processes_.empty());
  DCHECK(seller_processes_.empty());
  DCHECK_EQ(0u, num_active_seller_worklets_);
}

bool AuctionProcessManager::RequestWorkletService(WorkletType worklet_type,
                                                  const url::Origin& origin,
                                                  ProcessHandle* process_handle,
                                                  base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  // `process_handle` should not already be in use.
  DCHECK(!process_handle->manager_);
  DCHECK(!process_handle->callback_);
  DCHECK(!process_handle->worklet_process_);

  process_handle->manager_ = this;
  process_handle->origin_ = origin;
  process_handle->worklet_type_ = worklet_type;

  // If can assign a process to the handle instantly, nothing else to do.
  if (TryCreateOrGetProcessForHandle(process_handle))
    return true;

  PendingRequestQueue* pending_requests = GetPendingRequestQueue(worklet_type);
  pending_requests->push_back(process_handle);
  process_handle->queued_request_ = std::prev(pending_requests->end());
  process_handle->callback_ = std::move(callback);

  // Bidder processes are also tracked in map, to aid in the bidder process
  // assignment logic.
  if (worklet_type == WorkletType::kBidder)
    pending_bidder_requests_[origin].insert(process_handle);

  return false;
}

bool AuctionProcessManager::TryCreateOrGetProcessForHandle(
    ProcessHandle* process_handle) {
  if (process_handle->worklet_type_ == WorkletType::kSeller) {
    // If this is a seller worklet and the seller limit has been hit, nothing
    // else to do - can't reuse a process even if there's a match. See
    // discussion on deadlock in header file.
    if (num_active_seller_worklets_ >= kMaxActiveSellerWorklets) {
      DCHECK_EQ(num_active_seller_worklets_, kMaxActiveSellerWorklets);
      return false;
    }

    // Otherwise, a process will be assigned to the seller worklet (either a new
    // one or a pre-existing one).
    ++num_active_seller_worklets_;
  }

  // Look for a pre-existing matching process.
  ProcessMap* processes = Processes(process_handle->worklet_type_);
  auto process_it = processes->find(process_handle->origin_);
  if (process_it != processes->end()) {
    // If there's a matching process, assign it.
    process_handle->AssignProcess(WrapRefCounted(process_it->second));
    return true;
  }

  // If this is for a bidder worklet, and the bidder process limit has been hit,
  // can't create a new process.
  if (process_handle->worklet_type_ == WorkletType::kBidder &&
      bidder_processes_.size() == kMaxBidderProcesses) {
    return false;
  }

  // Create WorkletProcess object. It will lazily create the actual process on
  // the first GetService() call.
  scoped_refptr<WorkletProcess> worklet_process =
      base::MakeRefCounted<WorkletProcess>(this, process_handle->worklet_type_,
                                           process_handle->origin_);
  (*processes)[process_handle->origin_] = worklet_process.get();
  process_handle->AssignProcess(std::move(worklet_process));
  return true;
}

void AuctionProcessManager::LaunchProcess(
    mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
        auction_worklet_service_receiver,
    const std::string& display_name) {
  content::ServiceProcessHost::Launch(
      std::move(auction_worklet_service_receiver),
      ServiceProcessHost::Options().WithDisplayName(display_name).Pass());
}

std::string AuctionProcessManager::ComputeDisplayName(
    WorkletType worklet_type,
    const url::Origin& origin) {
  // Use origin and whether it's a buyer/seller in display in task manager,
  // though admittedly, worklet processes should hopefully not be around too
  // long.
  std::string display_name;
  if (worklet_type == WorkletType::kBidder) {
    display_name = "Auction Bidder Worklet: ";
  } else {
    display_name = "Auction Seller Worklet: ";
  }
  return display_name + origin.Serialize();
}

void AuctionProcessManager::OnProcessHandleDestroyed(
    ProcessHandle* process_handle) {
  // If the ProcessHandle has been assigned a WorkletProcess, update the number
  // of seller slots if it's a seller worklet. For bidder worklets, nothing to
  // do, since process creation/assignment happens when the process is destroyed
  // instead.
  if (process_handle->worklet_process_) {
    if (process_handle->worklet_type_ == WorkletType::kSeller) {
      DCHECK_GT(num_active_seller_worklets_, 0u);
      --num_active_seller_worklets_;
      OnSellerWorkletSlotFreed();
    }
    return;
  }

  // Otherwise, the ProcessHandle is in the corresponding list of pending
  // handles.
  DCHECK(process_handle->callback_);
  RemovePendingProcessHandle(process_handle);
}

void AuctionProcessManager::RemovePendingProcessHandle(
    ProcessHandle* process_handle) {
  DCHECK(!process_handle->worklet_process_);

  // Remove the ProcessHandle from internal data structure(s) tracking it. No
  // need to do anything else, as the handle hadn't yet been assigned a process.

  PendingRequestQueue* pending_request_queue =
      GetPendingRequestQueue(process_handle->worklet_type_);
  pending_request_queue->erase(process_handle->queued_request_);
  // Clear the iterator, which will hopefully make crashes more likely if it's
  // accidentally used again.
  process_handle->queued_request_ = PendingRequestQueue::iterator();

  // Bidder requests must also be removed from the map.
  if (process_handle->worklet_type_ == WorkletType::kBidder) {
    auto it = pending_bidder_requests_.find(process_handle->origin_);
    DCHECK(it != pending_bidder_requests_.end());
    DCHECK_EQ(1u, it->second.count(process_handle));

    it->second.erase(process_handle);
    if (it->second.empty())
      pending_bidder_requests_.erase(it);
  }
}

void AuctionProcessManager::OnWorkletProcessDestroyed(
    WorkletProcess* worklet_process) {
  ProcessMap* processes = Processes(worklet_process->worklet_type());
  auto it = processes->find(worklet_process->origin());
  DCHECK(it != processes->end());
  processes->erase(it);

  // May need to launch another bidder process at this point. Pending seller
  // requests are handled in RemovePendingProcessHandle() because of the
  // differing logic.
  if (worklet_process->worklet_type() == WorkletType::kBidder)
    OnBidderProcessDestroyed();
}

void AuctionProcessManager::OnSellerWorkletSlotFreed() {
  // This method is currently only called once a seller worklet slot is freed.
  DCHECK_LT(num_active_seller_worklets_, kMaxActiveSellerWorklets);

  // Nothing to do if no pending seller requests.
  if (pending_seller_request_queue_.empty())
    return;

  // Since the queue wasn't empty before, and only one seller worklet was freed,
  // there must now be exactly one free seller worklet slot.
  DCHECK_EQ(num_active_seller_worklets_, kMaxActiveSellerWorklets - 1);

  ProcessHandle* process_handle = pending_seller_request_queue_.front();

  // Remove the process handle from the list of pending requests.
  RemovePendingProcessHandle(process_handle);

  // Create or reuse a process handle for the request.
  bool process_created = TryCreateOrGetProcessForHandle(process_handle);
  // This follows from the DCHECK at the start of this method.
  DCHECK(process_created);
  // There should now be no free seller worklet slots.
  DCHECK_EQ(num_active_seller_worklets_, kMaxActiveSellerWorklets);

  // Nothing else to do after assigning the process - assigning a process
  // results in the callback being invoked asynchonrously.
}

void AuctionProcessManager::OnBidderProcessDestroyed() {
  // This method is currently only called once a bidder worklet is closed.
  DCHECK_LT(bidder_processes_.size(), kMaxBidderProcesses);

  // Nothing to do if no pending bidder requests.
  if (pending_bidder_request_queue_.empty())
    return;

  // Walk through all requests that can be served by the same process as the
  // next bidder process in the queue, assigning them a process. This code does
  // not walk through them in FIFO order. Network response order matters most
  // here, but that will likely be influenced by callback invocation order.
  //
  // TODO(mmenke): Consider assigning processes to these matching requests in
  // FIFO order.

  std::set<ProcessHandle*>* pending_requests =
      &pending_bidder_requests_[pending_bidder_request_queue_.front()->origin_];

  // Have to record the number of requests and iterate on that, as
  // `pending_requests` will be deleted when the last request is removed.
  size_t num_matching_requests = pending_requests->size();
  DCHECK_GT(num_matching_requests, 0u);

  while (num_matching_requests > 0) {
    ProcessHandle* process_handle = *pending_requests->begin();

    RemovePendingProcessHandle(process_handle);

    // This should always succeed for the fist request because
    // `bidder_processes_` is less than kMaxBidderProcesses. Subsequent requests
    // will just receive the process created for the first request. Could cache
    // the process returned by the first request and reuse it, but doesn't seem
    // worth the effort.
    bool process_created = TryCreateOrGetProcessForHandle(process_handle);
    DCHECK(process_created);
    --num_matching_requests;

    // Nothing else to do after assigning the process - assigning a process
    // results in the callback being invoked asynchonrously.
  }
}

AuctionProcessManager::PendingRequestQueue*
AuctionProcessManager::GetPendingRequestQueue(WorkletType worklet_type) {
  if (worklet_type == WorkletType::kBidder)
    return &pending_bidder_request_queue_;
  return &pending_seller_request_queue_;
}

AuctionProcessManager::ProcessMap* AuctionProcessManager::Processes(
    WorkletType worklet_type) {
  if (worklet_type == WorkletType::kBidder)
    return &bidder_processes_;
  return &seller_processes_;
}

}  // namespace content

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/origin.h"

namespace content {

constexpr size_t AuctionProcessManager::kMaxBidderProcesses = 10;
constexpr size_t AuctionProcessManager::kMaxSellerProcesses = 3;

class AuctionProcessManager::WorkletProcess
    : public base::RefCounted<WorkletProcess> {
 public:
  WorkletProcess(
      AuctionProcessManager* auction_process_manager,
      mojo::Remote<auction_worklet::mojom::AuctionWorkletService> service,
      WorkletType worklet_type,
      const url::Origin& origin)
      : worklet_type_(worklet_type),
        origin_(origin),
        auction_process_manager_(auction_process_manager),
        service_(std::move(service)) {
    DCHECK(auction_process_manager);
    service_.set_disconnect_handler(base::BindOnce(
        &WorkletProcess::NotifyUnusableOnce, base::Unretained(this)));
  }

  auction_worklet::mojom::AuctionWorkletService* GetService() {
    DCHECK(service_.is_connected());
    return service_.get();
  }

  WorkletType worklet_type() const { return worklet_type_; }
  const url::Origin& origin() const { return origin_; }

 private:
  friend class base::RefCounted<WorkletProcess>;

  void NotifyUnusableOnce() {
    AuctionProcessManager* maybe_apm = auction_process_manager_;
    // Clear `auction_process_manager_` to make sure OnWorkletProcessUnusable()
    // is called once.  Clear it before call to ensure this is the case even
    // if this method is re-entered somehow.
    auction_process_manager_ = nullptr;
    if (maybe_apm)
      maybe_apm->OnWorkletProcessUnusable(this);
  }

  ~WorkletProcess() { NotifyUnusableOnce(); }

  const WorkletType worklet_type_;
  const url::Origin origin_;

  // nulled out once OnWorkletProcessUnusable() called.
  raw_ptr<AuctionProcessManager> auction_process_manager_;

  mojo::Remote<auction_worklet::mojom::AuctionWorkletService> service_;
};

AuctionProcessManager::ProcessHandle::ProcessHandle() = default;

AuctionProcessManager::ProcessHandle::~ProcessHandle() {
  if (manager_) {
    // `manager_` should only be non-null if the handle is waiting for a
    // process.
    DCHECK(callback_);
    manager_->RemovePendingProcessHandle(this);
  }
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

  // No longer needed.
  manager_ = nullptr;

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
  DCHECK(pending_seller_requests_.empty());
  DCHECK(bidder_processes_.empty());
  DCHECK(seller_processes_.empty());
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

  // Pending requests are also tracked in a map, to aid in the bidder process
  // assignment logic.
  (*GetPendingRequestMap(worklet_type))[origin].insert(process_handle);

  return false;
}

bool AuctionProcessManager::TryCreateOrGetProcessForHandle(
    ProcessHandle* process_handle) {
  // Look for a pre-existing matching process.
  ProcessMap* processes = Processes(process_handle->worklet_type_);
  auto process_it = processes->find(process_handle->origin_);
  if (process_it != processes->end()) {
    // If there's a matching process, assign it.
    process_handle->AssignProcess(WrapRefCounted(process_it->second));
    return true;
  }

  // If the corresponding process limit has been hit, can't create a new
  // process.
  if (!HasAvailableProcessSlot(process_handle->worklet_type_))
    return false;

  // Launch the process and create WorkletProcess object bound to it.
  mojo::Remote<auction_worklet::mojom::AuctionWorkletService> service;
  LaunchProcess(service.BindNewPipeAndPassReceiver(),
                ComputeDisplayName(process_handle->worklet_type_,
                                   process_handle->origin_));

  scoped_refptr<WorkletProcess> worklet_process =
      base::MakeRefCounted<WorkletProcess>(this, std::move(service),
                                           process_handle->worklet_type_,
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
      ServiceProcessHost::Options()
          .WithDisplayName(display_name)
#if BUILDFLAG(IS_MAC)
          // TODO(https://crbug.com/1281311) add a utility helper for Jit.
          .WithChildFlags(ChildProcessHost::CHILD_RENDERER)
#endif
          .Pass());
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

  // Requests must also be removed from the map.
  PendingRequestMap* pending_request_map =
      GetPendingRequestMap(process_handle->worklet_type_);
  auto it = pending_request_map->find(process_handle->origin_);
  DCHECK(it != pending_request_map->end());
  DCHECK_EQ(1u, it->second.count(process_handle));
  it->second.erase(process_handle);
  // If there are no more pending requests for the same origin, remove the
  // origin's entry in `pending_request_map` as well.
  if (it->second.empty())
    pending_request_map->erase(it);
}

void AuctionProcessManager::OnWorkletProcessUnusable(
    WorkletProcess* worklet_process) {
  ProcessMap* processes = Processes(worklet_process->worklet_type());
  auto it = processes->find(worklet_process->origin());
  DCHECK(it != processes->end());
  processes->erase(it);

  // May need to launch another process at this point.

  // Since a process was just destroyed, there should be at least one available
  // slot to create another.
  DCHECK(HasAvailableProcessSlot(worklet_process->worklet_type()));

  // If there are no pending requests for the corresponding worklet type,
  // nothing more to do.
  PendingRequestQueue* queue =
      GetPendingRequestQueue(worklet_process->worklet_type());
  if (queue->empty())
    return;

  // All the pending requests for the same origin as the oldest pending request.
  std::set<ProcessHandle*>* pending_requests = &(*GetPendingRequestMap(
      worklet_process->worklet_type()))[queue->front()->origin_];

  // Walk through all requests that can be served by the same process as the
  // next bidder process in the queue, assigning them a process. This code does
  // not walk through them in FIFO order. Network response order matters most
  // here, but that will likely be influenced by callback invocation order.
  //
  // TODO(mmenke): Consider assigning processes to these matching requests in
  // FIFO order.

  // Have to record the number of requests and iterate on that, as
  // `pending_requests` will be deleted when the last request is removed.
  size_t num_matching_requests = pending_requests->size();
  DCHECK_GT(num_matching_requests, 0u);

  while (num_matching_requests > 0) {
    ProcessHandle* process_handle = *pending_requests->begin();

    RemovePendingProcessHandle(process_handle);

    // This should always succeed for the first request because there's an
    // available process slot. Subsequent requests will just receive the process
    // created for the first request. Could cache the process returned by the
    // first request and reuse it, but doesn't seem worth the effort.
    bool process_created = TryCreateOrGetProcessForHandle(process_handle);
    DCHECK(process_created);
    --num_matching_requests;

    // Nothing else to do after assigning the process - assigning a process
    // results in the callback being invoked asynchronously.
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

AuctionProcessManager::PendingRequestMap*
AuctionProcessManager::GetPendingRequestMap(WorkletType worklet_type) {
  if (worklet_type == WorkletType::kBidder)
    return &pending_bidder_requests_;
  return &pending_seller_requests_;
}

bool AuctionProcessManager::HasAvailableProcessSlot(
    WorkletType worklet_type) const {
  if (worklet_type == WorkletType::kBidder)
    return bidder_processes_.size() < kMaxBidderProcesses;
  return seller_processes_.size() < kMaxSellerProcesses;
}

}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/trusted_signals_cache_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/origin.h"

namespace content {

namespace {

void RecordRequestWorkletServiceOutcomeUMA(
    AuctionProcessManager::WorkletType worklet_type,
    AuctionProcessManager::RequestWorkletServiceOutcome result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Ads.InterestGroup.Auction.",
                    worklet_type == AuctionProcessManager::WorkletType::kSeller
                        ? "Seller."
                        : "Buyer.",
                    "RequestWorkletServiceOutcome"}),
      result);
}
}  // namespace

constexpr size_t AuctionProcessManager::kMaxBidderProcesses = 10;
constexpr size_t AuctionProcessManager::kMaxSellerProcesses = 3;

AuctionProcessManager::WorkletProcess::ProcessContext::ProcessContext(
    mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service,
    RenderProcessHost* render_process_host)
    : service(std::move(service)), render_process_host(render_process_host) {}

AuctionProcessManager::WorkletProcess::ProcessContext::ProcessContext(
    ProcessContext&&) = default;

AuctionProcessManager::WorkletProcess::ProcessContext::~ProcessContext() =
    default;

AuctionProcessManager::WorkletProcess::WorkletProcess(
    AuctionProcessManager* auction_process_manager,
    scoped_refptr<SiteInstance> site_instance,
    WorkletType worklet_type,
    const url::Origin& origin,
    bool uses_shared_process,
    bool is_idle,
    bool is_bound_to_origin)
    : site_instance_(std::move(site_instance)),
      worklet_type_(worklet_type),
      origin_(origin),
      start_time_(base::TimeTicks::Now()),
      uses_shared_process_(uses_shared_process),
      auction_process_manager_(auction_process_manager),
      is_idle_(is_idle),
      is_bound_to_origin_(is_bound_to_origin) {
  DCHECK(auction_process_manager);
  // Non-idle processes must be bound.
  CHECK(is_idle_ || is_bound_to_origin_);
  // Shared processes cannot be idle.
  CHECK(!uses_shared_process || !is_idle);

  if (is_idle_) {
    remove_idle_process_from_manager_timer_.Start(
        FROM_HERE,
        features::kFledgeStartAnticipatoryProcessExpirationTime.Get(),
        base::BindOnce(&WorkletProcess::RemoveFromProcessManager,
                       base::Unretained(this),
                       /*on_destruction=*/false));
  }
}

auction_worklet::mojom::AuctionWorkletService*
AuctionProcessManager::WorkletProcess::GetService() {
  DCHECK(service_.is_connected());
  return service_.get();
}

std::optional<base::ProcessId> AuctionProcessManager::WorkletProcess::GetPid(
    base::OnceCallback<void(base::ProcessId)> callback) {
  if (pid_.has_value()) {
    return pid_;
  } else {
    waiting_for_pid_.push_back(std::move(callback));
    return std::nullopt;
  }
}

bool AuctionProcessManager::WorkletProcess::HasPid() const {
  return pid_.has_value();
}

void AuctionProcessManager::WorkletProcess::OnLaunchedWithProcess(
    const base::Process& process) {
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ProcessLaunchTime",
                          base::TimeTicks::Now() - start_time_);
  DCHECK(!pid_.has_value());
  base::ProcessId pid = process.Pid();
  pid_ = std::make_optional<base::ProcessId>(pid);
  std::vector<base::OnceCallback<void(base::ProcessId)>> waiting_for_pid =
      std::move(waiting_for_pid_);
  for (auto& callback : waiting_for_pid) {
    std::move(callback).Run(pid);
  }
}

void AuctionProcessManager::WorkletProcess::ReassignWorkletTypeAndOrigin(
    AuctionProcessManager::WorkletType worklet_type,
    const url::Origin& origin) {
  // We should only reassign the worklet type and origin of an unused unbound
  // non-shared process.
  CHECK(!is_bound_to_origin_);
  CHECK(is_idle_);
  CHECK(!uses_shared_process_);
  worklet_type_ = worklet_type;
  origin_ = origin;
}

void AuctionProcessManager::WorkletProcess::ActivateAndBindIfUnbound(
    WorkletType worklet_type,
    const url::Origin& origin) {
  DCHECK(is_idle_);
  DCHECK(!uses_shared_process_);
  if (is_bound_to_origin_) {
    CHECK_EQ(worklet_type, worklet_type_);
    CHECK_EQ(origin, origin_);
  } else {
    ReassignWorkletTypeAndOrigin(worklet_type, origin);
    is_bound_to_origin_ = true;
    OnBoundToOrigin();
  }
  is_idle_ = false;
  remove_idle_process_from_manager_timer_.Stop();
}

void AuctionProcessManager::WorkletProcess::SetService(
    ProcessContext service_context) {
  DCHECK(!service_);
  DCHECK(!render_process_host_);
  DCHECK(service_context.service);

  service_.Bind(std::move(service_context.service));
  service_.set_disconnect_handler(
      base::BindOnce(&WorkletProcess::RemoveFromProcessManager,
                     base::Unretained(this), /*on_destruction=*/false));

  if (service_context.render_process_host) {
    DCHECK(site_instance_);

    render_process_host_ = service_context.render_process_host;
    render_process_host_->IncrementWorkerRefCount();
    render_process_host_->AddObserver(this);

    // Note the PID if the process has already launched
    if (render_process_host_->IsReady()) {
      DCHECK(render_process_host_->GetProcess().IsValid());
      pid_ = render_process_host_->GetProcess().Pid();
    }
  }

  if (is_bound_to_origin_) {
    OnBoundToOrigin();
  }
}

void AuctionProcessManager::WorkletProcess::RenderProcessReady(
    RenderProcessHost* host) {
  DCHECK(render_process_host_);
  DCHECK(render_process_host_->GetProcess().IsValid());
  OnLaunchedWithProcess(render_process_host_->GetProcess());
}

void AuctionProcessManager::WorkletProcess::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_EQ(host, render_process_host_);
  RemoveFromProcessManager(/*on_destruction=*/false);
}

void AuctionProcessManager::WorkletProcess::RemoveFromProcessManager(
    bool on_destruction) {
  if (render_process_host_) {
    render_process_host_->RemoveObserver(this);
    if (!render_process_host_->AreRefCountsDisabled()) {
      render_process_host_->DecrementWorkerRefCount();
    }
    render_process_host_ = nullptr;
  }

  AuctionProcessManager* maybe_apm = auction_process_manager_;
  // Clear `auction_process_manager_` to make sure OnWorkletProcessUnusable()
  // is called once.  Clear it before call to ensure this is the case even
  // if this method is re-entered somehow.
  auction_process_manager_ = nullptr;
  if (maybe_apm) {
    if (is_idle_ && !on_destruction) {
      // Idle shared worklet processes are not allowed.
      DCHECK(!uses_shared_process_);
      // AuctionProcessManager owns idle processes, so if this
      // process is idle & being destructed, it must have been removed
      // already.
      maybe_apm->ReleaseIdleProcess(this);
    } else if (!is_idle_ && !uses_shared_process_) {
      maybe_apm->OnWorkletProcessUnusable(this);
    }
  }
}

AuctionProcessManager::WorkletProcess::~WorkletProcess() {
  RemoveFromProcessManager(/*on_destruction=*/true);
}

void AuctionProcessManager::WorkletProcess::OnBoundToOrigin() {
  DCHECK(is_bound_to_origin_);

  // If the TrustedSignalsCache exists (and thus is enabled), pass a pipe to
  // for KVv2 bidding signals fetches. Seller signals are not yet supported, so
  // only do this for bidder worklets.
  auto* trusted_signals_cache =
      auction_process_manager_->trusted_signals_cache_.get();
  if (trusted_signals_cache && worklet_type_ == WorkletType::kBidder) {
    service_->SetTrustedSignalsCache(trusted_signals_cache->CreateRemote(
        TrustedSignalsCacheImpl::SignalsType::kBidding, origin_));
  }
}

AuctionProcessManager::ProcessHandle::ProcessHandle() = default;

AuctionProcessManager::ProcessHandle::~ProcessHandle() {
  if (worklet_process_) {
    // Make sure the worklet process was not reassigned an origin or type
    // while this ProcessHandle was alive.
    DCHECK_EQ(worklet_process_->origin(), origin_);
    DCHECK_EQ(worklet_process_->worklet_type(), worklet_type_);
  }
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

RenderProcessHost*
AuctionProcessManager::ProcessHandle::GetRenderProcessHostForTesting() {
  if (!worklet_process_)
    return nullptr;
  return worklet_process_->render_process_host();
}

std::optional<base::ProcessId> AuctionProcessManager::ProcessHandle::GetPid(
    base::OnceCallback<void(base::ProcessId)> callback) {
  DCHECK(worklet_process_);
  return worklet_process_->GetPid(std::move(callback));
}

void AuctionProcessManager::ProcessHandle::AssignProcess(
    scoped_refptr<WorkletProcess> worklet_process) {
  // Only a process matching in origin and type can be assigned.
  DCHECK_EQ(worklet_process->origin(), origin_);
  DCHECK_EQ(worklet_process->worklet_type(), worklet_type_);
  worklet_process_ = std::move(worklet_process);

  // No longer needed.
  manager_ = nullptr;

  if (callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessHandle::InvokeCallback,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void AuctionProcessManager::ProcessHandle::OnBaseProcessLaunchedForTesting(
    const base::Process& process) const {
  if (worklet_process_) {
    worklet_process_->OnLaunchedWithProcess(process);
  }
}

void AuctionProcessManager::ProcessHandle::InvokeCallback() {
  DCHECK(callback_);
  std::move(callback_).Run();
}

AuctionProcessManager::~AuctionProcessManager() {
  DCHECK(pending_bidder_request_queue_.empty());
  DCHECK(pending_seller_request_queue_.empty());
  DCHECK(pending_bidder_requests_.empty());
  DCHECK(pending_seller_requests_.empty());
  DCHECK(bidder_processes_.empty());
  DCHECK(seller_processes_.empty());
}

bool AuctionProcessManager::RequestWorkletService(
    WorkletType worklet_type,
    const url::Origin& origin,
    scoped_refptr<SiteInstance> frame_site_instance,
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
  process_handle->site_instance_ =
      MaybeComputeSiteInstance(frame_site_instance.get(), origin);

  // See if a subclass can reuse existing non-auction process for this.
  //
  // This needs to be done before TryCreateOrGetProcessForHandle, since
  // shared processes really can't be keyed by origin.
  if (TryUseSharedProcess(process_handle)) {
    RecordRequestWorkletServiceOutcomeUMA(
        worklet_type, RequestWorkletServiceOutcome::kUsedSharedProcess);
    return true;
  }

  // If can assign a process to the handle instantly, nothing else to do.
  RequestWorkletServiceOutcome create_or_get_process_outcome =
      TryCreateOrGetProcessForHandle(process_handle);
  RecordRequestWorkletServiceOutcomeUMA(worklet_type,
                                        create_or_get_process_outcome);
  if (create_or_get_process_outcome !=
      RequestWorkletServiceOutcome::kHitProcessLimit) {
    return true;
  }

  PendingRequestQueue* pending_requests = GetPendingRequestQueue(worklet_type);
  pending_requests->push_back(process_handle);
  process_handle->queued_request_ = std::prev(pending_requests->end());
  process_handle->callback_ = std::move(callback);

  // Pending requests are also tracked in a map, to aid in the bidder process
  // assignment logic.
  (*GetPendingRequestMap(worklet_type))[origin].insert(process_handle);

  return false;
}

void AuctionProcessManager::MaybeStartAnticipatoryProcess(
    const url::Origin& origin,
    SiteInstance* frame_site_instance,
    WorkletType worklet_type) {
  if (!base::FeatureList::IsEnabled(
          features::kFledgeStartAnticipatoryProcesses)) {
    return;
  }

  // Don't start a process if we can use a shared process.
  // `site_instance` will be null if we're using dedicated utility processes
  // only.
  scoped_refptr<SiteInstance> site_instance =
      MaybeComputeSiteInstance(frame_site_instance, origin);
  if (site_instance && !site_instance->RequiresDedicatedProcess()) {
    return;
  }

  // Do not create a process for this origin/worklet_type if there already is
  // one (active or idle).
  ProcessMap* processes = Processes(worklet_type);
  auto process_it = processes->find(origin);
  if (process_it != processes->end()) {
    return;
  }
  // Keep track of the number of idle processes of this type.
  size_t num_idle_processes = 0;
  for (const scoped_refptr<WorkletProcess>& process : idle_processes_) {
    if (process->worklet_type() == worklet_type) {
      if (process->origin() == origin) {
        return;
      }
      num_idle_processes += 1;
    }
  }

  // Don't start a process if we've already hit the process limit.
  if (!HasAvailableProcessSlotForIdleProcess(worklet_type,
                                             num_idle_processes)) {
    return;
  }

  scoped_refptr<WorkletProcess> worklet_process = LaunchProcess(
      worklet_type, origin, std::move(site_instance), /*is_idle=*/true);
  idle_processes_.push_back(std::move(worklet_process));
}

AuctionProcessManager::RequestWorkletServiceOutcome
AuctionProcessManager::TryCreateOrGetProcessForHandle(
    ProcessHandle* process_handle) {
  // Look for a pre-existing matching process.
  ProcessMap* processes = Processes(process_handle->worklet_type_);
  auto process_it = processes->find(process_handle->origin_);
  if (process_it != processes->end()) {
    // If there's a matching process, assign it.
    process_handle->AssignProcess(WrapRefCounted(process_it->second));
    return RequestWorkletServiceOutcome::kUsedExistingDedicatedProcess;
  }

  // If the corresponding process limit has been hit, can't create a new
  // process.
  if (!HasAvailableProcessSlotForActiveProcess(process_handle->worklet_type_)) {
    return RequestWorkletServiceOutcome::kHitProcessLimit;
  }

  if (TryToUseIdleProcessForHandle(process_handle)) {
    return RequestWorkletServiceOutcome::kUsedIdleProcess;
  }

  // Making a new process shouldn't cause us to exceed the permitted number
  // of idle processes.
  DCHECK(HasAvailableProcessSlotForIdleProcess(
      process_handle->worklet_type(),
      std::count_if(
          idle_processes_.begin(), idle_processes_.end(),
          [&process_handle](const scoped_refptr<WorkletProcess>& process) {
            return process->worklet_type() == process_handle->worklet_type();
          })));

  // Launch the process and create WorkletProcess object bound to it.
  scoped_refptr<WorkletProcess> worklet_process =
      LaunchProcess(process_handle->worklet_type_, process_handle->origin_,
                    process_handle->site_instance_,
                    /*is_idle=*/false);
  (*processes)[process_handle->origin_] = worklet_process.get();
  process_handle->AssignProcess(std::move(worklet_process));
  OnNewProcessAssigned(process_handle);
  return RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess;
}

bool AuctionProcessManager::TryToUseIdleProcessForHandle(
    ProcessHandle* process_handle) {
  // This function shouldn't be called unless we have a spot for a new process.
  DCHECK(
      HasAvailableProcessSlotForActiveProcess(process_handle->worklet_type_));
  if (idle_processes_.empty()) {
    return false;
  }

  // Keep track of processes we may want to use for the handle. Prefer to use a
  // process that has already launched, and if there is none,  a process that
  // was created the earliest possible. idle_processes_  is sorted by process
  // creation time.
  auto process_matching_origin_and_type = idle_processes_.end();
  auto first_process_matching_type = idle_processes_.end();
  auto best_unbound_process_matching_type = idle_processes_.end();
  auto best_unbound_process = idle_processes_.end();
  size_t num_idle_processes_of_type = 0;
  for (auto it = idle_processes_.begin(); it != idle_processes_.end(); ++it) {
    if (it->get()->worklet_type() == process_handle->worklet_type()) {
      num_idle_processes_of_type++;
      if (it->get()->origin() == process_handle->origin()) {
        process_matching_origin_and_type = it;
      }
      if (first_process_matching_type == idle_processes_.end()) {
        first_process_matching_type = it;
      }
      if (!it->get()->is_bound_to_origin() &&
          (best_unbound_process_matching_type == idle_processes_.end() ||
           (!best_unbound_process_matching_type->get()->HasPid() &&
            it->get()->HasPid()))) {
        best_unbound_process_matching_type = it;
      }
    }
    if (!it->get()->is_bound_to_origin() &&
        (best_unbound_process == idle_processes_.end() ||
         (!best_unbound_process->get()->HasPid() && it->get()->HasPid()))) {
      best_unbound_process = it;
    }
  }

  auto idle_process_to_use = idle_processes_.end();
  if (process_matching_origin_and_type != idle_processes_.end()) {
    // If we have a perfectly matching process that is bound
    // to its origin, prefer to use it. If it's unbound, prefer to
    // use the first unbound process (because it was created earlier).
    if (process_matching_origin_and_type->get()->is_bound_to_origin() ||
        process_matching_origin_and_type == best_unbound_process) {
      idle_process_to_use = process_matching_origin_and_type;
    } else {
      // There's at least 1 unbound process because
      // `process_matching_origin_and_type` is unbound. We can use the first
      // unbound process no matter its type because we can't go over the process
      // limit if the `process_handle`'s origin and type are already in
      // `idle_processes_`.
      CHECK(best_unbound_process != idle_processes_.end());
      idle_process_to_use = best_unbound_process;
      // We want the `best_unbound_process` origin and type to remain in
      // `idle_processes_` so we can confirm we've already started
      // a process for that origin and type.
      process_matching_origin_and_type->get()->ReassignWorkletTypeAndOrigin(
          best_unbound_process->get()->worklet_type(),
          best_unbound_process->get()->origin());
    }
  } else {
    if (HasAvailableProcessSlotForIdleProcess(process_handle->worklet_type(),
                                              num_idle_processes_of_type)) {
      // We can use the first unbound process regardless of its `worklet_type`
      // since we have an available spot of `worklet_type.`
      if (best_unbound_process == idle_processes_.end()) {
        return false;
      }
      idle_process_to_use = best_unbound_process;
    } else {
      // There must be a `first_process_matching_type` because
      // HasAvailableProcessSlotForActiveProcess() is true but
      // HasAvailableProcessSlotForIdleProcess() is false.
      CHECK(first_process_matching_type != idle_processes_.end());
      if (best_unbound_process_matching_type != idle_processes_.end()) {
        idle_process_to_use = best_unbound_process_matching_type;
      } else {
        // There's no process we can use without going over the limit.
        // Remove a process so we can create a new one.
        idle_processes_.erase(first_process_matching_type);
        return false;
      }
    }
  }

  CHECK(idle_process_to_use != idle_processes_.end());

  idle_process_to_use->get()->ActivateAndBindIfUnbound(
      process_handle->worklet_type(), process_handle->origin());
  process_handle->AssignProcess(idle_process_to_use->get());
  ProcessMap* processes = Processes(process_handle->worklet_type_);
  (*processes)[process_handle->origin_] = idle_process_to_use->get();
  idle_processes_.erase(idle_process_to_use);
  return true;
}

AuctionProcessManager::AuctionProcessManager::AuctionProcessManager(
    TrustedSignalsCacheImpl* trusted_signals_cache)
    : trusted_signals_cache_(trusted_signals_cache) {}

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
  CHECK(it != pending_request_map->end(), base::NotFatalUntil::M130);
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
  CHECK(it != processes->end(), base::NotFatalUntil::M130);
  processes->erase(it);

  // May need to launch another process at this point.

  // Since a process was just destroyed, there should be at least one available
  // slot to create another.
  DCHECK(
      HasAvailableProcessSlotForActiveProcess(worklet_process->worklet_type()));

  // If there are no pending requests for the corresponding worklet type,
  // nothing more to do.
  PendingRequestQueue* queue =
      GetPendingRequestQueue(worklet_process->worklet_type());
  if (queue->empty())
    return;

  // All the pending requests for the same origin as the oldest pending request.
  std::set<raw_ptr<ProcessHandle, SetExperimental>>* pending_requests =
      &(*GetPendingRequestMap(
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
    bool process_created = TryCreateOrGetProcessForHandle(process_handle) !=
                           RequestWorkletServiceOutcome::kHitProcessLimit;
    CHECK(process_created);
    --num_matching_requests;

    // Nothing else to do after assigning the process - assigning a process
    // results in the callback being invoked asynchronously.
  }
}

void AuctionProcessManager::ReleaseIdleProcess(
    AuctionProcessManager::WorkletProcess* worklet_process) {
  std::erase_if(idle_processes_,
                [worklet_process](const scoped_refptr<WorkletProcess>& item) {
                  return item.get() == worklet_process;
                });
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

bool AuctionProcessManager::HasAvailableProcessSlotForActiveProcess(
    WorkletType worklet_type) const {
  if (worklet_type == WorkletType::kBidder)
    return bidder_processes_.size() < kMaxBidderProcesses;
  return seller_processes_.size() < kMaxSellerProcesses;
}

bool AuctionProcessManager::HasAvailableProcessSlotForIdleProcess(
    WorkletType worklet_type,
    size_t num_idle_processes_of_type) const {
  if (worklet_type == WorkletType::kBidder) {
    return num_idle_processes_of_type + bidder_processes_.size() <
           kMaxBidderProcesses;
  }
  return num_idle_processes_of_type + seller_processes_.size() <
         kMaxSellerProcesses;
}

DedicatedAuctionProcessManager::DedicatedAuctionProcessManager(
    TrustedSignalsCacheImpl* trusted_signals_cache)
    : AuctionProcessManager(trusted_signals_cache) {}

DedicatedAuctionProcessManager::~DedicatedAuctionProcessManager() = default;

AuctionProcessManager::WorkletProcess::ProcessContext
DedicatedAuctionProcessManager::CreateProcessInternal(
    WorkletProcess& worklet_process) {
  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  content::ServiceProcessHost::Launch(
      service.InitWithNewPipeAndPassReceiver(),
      ServiceProcessHost::Options()
          .WithDisplayName("Protected Audience JavaScript Process")
#if BUILDFLAG(IS_MAC)
          // TODO(crbug.com/40812055) add a utility helper for Jit.
          .WithChildFlags(ChildProcessHost::CHILD_RENDERER)
#endif
          .WithProcessCallback(
              base::BindOnce(&WorkletProcess::OnLaunchedWithProcess,
                             worklet_process.weak_ptr_factory_.GetWeakPtr()))
          .Pass());
  return WorkletProcess::ProcessContext(std::move(service));
}

scoped_refptr<AuctionProcessManager::WorkletProcess>
DedicatedAuctionProcessManager::LaunchProcess(
    WorkletType worklet_type,
    const url::Origin& origin,
    scoped_refptr<SiteInstance> site_instance,
    bool is_idle) {
  // Start all idle processes unbound and all non-idle processes bound.
  scoped_refptr<WorkletProcess> worklet_process =
      base::MakeRefCounted<WorkletProcess>(
          this, /*site_instance=*/nullptr, worklet_type, origin,
          /*uses_shared_process=*/false, /*is_idle=*/is_idle,
          /*is_bound_to_origin=*/!is_idle);
  worklet_process->SetService(CreateProcessInternal(*worklet_process));
  return worklet_process;
}

scoped_refptr<SiteInstance>
DedicatedAuctionProcessManager::MaybeComputeSiteInstance(
    SiteInstance* frame_site_instance,
    const url::Origin& worklet_origin) {
  return nullptr;
}

bool DedicatedAuctionProcessManager::TryUseSharedProcess(
    ProcessHandle* process_handle) {
  return false;
}

InRendererAuctionProcessManager::InRendererAuctionProcessManager(
    TrustedSignalsCacheImpl* trusted_signals_cache)
    : AuctionProcessManager(trusted_signals_cache) {}

InRendererAuctionProcessManager::~InRendererAuctionProcessManager() = default;

AuctionProcessManager::WorkletProcess::ProcessContext
InRendererAuctionProcessManager::CreateProcessInternal(
    WorkletProcess& worklet_process) {
  SiteInstance* site_instance = worklet_process.site_instance();
  if (site_instance->GetBrowserContext()->ShutdownStarted()) {
    // This browser context is shutting down, so we shouldn't start any
    // processes, in part because managing their lifetime will be impossible.
    // So... just give up. Create a pipe and drop the other end. The service
    // pipe will be broken, but that should be OK since the destination of the
    // async callback on process assignment should get deleted before we get
    // back to the event loop.
    return WorkletProcess::ProcessContext(
        mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>()
            .InitWithNewPipeAndPassRemote());
  }

  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  site_instance->GetProcess()->Init();
  site_instance->GetProcess()->BindReceiver(
      service.InitWithNewPipeAndPassReceiver());
  return WorkletProcess::ProcessContext(std::move(service),
                                        site_instance->GetProcess());
}

scoped_refptr<AuctionProcessManager::WorkletProcess>
InRendererAuctionProcessManager::LaunchProcess(
    WorkletType worklet_type,
    const url::Origin& origin,
    scoped_refptr<SiteInstance> site_instance,
    bool is_idle) {
  DCHECK(site_instance);
  DCHECK(site_instance->RequiresDedicatedProcess());
  auto worklet_process = base::MakeRefCounted<WorkletProcess>(
      this, std::move(site_instance), worklet_type, origin,
      /*uses_shared_process=*/false, /*is_idle=*/is_idle,
      /*is_bound_to_origin=*/true);
  worklet_process->SetService(CreateProcessInternal(*worklet_process));
  // This process must be bound to an origin because it's launched in
  // the site instance.
  return worklet_process;
}

scoped_refptr<SiteInstance>
InRendererAuctionProcessManager::MaybeComputeSiteInstance(
    SiteInstance* frame_site_instance,
    const url::Origin& worklet_origin) {
  return frame_site_instance->GetRelatedSiteInstance(worklet_origin.GetURL());
}

bool InRendererAuctionProcessManager::TryUseSharedProcess(
    ProcessHandle* process_handle) {
  // If this needs a dedicated process due to site isolation, return and let
  // AuctionProcessManager do the quota thing. Then it will ask for one in
  // LaunchProcess once process count is low enough. This is only reasonable to
  // do since dedicated processes are shared among different BrowsingInstances,
  // so the stored `process_handle->site_instance_` requiring a dedicated
  // process is as good as any.
  if (process_handle->site_instance_->RequiresDedicatedProcess())
    return false;

  // Shared process case.
  auto worklet_process = base::MakeRefCounted<WorkletProcess>(
      this, process_handle->site_instance_, process_handle->worklet_type(),
      process_handle->origin(), /*uses_shared_process=*/true, /*is_idle=*/false,
      /*is_bound_to_origin=*/true);
  worklet_process->SetService(CreateProcessInternal(*worklet_process));
  process_handle->AssignProcess(std::move(worklet_process));
  return true;
}

}  // namespace content

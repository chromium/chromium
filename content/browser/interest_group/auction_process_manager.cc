// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
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

AuctionProcessManager::WorkletProcess::WorkletProcess(
    AuctionProcessManager* auction_process_manager,
    RenderProcessHost* render_process_host,
    mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service,
    WorkletType worklet_type,
    const url::Origin& origin,
    bool uses_shared_process)
    : render_process_host_(render_process_host),
      worklet_type_(worklet_type),
      origin_(origin),
      start_time_(base::TimeTicks::Now()),
      uses_shared_process_(uses_shared_process),
      auction_process_manager_(auction_process_manager),
      service_(std::move(service)) {
  DCHECK(auction_process_manager);
  service_.set_disconnect_handler(base::BindOnce(
      &WorkletProcess::NotifyUnusableOnce, base::Unretained(this)));

  if (render_process_host_) {
    render_process_host_->IncrementWorkerRefCount();
    render_process_host_->AddObserver(this);

    // Note the PID if the process has already launched
    if (render_process_host_->IsReady()) {
      DCHECK(render_process_host_->GetProcess().IsValid());
      pid_ = render_process_host_->GetProcess().Pid();
    }
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

void AuctionProcessManager::WorkletProcess::RenderProcessReady(
    RenderProcessHost* host) {
  DCHECK(render_process_host_);
  DCHECK(render_process_host_->GetProcess().IsValid());
  OnLaunchedWithProcess(render_process_host_->GetProcess());
}

void AuctionProcessManager::WorkletProcess::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_EQ(host, render_process_host_);
  NotifyUnusableOnce();
}

void AuctionProcessManager::WorkletProcess::NotifyUnusableOnce() {
  AuctionProcessManager* maybe_apm = auction_process_manager_;
  // Clear `auction_process_manager_` to make sure OnWorkletProcessUnusable()
  // is called once.  Clear it before call to ensure this is the case even
  // if this method is re-entered somehow.
  auction_process_manager_ = nullptr;
  if (maybe_apm && !uses_shared_process_) {
    maybe_apm->OnWorkletProcessUnusable(this);
  }

  if (render_process_host_) {
    render_process_host_->RemoveObserver(this);
    if (!render_process_host_->AreRefCountsDisabled()) {
      render_process_host_->DecrementWorkerRefCount();
    }
    render_process_host_ = nullptr;
  }
}

AuctionProcessManager::WorkletProcess::~WorkletProcess() {
  NotifyUnusableOnce();
}

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
  if (!HasAvailableProcessSlot(process_handle->worklet_type_))
    return RequestWorkletServiceOutcome::kHitProcessLimit;

  // Launch the process and create WorkletProcess object bound to it.
  scoped_refptr<WorkletProcess> worklet_process = LaunchProcess(
      process_handle, ComputeDisplayName(process_handle->worklet_type_,
                                         process_handle->origin_));
  (*processes)[process_handle->origin_] = worklet_process.get();
  process_handle->AssignProcess(std::move(worklet_process));
  OnNewProcessAssigned(process_handle);
  return RequestWorkletServiceOutcome::kCreatedNewDedicatedProcess;
}

AuctionProcessManager::AuctionProcessManager() = default;

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
  DCHECK(HasAvailableProcessSlot(worklet_process->worklet_type()));

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

DedicatedAuctionProcessManager::DedicatedAuctionProcessManager() = default;
DedicatedAuctionProcessManager::~DedicatedAuctionProcessManager() = default;

scoped_refptr<AuctionProcessManager::WorkletProcess>
DedicatedAuctionProcessManager::LaunchProcess(
    const ProcessHandle* process_handle,
    const std::string& display_name) {
  mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService> receiver;
  scoped_refptr<WorkletProcess> worklet_process =
      base::MakeRefCounted<WorkletProcess>(
          this, /*render_process_host=*/nullptr,
          receiver.InitWithNewPipeAndPassRemote(),
          process_handle->worklet_type(), process_handle->origin(),
          /*uses_shared_process=*/false);
  content::ServiceProcessHost::Launch(
      std::move(receiver),
      ServiceProcessHost::Options()
          .WithDisplayName(display_name)
#if BUILDFLAG(IS_MAC)
          // TODO(crbug.com/40812055) add a utility helper for Jit.
          .WithChildFlags(ChildProcessHost::CHILD_RENDERER)
#endif
          .WithProcessCallback(
              base::BindOnce(&WorkletProcess::OnLaunchedWithProcess,
                             worklet_process->weak_ptr_factory_.GetWeakPtr()))
          .Pass());
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

InRendererAuctionProcessManager::InRendererAuctionProcessManager() = default;
InRendererAuctionProcessManager::~InRendererAuctionProcessManager() = default;

scoped_refptr<AuctionProcessManager::WorkletProcess>
InRendererAuctionProcessManager::LaunchProcess(
    const ProcessHandle* process_handle,
    const std::string& display_name) {
  DCHECK(process_handle->site_instance_);
  DCHECK(process_handle->site_instance_->RequiresDedicatedProcess());
  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  RenderProcessHost* render_process_host =
      LaunchInSiteInstance(process_handle->site_instance_.get(),
                           service.InitWithNewPipeAndPassReceiver());
  return base::MakeRefCounted<WorkletProcess>(
      this, render_process_host, std::move(service),
      process_handle->worklet_type(), process_handle->origin(),
      /*uses_shared_process=*/false);
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
  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  RenderProcessHost* render_process_host =
      LaunchInSiteInstance(process_handle->site_instance_.get(),
                           service.InitWithNewPipeAndPassReceiver());
  auto process = base::MakeRefCounted<WorkletProcess>(
      this, render_process_host, std::move(service),
      process_handle->worklet_type(), process_handle->origin(),
      /*uses_shared_process=*/true);
  process_handle->AssignProcess(std::move(process));
  return true;
}

RenderProcessHost* InRendererAuctionProcessManager::LaunchInSiteInstance(
    SiteInstance* site_instance,
    mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
        auction_worklet_service_receiver) {
  if (site_instance->GetBrowserContext()->ShutdownStarted()) {
    // This browser context is shutting down, so we shouldn't start any
    // processes, in part because managing their lifetime will be impossible.
    // So... just give up. The service pipe will be broken, but that should be
    // OK since the destination of the async callback on process assignment
    // should get deleted before we get back to the event loop.
    return nullptr;
  }
  site_instance->GetProcess()->Init();
  site_instance->GetProcess()->BindReceiver(
      std::move(auction_worklet_service_receiver));
  return site_instance->GetProcess();
}

}  // namespace content

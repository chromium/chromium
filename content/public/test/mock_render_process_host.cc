// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "media/media_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

namespace {

MockRenderProcessHost::CreateNetworkFactoryCallback&
GetNetworkFactoryCallback() {
  static base::NoDestructor<MockRenderProcessHost::CreateNetworkFactoryCallback>
      callback;
  return *callback;
}

}  // namespace

MockRenderProcessHost::MockRenderProcessHost(BrowserContext* browser_context,
                                             bool is_for_guests_only)
    : bad_msg_count_(0),
      factory_(nullptr),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      has_connection_(false),
      browser_context_(browser_context),
      prev_routing_id_(0),
      shutdown_requested_(false),
      fast_shutdown_started_(false),
      deletion_callback_called_(false),
      is_for_guests_only_(is_for_guests_only),
      is_process_backgrounded_(false),
      is_unused_(true),
      keep_alive_ref_count_(0),
      foreground_service_worker_count_(0),
      url_loader_factory_(std::make_unique<FakeNetworkURLLoaderFactory>()) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID(), browser_context);

  RenderProcessHostImpl::RegisterHost(GetID(), this);
}

MockRenderProcessHost::~MockRenderProcessHost() {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());
  if (factory_)
    factory_->Remove(this);

  // In unit tests, Cleanup() might not have been called.
  if (!deletion_callback_called_) {
    for (auto& observer : observers_)
      observer.RenderProcessHostDestroyed(this);
    RenderProcessHostImpl::UnregisterHost(GetID());
  }
}

void MockRenderProcessHost::SimulateCrash() {
  SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
}

void MockRenderProcessHost::SimulateRenderProcessExit(
    base::TerminationStatus status,
    int exit_code) {
  has_connection_ = false;
  ChildProcessTerminationInfo termination_info;
  termination_info.status = status;
  termination_info.exit_code = exit_code;
  termination_info.renderer_has_visible_clients = VisibleClientCount() > 0;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&termination_info));

  for (auto& observer : observers_)
    observer.RenderProcessExited(this, termination_info);
}

// static
void MockRenderProcessHost::SetNetworkFactory(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  GetNetworkFactoryCallback() = create_network_factory_callback;
}

bool MockRenderProcessHost::Init() {
  has_connection_ = true;
  return true;
}

void MockRenderProcessHost::EnableSendQueue() {}

int MockRenderProcessHost::GetNextRoutingID() {
  return ++prev_routing_id_;
}

void MockRenderProcessHost::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  listeners_.AddWithID(listener, routing_id);
}

void MockRenderProcessHost::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != nullptr);
  listeners_.Remove(routing_id);
  Cleanup();
}

void MockRenderProcessHost::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void MockRenderProcessHost::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockRenderProcessHost::ShutdownForBadMessage(
    CrashReportMode crash_report_mode) {
  ++bad_msg_count_;
}

void MockRenderProcessHost::UpdateClientPriority(PriorityClient* client) {}

void MockRenderProcessHost::UpdateFrameWithPriority(
    base::Optional<FramePriority> previous_priority,
    base::Optional<FramePriority> new_priority) {}

int MockRenderProcessHost::VisibleClientCount() {
  int count = 0;
  for (auto* client : priority_clients_) {
    const Priority priority = client->GetPriority();
    if (!priority.is_hidden) {
      count++;
    }
  }
  return count;
}

unsigned int MockRenderProcessHost::GetFrameDepth() {
  NOTIMPLEMENTED();
  return 0u;
}

bool MockRenderProcessHost::GetIntersectsViewport() {
  NOTIMPLEMENTED();
  return true;
}

bool MockRenderProcessHost::IsForGuestsOnly() {
  return is_for_guests_only_;
}

void MockRenderProcessHost::OnMediaStreamAdded() {}

void MockRenderProcessHost::OnMediaStreamRemoved() {}

void MockRenderProcessHost::OnForegroundServiceWorkerAdded() {
  foreground_service_worker_count_ += 1;
}

void MockRenderProcessHost::OnForegroundServiceWorkerRemoved() {
  DCHECK_GT(foreground_service_worker_count_, 0);
  foreground_service_worker_count_ -= 1;
}

StoragePartition* MockRenderProcessHost::GetStoragePartition() {
  return BrowserContext::GetDefaultStoragePartition(browser_context_);
}

void MockRenderProcessHost::AddWord(const base::string16& word) {
}

bool MockRenderProcessHost::Shutdown(int exit_code) {
  shutdown_requested_ = true;
  return true;
}

bool MockRenderProcessHost::ShutdownRequested() {
  return shutdown_requested_;
}

bool MockRenderProcessHost::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  if (GetActiveViewCount() != page_count)
    return false;
  // We aren't actually going to do anything, but set |fast_shutdown_started_|
  // to true so that tests know we've been called.
  fast_shutdown_started_ = true;
  return true;
}

bool MockRenderProcessHost::FastShutdownStarted() {
  return fast_shutdown_started_;
}

const base::Process& MockRenderProcessHost::GetProcess() {
  // Return the current-process handle for the IPC::GetPlatformFileForTransit
  // function.
  if (process.IsValid())
    return process;

  static const base::Process current_process(base::Process::Current());
  return current_process;
}

bool MockRenderProcessHost::IsReady() {
  return false;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

int MockRenderProcessHost::GetID() {
  return id_;
}

bool MockRenderProcessHost::IsInitializedAndNotDead() {
  return has_connection_;
}

void MockRenderProcessHost::SetBlocked(bool blocked) {}

bool MockRenderProcessHost::IsBlocked() {
  return false;
}

std::unique_ptr<RenderProcessHost::BlockStateChangedCallbackList::Subscription>
MockRenderProcessHost::RegisterBlockStateChangedCallback(
    const BlockStateChangedCallback& cb) {
  return nullptr;
}

static void DeleteIt(base::WeakPtr<MockRenderProcessHost> h) {
  if (h)
    delete h.get();
}

void MockRenderProcessHost::Cleanup() {
  if (listeners_.IsEmpty()) {
    if (IsInitializedAndNotDead()) {
      ChildProcessTerminationInfo termination_info;
      termination_info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
      termination_info.exit_code = 0;
      termination_info.renderer_has_visible_clients = VisibleClientCount() > 0;
      for (auto& observer : observers_)
        observer.RenderProcessExited(this, termination_info);
    }

    for (auto& observer : observers_)
      observer.RenderProcessHostDestroyed(this);
    // Post the delete of |this| as a WeakPtr so that if |this| is deleted by a
    // test directly, we don't double free.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DeleteIt, weak_ptr_factory_.GetWeakPtr()));
    RenderProcessHostImpl::UnregisterHost(GetID());
    deletion_callback_called_ = true;
  }
}

void MockRenderProcessHost::AddPendingView() {
}

void MockRenderProcessHost::RemovePendingView() {
}

void MockRenderProcessHost::AddPriorityClient(PriorityClient* priority_client) {
  priority_clients_.insert(priority_client);
}

void MockRenderProcessHost::RemovePriorityClient(
    PriorityClient* priority_client) {
  priority_clients_.erase(priority_client);
}

void MockRenderProcessHost::SetPriorityOverride(bool foreground) {}

bool MockRenderProcessHost::HasPriorityOverride() {
  return false;
}

void MockRenderProcessHost::ClearPriorityOverride() {}

#if defined(OS_ANDROID)
ChildProcessImportance MockRenderProcessHost::GetEffectiveImportance() {
  NOTIMPLEMENTED();
  return ChildProcessImportance::NORMAL;
}

void MockRenderProcessHost::DumpProcessStack() {}
#endif

void MockRenderProcessHost::SetSuddenTerminationAllowed(bool allowed) {
}

bool MockRenderProcessHost::SuddenTerminationAllowed() {
  return true;
}

BrowserContext* MockRenderProcessHost::GetBrowserContext() {
  return browser_context_;
}

bool MockRenderProcessHost::InSameStoragePartition(
    StoragePartition* partition) {
  // Mock RPHs only have one partition.
  return true;
}

IPC::ChannelProxy* MockRenderProcessHost::GetChannel() {
  return nullptr;
}

void MockRenderProcessHost::AddFilter(BrowserMessageFilter* filter) {
}

base::TimeDelta MockRenderProcessHost::GetChildProcessIdleTime() {
  return base::TimeDelta::FromMilliseconds(0);
}

void MockRenderProcessHost::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  auto it = binder_overrides_.find(*receiver.interface_name());
  if (it != binder_overrides_.end())
    it->second.Run(receiver.PassPipe());
}

std::unique_ptr<base::PersistentMemoryAllocator>
MockRenderProcessHost::TakeMetricsAllocator() {
  return nullptr;
}

const base::TimeTicks&
MockRenderProcessHost::GetInitTimeForNavigationMetrics() {
  static base::TimeTicks dummy_time = base::TimeTicks::Now();
  return dummy_time;
}

bool MockRenderProcessHost::IsProcessBackgrounded() {
  return is_process_backgrounded_;
}

size_t MockRenderProcessHost::GetKeepAliveRefCount() const {
  return keep_alive_ref_count_;
}

void MockRenderProcessHost::IncrementKeepAliveRefCount() {
  ++keep_alive_ref_count_;
}

void MockRenderProcessHost::DecrementKeepAliveRefCount() {
  --keep_alive_ref_count_;
}

void MockRenderProcessHost::DisableKeepAliveRefCount() {
  keep_alive_ref_count_ = 0;

  // RenderProcessHost::DisableKeepAliveRefCount() virtual method gets called as
  // part of BrowserContext::NotifyWillBeDestroyed(...).  Normally
  // MockRenderProcessHost::DisableKeepAliveRefCount doesn't call Cleanup,
  // because the MockRenderProcessHost might be owned by a test.  However, when
  // the MockRenderProcessHost is the spare RenderProcessHost, we know that it
  // is owned by the SpareRenderProcessHostManager and we need to delete the
  // spare to avoid reports/DCHECKs about memory leaks.
  if (this == RenderProcessHostImpl::GetSpareRenderProcessHostForTesting())
    Cleanup();
}

bool MockRenderProcessHost::IsKeepAliveRefCountDisabled() {
  return false;
}

mojom::Renderer* MockRenderProcessHost::GetRendererInterface() {
  if (!renderer_interface_) {
    renderer_interface_ =
        std::make_unique<mojo::AssociatedRemote<mojom::Renderer>>();
    ignore_result(
        renderer_interface_->BindNewEndpointAndPassDedicatedReceiver());
  }
  return renderer_interface_->get();
}

void MockRenderProcessHost::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  if (GetNetworkFactoryCallback().is_null()) {
    url_loader_factory_->Clone(std::move(receiver));
    return;
  }

  mojo::Remote<network::mojom::URLLoaderFactory> original_factory;
  url_loader_factory_->Clone(original_factory.BindNewPipeAndPassReceiver());
  GetNetworkFactoryCallback().Run(std::move(receiver), GetID(),
                                  original_factory.Unbind());
}

bool MockRenderProcessHost::MayReuseHost() {
  return true;
}

bool MockRenderProcessHost::IsUnused() {
  return is_unused_;
}

void MockRenderProcessHost::SetIsUsed() {
  is_unused_ = false;
}

bool MockRenderProcessHost::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && GetKeepAliveRefCount() == 0;
}

void MockRenderProcessHost::SetProcessLock(
    const IsolationContext& isolation_context,
    const ProcessLock& process_lock) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
      isolation_context, GetID(), process_lock);
  if (process_lock.IsASiteOrOrigin())
    is_renderer_locked_to_site_ = true;
}

bool MockRenderProcessHost::IsProcessLockedToSiteForTesting() {
  ProcessLock lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(GetID());
  return lock.is_locked_to_site();
}

void MockRenderProcessHost::BindCacheStorage(
    const network::CrossOriginEmbedderPolicy&,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  cache_storage_receiver_ = std::move(receiver);
}

void MockRenderProcessHost::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  idb_factory_receiver_ = std::move(receiver);
}

void MockRenderProcessHost::
    CleanupNetworkServicePluginExceptionsUponDestruction() {}

std::string
MockRenderProcessHost::GetInfoForBrowserContextDestructionCrashReporting() {
  return std::string();
}

void MockRenderProcessHost::FilterURL(bool empty_allowed, GURL* url) {
  RenderProcessHostImpl::FilterURL(this, empty_allowed, url);
}

void MockRenderProcessHost::EnableAudioDebugRecordings(
    const base::FilePath& file) {
}

void MockRenderProcessHost::DisableAudioDebugRecordings() {}

RenderProcessHost::WebRtcStopRtpDumpCallback
MockRenderProcessHost::StartRtpDump(bool incoming,
                                    bool outgoing,
                                    WebRtcRtpPacketCallback packet_callback) {
  return base::NullCallback();
}

void MockRenderProcessHost::EnableWebRtcEventLogOutput(int lid,
                                                       int output_period_ms) {}
void MockRenderProcessHost::DisableWebRtcEventLogOutput(int lid) {}

bool MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (listener)
    return listener->OnMessageReceived(msg);
  return false;
}

void MockRenderProcessHost::OnChannelConnected(int32_t peer_pid) {}

void MockRenderProcessHost::OverrideBinderForTesting(
    const std::string& interface_name,
    const InterfaceBinder& binder) {
  binder_overrides_[interface_name] = binder;
}

void MockRenderProcessHost::OverrideRendererInterfaceForTesting(
    std::unique_ptr<mojo::AssociatedRemote<mojom::Renderer>>
        renderer_interface) {
  renderer_interface_ = std::move(renderer_interface);
}

MockRenderProcessHostFactory::MockRenderProcessHostFactory() = default;

MockRenderProcessHostFactory::~MockRenderProcessHostFactory() {
  // Detach this object from MockRenderProcesses to prevent them from calling
  // MockRenderProcessHostFactory::Remove() when destroyed.
  for (const auto& process : processes_)
    process->SetFactory(nullptr);
}

RenderProcessHost* MockRenderProcessHostFactory::CreateRenderProcessHost(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  const bool is_for_guests_only = site_instance && site_instance->IsGuest();
  std::unique_ptr<MockRenderProcessHost> host =
      std::make_unique<MockRenderProcessHost>(browser_context,
                                              is_for_guests_only);
  processes_.push_back(std::move(host));
  processes_.back()->SetFactory(this);
  return processes_.back().get();
}

void MockRenderProcessHostFactory::Remove(MockRenderProcessHost* host) const {
  for (auto it = processes_.begin(); it != processes_.end(); ++it) {
    if (it->get() == host) {
      it->release();
      processes_.erase(it);
      break;
    }
  }
}

}  // namespace content

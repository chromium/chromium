// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
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
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace content {

MockRenderProcessHost::MockRenderProcessHost(BrowserContext* browser_context)
    : bad_msg_count_(0),
      factory_(nullptr),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      has_connection_(false),
      browser_context_(browser_context),
      prev_routing_id_(0),
      fast_shutdown_started_(false),
      deletion_callback_called_(false),
      is_for_guests_only_(false),
      is_never_suitable_for_reuse_(false),
      is_process_backgrounded_(false),
      is_unused_(true),
      keep_alive_ref_count_(0),
      child_identity_(mojom::kRendererServiceName,
                      BrowserContext::GetServiceUserIdFor(browser_context),
                      base::StringPrintf("%d", id_)),
      url_loader_factory_(nullptr),
      weak_ptr_factory_(this) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

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
  ChildProcessTerminationInfo termination_info{status, exit_code};
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&termination_info));

  for (auto& observer : observers_)
    observer.RenderProcessExited(this, termination_info);

  // Send every routing ID a FrameHostMsg_RenderProcessGone message. To ensure a
  // predictable order for unittests which may assert against the order, we sort
  // the listeners by descending routing ID, instead of using the arbitrary
  // hash-map order like RenderProcessHostImpl.
  std::vector<std::pair<int32_t, IPC::Listener*>> sorted_listeners_;
  base::IDMap<IPC::Listener*>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    sorted_listeners_.push_back(
        std::make_pair(iter.GetCurrentKey(), iter.GetCurrentValue()));
    iter.Advance();
  }
  std::sort(sorted_listeners_.rbegin(), sorted_listeners_.rend());

  for (auto& entry_pair : sorted_listeners_) {
    entry_pair.second->OnMessageReceived(FrameHostMsg_RenderProcessGone(
        entry_pair.first, static_cast<int>(termination_info.status),
        termination_info.exit_code));
  }
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

int MockRenderProcessHost::VisibleClientCount() const {
  int count = 0;
  for (auto* client : priority_clients_) {
    const Priority priority = client->GetPriority();
    if (!priority.is_hidden) {
      count++;
    }
  }
  return count;
}

unsigned int MockRenderProcessHost::GetFrameDepth() const {
  NOTIMPLEMENTED();
  return 0u;
}

bool MockRenderProcessHost::GetIntersectsViewport() const {
  NOTIMPLEMENTED();
  return true;
}

bool MockRenderProcessHost::IsForGuestsOnly() const {
  return is_for_guests_only_;
}

RendererAudioOutputStreamFactoryContext*
MockRenderProcessHost::GetRendererAudioOutputStreamFactoryContext() {
  return nullptr;
}

void MockRenderProcessHost::OnMediaStreamAdded() {}

void MockRenderProcessHost::OnMediaStreamRemoved() {}

StoragePartition* MockRenderProcessHost::GetStoragePartition() const {
  return BrowserContext::GetDefaultStoragePartition(browser_context_);
}

void MockRenderProcessHost::AddWord(const base::string16& word) {
}

bool MockRenderProcessHost::Shutdown(int exit_code) {
  return true;
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

bool MockRenderProcessHost::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

const base::Process& MockRenderProcessHost::GetProcess() const {
  // Return the current-process handle for the IPC::GetPlatformFileForTransit
  // function.
  if (process.IsValid())
    return process;

  static const base::Process current_process(base::Process::Current());
  return current_process;
}

bool MockRenderProcessHost::IsReady() const {
  return false;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

int MockRenderProcessHost::GetID() const {
  return id_;
}

bool MockRenderProcessHost::IsInitializedAndNotDead() const {
  return has_connection_;
}

void MockRenderProcessHost::SetIgnoreInputEvents(bool ignore_input_events) {
}

bool MockRenderProcessHost::IgnoreInputEvents() const {
  return false;
}

static void DeleteIt(base::WeakPtr<MockRenderProcessHost> h) {
  if (h)
    delete h.get();
}

void MockRenderProcessHost::Cleanup() {
  if (listeners_.IsEmpty()) {
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

void MockRenderProcessHost::AddWidget(RenderWidgetHost* widget) {
  priority_clients_.insert(static_cast<RenderWidgetHostImpl*>(widget));
}

void MockRenderProcessHost::RemoveWidget(RenderWidgetHost* widget) {
  priority_clients_.erase(static_cast<RenderWidgetHostImpl*>(widget));
}

#if defined(OS_ANDROID)
ChildProcessImportance MockRenderProcessHost::GetEffectiveImportance() {
  NOTIMPLEMENTED();
  return ChildProcessImportance::NORMAL;
}
#endif

void MockRenderProcessHost::SetSuddenTerminationAllowed(bool allowed) {
}

bool MockRenderProcessHost::SuddenTerminationAllowed() const {
  return true;
}

BrowserContext* MockRenderProcessHost::GetBrowserContext() const {
  return browser_context_;
}

bool MockRenderProcessHost::InSameStoragePartition(
    StoragePartition* partition) const {
  // Mock RPHs only have one partition.
  return true;
}

IPC::ChannelProxy* MockRenderProcessHost::GetChannel() {
  return nullptr;
}

void MockRenderProcessHost::AddFilter(BrowserMessageFilter* filter) {
}

base::TimeDelta MockRenderProcessHost::GetChildProcessIdleTime() const {
  return base::TimeDelta::FromMilliseconds(0);
}

void MockRenderProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (binder_overrides_.count(interface_name) > 0)
    binder_overrides_[interface_name].Run(std::move(interface_pipe));
}

const service_manager::Identity& MockRenderProcessHost::GetChildIdentity()
    const {
  return child_identity_;
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
MockRenderProcessHost::TakeMetricsAllocator() {
  return nullptr;
}

const base::TimeTicks& MockRenderProcessHost::GetInitTimeForNavigationMetrics()
    const {
  static base::TimeTicks dummy_time = base::TimeTicks::Now();
  return dummy_time;
}

bool MockRenderProcessHost::IsProcessBackgrounded() const {
  return is_process_backgrounded_;
}

size_t MockRenderProcessHost::GetKeepAliveRefCount() const {
  return keep_alive_ref_count_;
}

void MockRenderProcessHost::IncrementKeepAliveRefCount(
    KeepAliveClientType client) {
  ++keep_alive_ref_count_;
}

void MockRenderProcessHost::DecrementKeepAliveRefCount(
    KeepAliveClientType client) {
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

void MockRenderProcessHost::PurgeAndSuspend() {}

void MockRenderProcessHost::Resume() {}

mojom::Renderer* MockRenderProcessHost::GetRendererInterface() {
  if (!renderer_interface_) {
    renderer_interface_.reset(new mojom::RendererAssociatedPtr);
    mojo::MakeRequestAssociatedWithDedicatedPipe(renderer_interface_.get());
  }
  return renderer_interface_->get();
}

resource_coordinator::ProcessResourceCoordinator*
MockRenderProcessHost::GetProcessResourceCoordinator() {
  if (!process_resource_coordinator_) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    process_resource_coordinator_ =
        std::make_unique<resource_coordinator::ProcessResourceCoordinator>(
            connector);
  }
  return process_resource_coordinator_.get();
}

void MockRenderProcessHost::CreateURLLoaderFactory(
    const url::Origin& origin,
    network::mojom::URLLoaderFactoryRequest request) {
  url_loader_factory_->Clone(std::move(request));
}

void MockRenderProcessHost::SetIsNeverSuitableForReuse() {
  is_never_suitable_for_reuse_ = true;
}

bool MockRenderProcessHost::MayReuseHost() {
  return !is_never_suitable_for_reuse_;
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

void MockRenderProcessHost::LockToOrigin(const GURL& lock_url) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockToOrigin(GetID(),
                                                              lock_url);
  if (SiteInstanceImpl::IsOriginLockASite(lock_url))
    is_renderer_locked_to_site_ = true;
}

void MockRenderProcessHost::BindCacheStorage(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  cache_storage_request_ = std::move(request);
}

void MockRenderProcessHost::CleanupCorbExceptionForPluginUponDestruction() {}

void MockRenderProcessHost::FilterURL(bool empty_allowed, GURL* url) {
  RenderProcessHostImpl::FilterURL(this, empty_allowed, url);
}

void MockRenderProcessHost::EnableAudioDebugRecordings(
    const base::FilePath& file) {
}

void MockRenderProcessHost::DisableAudioDebugRecordings() {}

void MockRenderProcessHost::SetEchoCanceller3(
    bool enable,
    base::OnceCallback<void(bool, const std::string&)> callback) {}

RenderProcessHost::WebRtcStopRtpDumpCallback
MockRenderProcessHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const WebRtcRtpPacketCallback& packet_callback) {
  return WebRtcStopRtpDumpCallback();
}

void MockRenderProcessHost::SetWebRtcEventLogOutput(int lid, bool enabled) {}

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
    std::unique_ptr<mojo::AssociatedInterfacePtr<mojom::Renderer>>
        renderer_interface) {
  renderer_interface_ = std::move(renderer_interface);
}

void MockRenderProcessHost::OverrideURLLoaderFactory(
    network::mojom::URLLoaderFactory* factory) {
  url_loader_factory_ = factory;
}

MockRenderProcessHostFactory::MockRenderProcessHostFactory()
    : default_mock_url_loader_factory_(
          std::make_unique<NotImplementedNetworkURLLoaderFactory>()) {}

MockRenderProcessHostFactory::~MockRenderProcessHostFactory() {
  // Detach this object from MockRenderProcesses to prevent them from calling
  // MockRenderProcessHostFactory::Remove() when destroyed.
  for (const auto& process : processes_)
    process->SetFactory(nullptr);
}

RenderProcessHost* MockRenderProcessHostFactory::CreateRenderProcessHost(
    BrowserContext* browser_context,
    SiteInstance* site_instance) const {
  std::unique_ptr<MockRenderProcessHost> host =
      std::make_unique<MockRenderProcessHost>(browser_context);
  host->OverrideURLLoaderFactory(default_mock_url_loader_factory_.get());
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

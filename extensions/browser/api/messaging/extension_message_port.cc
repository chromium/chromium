// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/extension_message_port.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace {

std::string PortIdToString(const extensions::PortId& port_id) {
  return base::StrCat({port_id.GetChannelId().first.ToString(), ":",
                       base::NumberToString(port_id.GetChannelId().second)});
}

}  // namespace

namespace extensions {

const char kReceivingEndDoesntExistError[] =
    // TODO(lazyboy): Test these in service worker implementation.
    "Could not establish connection. Receiving end does not exist.";

// Helper class to detect when frames are destroyed.
class ExtensionMessagePort::FrameTracker : public content::WebContentsObserver,
                                           public ProcessManagerObserver {
 public:
  explicit FrameTracker(ExtensionMessagePort* port)
      : pm_observer_(this), port_(port), interstitial_frame_(nullptr) {}
  ~FrameTracker() override {}

  void TrackExtensionProcessFrames() {
    pm_observer_.Add(ProcessManager::Get(port_->browser_context_));
  }

  void TrackTabFrames(content::WebContents* tab) {
    Observe(tab);
  }

  void TrackInterstitialFrame(content::WebContents* tab,
                              content::RenderFrameHost* interstitial_frame) {
    // |tab| should never be nullptr, because an interstitial's lifetime is
    // tied to a tab. This is a CHECK, not a DCHECK because we really need an
    // observer subject to detect frame removal (via DidDetachInterstitialPage).
    CHECK(tab);
    DCHECK(interstitial_frame);
    interstitial_frame_ = interstitial_frame;
    Observe(tab);
  }

 private:
  // content::WebContentsObserver overrides:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host)
      override {
    port_->UnregisterFrame(render_frame_host);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsSameDocument()) {
      port_->UnregisterFrame(navigation_handle->GetRenderFrameHost());
    }
  }

  void DidDetachInterstitialPage() override {
    if (interstitial_frame_)
      port_->UnregisterFrame(interstitial_frame_);
  }

  // extensions::ProcessManagerObserver overrides:
  void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override {
    if (extension_id == port_->extension_id_)
      port_->UnregisterFrame(render_frame_host);
  }

  ScopedObserver<ProcessManager, ProcessManagerObserver> pm_observer_;
  ExtensionMessagePort* port_;  // Owns this FrameTracker.

  // Set to the main frame of an interstitial if we are tracking an interstitial
  // page, because RenderFrameDeleted is never triggered for frames in an
  // interstitial (and we only support tracking the interstitial's main frame).
  content::RenderFrameHost* interstitial_frame_;

  DISALLOW_COPY_AND_ASSIGN(FrameTracker);
};

// Represents target of an IPC (render frame, ServiceWorker or render process).
struct ExtensionMessagePort::IPCTarget {
  content::RenderProcessHost* render_process_host;
  content::RenderFrameHost* render_frame_host;
  int worker_thread_id;
};

ExtensionMessagePort::ExtensionMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const std::string& extension_id,
    content::RenderProcessHost* extension_process)
    : weak_channel_delegate_(channel_delegate),
      port_id_(port_id),
      extension_id_(extension_id),
      browser_context_(extension_process->GetBrowserContext()),
      extension_process_(extension_process),
      did_create_port_(false),
      background_host_ptr_(nullptr),
      frame_tracker_(new FrameTracker(this)) {
  auto all_hosts = ProcessManager::Get(browser_context_)
                       ->GetRenderFrameHostsForExtension(extension_id);
  for (content::RenderFrameHost* rfh : all_hosts)
    RegisterFrame(rfh);

  std::vector<WorkerId> running_workers_in_process =
      ProcessManager::Get(browser_context_)
          ->GetServiceWorkers(extension_id_, extension_process_->GetID());
  for (const WorkerId& running_worker_id : running_workers_in_process)
    RegisterWorker(running_worker_id);

  frame_tracker_->TrackExtensionProcessFrames();
}

ExtensionMessagePort::ExtensionMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const std::string& extension_id,
    content::RenderFrameHost* rfh,
    bool include_child_frames)
    : weak_channel_delegate_(channel_delegate),
      port_id_(port_id),
      extension_id_(extension_id),
      browser_context_(rfh->GetProcess()->GetBrowserContext()),
      extension_process_(nullptr),
      did_create_port_(false),
      background_host_ptr_(nullptr),
      frame_tracker_(new FrameTracker(this)) {
  content::WebContents* tab = content::WebContents::FromRenderFrameHost(rfh);
  if (!tab) {
    content::InterstitialPage* interstitial =
        content::InterstitialPage::FromRenderFrameHost(rfh);
    // A RenderFrameHost must be hosted in a WebContents or InterstitialPage.
    CHECK(interstitial);

    // Only the main frame of an interstitial is supported, because frames in
    // the interstitial do not trigger RenderFrameCreated / RenderFrameDeleted
    // on WebContentObservers. Consequently, (1) we cannot detect removal of
    // RenderFrameHosts, and (2) even if the RenderFrameDeleted is propagated,
    // then WebContentsObserverSanityChecker triggers a CHECK when it detects
    // frame notifications without a corresponding RenderFrameCreated.
    if (!rfh->GetParent()) {
      // It is safe to pass the interstitial's WebContents here because we only
      // use it to observe DidDetachInterstitialPage.
      frame_tracker_->TrackInterstitialFrame(interstitial->GetWebContents(),
                                             rfh);
      RegisterFrame(rfh);
    }
    return;
  }

  frame_tracker_->TrackTabFrames(tab);
  if (include_child_frames) {
    tab->ForEachFrame(base::BindRepeating(&ExtensionMessagePort::RegisterFrame,
                                          base::Unretained(this)));
  } else {
    RegisterFrame(rfh);
  }
}

ExtensionMessagePort::ExtensionMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    content::BrowserContext* browser_context)
    : weak_channel_delegate_(channel_delegate),
      port_id_(port_id),
      browser_context_(browser_context) {}

// static
std::unique_ptr<ExtensionMessagePort> ExtensionMessagePort::CreateForEndpoint(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const std::string& extension_id,
    const ChannelEndpoint& endpoint,
    bool include_child_frames) {
  if (endpoint.is_for_render_frame()) {
    return std::make_unique<ExtensionMessagePort>(
        channel_delegate, port_id, extension_id, endpoint.GetRenderFrameHost(),
        include_child_frames);
  }
  DCHECK(!include_child_frames);
  // NOTE: We don't want all the workers within the extension, so we cannot
  // reuse other constructor from above.
  std::unique_ptr<ExtensionMessagePort> port(new ExtensionMessagePort(
      channel_delegate, port_id, endpoint.browser_context()));
  port->RegisterWorker(endpoint.GetWorkerId());
  return port;
}

ExtensionMessagePort::~ExtensionMessagePort() {}

void ExtensionMessagePort::RemoveCommonFrames(const MessagePort& port) {
  // Avoid overlap in the set of frames to make sure that it does not matter
  // when UnregisterFrame is called.
  for (auto it = frames_.begin(); it != frames_.end();) {
    if (port.HasFrame(*it)) {
      frames_.erase(it++);
    } else {
      ++it;
    }
  }
}

bool ExtensionMessagePort::HasFrame(content::RenderFrameHost* rfh) const {
  return frames_.find(rfh) != frames_.end();
}

bool ExtensionMessagePort::IsValidPort() {
  return !frames_.empty() || !service_workers_.empty();
}

void ExtensionMessagePort::RevalidatePort() {
  // Checks whether the frames to which this port is tied at its construction
  // are still aware of this port's existence. Frames that don't know about
  // the port are removed from the set of frames. This should be used for opener
  // ports because the frame may be navigated before the port was initialized.

  // Only opener ports need to be revalidated, because these are created in the
  // renderer before the browser knows about them.
  if (service_workers_.empty())
    DCHECK(!extension_process_);
  DCHECK_LE(frames_.size() + service_workers_.size(), 1U);

  DCHECK(frames_.empty() ^ service_workers_.empty())
      << "Either frame or Service Worker should be present.";

  // If the port is unknown, the renderer will respond by closing the port.
  // NOTE: There is only one opener target.
  if (!frames_.empty()) {
    SendToIPCTarget({nullptr, *frames_.begin(), kMainThreadId},
                    std::make_unique<ExtensionMsg_ValidateMessagePort>(
                        MSG_ROUTING_NONE, kMainThreadId, port_id_));
    return;
  }
  if (!service_workers_.empty()) {
    const WorkerId& service_worker = *service_workers_.begin();
    SendToIPCTarget(
        {content::RenderProcessHost::FromID(service_worker.render_process_id),
         nullptr, service_worker.thread_id},
        std::make_unique<ExtensionMsg_ValidateMessagePort>(
            MSG_ROUTING_NONE, service_worker.thread_id, port_id_));
  }
}

void ExtensionMessagePort::DispatchOnConnect(
    const std::string& channel_name,
    std::unique_ptr<base::DictionaryValue> source_tab,
    int source_frame_id,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url) {
  SendToPort(base::BindRepeating(
      &ExtensionMessagePort::BuildDispatchOnConnectIPC,
      // Called synchronously.
      base::Unretained(this), channel_name, source_tab.get(), source_frame_id,
      guest_process_id, guest_render_frame_routing_id, source_endpoint,
      target_extension_id, source_url));
}

void ExtensionMessagePort::DispatchOnDisconnect(
    const std::string& error_message) {
  SendToPort(
      base::BindRepeating(&ExtensionMessagePort::BuildDispatchOnDisconnectIPC,
                          base::Unretained(this), error_message));
}

void ExtensionMessagePort::DispatchOnMessage(const Message& message) {
  SendToPort(base::BindRepeating(&ExtensionMessagePort::BuildDeliverMessageIPC,
                                 // Called synchronously.
                                 base::Unretained(this), message));
}

void ExtensionMessagePort::IncrementLazyKeepaliveCount() {
  ProcessManager* pm = ProcessManager::Get(browser_context_);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && BackgroundInfo::HasLazyBackgroundPage(host->extension())) {
    pm->IncrementLazyKeepaliveCount(host->extension(), Activity::MESSAGE_PORT,
                                    PortIdToString(port_id_));
  }

  for (const auto& worker_id : service_workers_) {
    std::string request_uuid = pm->IncrementServiceWorkerKeepaliveCount(
        worker_id, Activity::MESSAGE_PORT, PortIdToString(port_id_));
    if (!request_uuid.empty())
      pending_keepalive_uuids_[worker_id].push_back(request_uuid);
  }

  // Keep track of the background host, so when we decrement, we only do so if
  // the host hasn't reloaded.
  background_host_ptr_ = host;
}

void ExtensionMessagePort::DecrementLazyKeepaliveCount() {
  ProcessManager* pm = ProcessManager::Get(browser_context_);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && host == background_host_ptr_) {
    pm->DecrementLazyKeepaliveCount(host->extension(), Activity::MESSAGE_PORT,
                                    PortIdToString(port_id_));
    return;
  }

  for (const auto& worker_id : service_workers_) {
    auto& uuids = pending_keepalive_uuids_[worker_id];
    DCHECK(!uuids.empty());
    std::string request_uuid = std::move(uuids.back());
    uuids.pop_back();
    pm->DecrementServiceWorkerKeepaliveCount(worker_id, request_uuid,
                                             Activity::MESSAGE_PORT,
                                             PortIdToString(port_id_));
  }
}

void ExtensionMessagePort::OpenPort(int process_id,
                                    const PortContext& port_context) {
  DCHECK((port_context.is_for_render_frame() &&
          port_context.frame->routing_id != MSG_ROUTING_NONE) ||
         (port_context.is_for_service_worker() &&
          port_context.worker->thread_id != kMainThreadId) ||
         extension_process_);

  did_create_port_ = true;
}

void ExtensionMessagePort::ClosePort(int process_id,
                                     int routing_id,
                                     int worker_thread_id) {
  const bool is_for_service_worker = worker_thread_id != kMainThreadId;
  if (!is_for_service_worker && routing_id == MSG_ROUTING_NONE) {
    // The only non-frame-specific message is the response to an unhandled
    // onConnect event in the extension process.
    DCHECK(extension_process_);
    frames_.clear();
    if (!HasReceivers())
      CloseChannel();
    return;
  }

  if (is_for_service_worker) {
    UnregisterWorker(process_id, worker_thread_id);
  } else {
    DCHECK_NE(MSG_ROUTING_NONE, routing_id);
    if (auto* rfh = content::RenderFrameHost::FromID(process_id, routing_id))
      UnregisterFrame(rfh);
  }
}

void ExtensionMessagePort::CloseChannel() {
  std::string error_message = did_create_port_ ? std::string() :
      kReceivingEndDoesntExistError;
  if (weak_channel_delegate_)
    weak_channel_delegate_->CloseChannel(port_id_, error_message);
}

void ExtensionMessagePort::RegisterFrame(content::RenderFrameHost* rfh) {
  // Only register a RenderFrameHost whose RenderFrame has been created, to
  // ensure that we are notified of frame destruction. Without this check,
  // |frames_| can eventually contain a stale pointer because RenderFrameDeleted
  // is not triggered for |rfh|.
  if (rfh->IsRenderFrameLive())
    frames_.insert(rfh);
}

void ExtensionMessagePort::UnregisterFrame(content::RenderFrameHost* rfh) {
  if (frames_.erase(rfh) != 0 && !HasReceivers())
    CloseChannel();
}

bool ExtensionMessagePort::HasReceivers() const {
  return !frames_.empty() || !service_workers_.empty();
}

void ExtensionMessagePort::RegisterWorker(const WorkerId& worker_id) {
  DCHECK(!worker_id.extension_id.empty());
  service_workers_.insert(worker_id);
}

void ExtensionMessagePort::UnregisterWorker(const WorkerId& worker_id) {
  DCHECK_EQ(extension_id_, worker_id.extension_id);
  if (service_workers_.erase(worker_id) == 0)
    return;

  if (!HasReceivers())
    CloseChannel();
}

void ExtensionMessagePort::UnregisterWorker(int render_process_id,
                                            int worker_thread_id) {
  DCHECK_NE(kMainThreadId, worker_thread_id);

  // Note: We iterate through *each* workers belonging to this port to find the
  // worker we are interested in. Since there will only be a handful of such
  // workers, this is OK.
  for (auto iter = service_workers_.begin(); iter != service_workers_.end();) {
    if (iter->render_process_id == render_process_id &&
        iter->thread_id == worker_thread_id) {
      service_workers_.erase(iter);
      break;
    } else {
      ++iter;
    }
  }

  if (!HasReceivers())
    CloseChannel();
}

void ExtensionMessagePort::SendToPort(IPCBuilderCallback ipc_builder) {
  std::vector<IPCTarget> targets;
  {
    // Build the list of targets.
    if (extension_process_) {
      // All extension frames reside in the same process, so we can just send a
      // single IPC message to the extension process as an optimization if
      // there are not Service Worker recipient for this port.
      // The frame tracking is then only used to make sure that the port gets
      // closed when all frames have closed / reloaded.
      targets.push_back({extension_process_, nullptr, kMainThreadId});
    } else {
      for (content::RenderFrameHost* frame : frames_)
        targets.push_back({nullptr, frame, kMainThreadId});
    }

    for (const auto& running_worker : service_workers_) {
      targets.push_back(
          {content::RenderProcessHost::FromID(running_worker.render_process_id),
           nullptr, running_worker.thread_id});
    }
  }

  for (const IPCTarget& target : targets) {
    std::unique_ptr<IPC::Message> ipc_message = ipc_builder.Run(target);
    SendToIPCTarget(target, std::move(ipc_message));
  }
}

void ExtensionMessagePort::SendToIPCTarget(const IPCTarget& target,
                                           std::unique_ptr<IPC::Message> msg) {
  if (target.render_frame_host) {
    msg->set_routing_id(target.render_frame_host->GetRoutingID());
    target.render_frame_host->Send(msg.release());
    return;
  }

  if (target.render_process_host) {
    msg->set_routing_id(MSG_ROUTING_CONTROL);
    target.render_process_host->Send(msg.release());
    return;
  }

  DCHECK(extension_process_);
  msg->set_routing_id(MSG_ROUTING_CONTROL);
  extension_process_->Send(msg.release());
}

std::unique_ptr<IPC::Message> ExtensionMessagePort::BuildDispatchOnConnectIPC(
    const std::string& channel_name,
    const base::DictionaryValue* source_tab,
    int source_frame_id,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url,
    const IPCTarget& target) {
  ExtensionMsg_TabConnectionInfo source;
  if (source_tab) {
    std::unique_ptr<base::Value> source_tab_value =
        base::Value::ToUniquePtrValue(source_tab->Clone());
    // TODO(lazyboy): Make ExtensionMsg_TabConnectionInfo.tab a base::Value and
    // remove this cast.
    source.tab.Swap(
        static_cast<base::DictionaryValue*>(source_tab_value.get()));
  }
  source.frame_id = source_frame_id;

  ExtensionMsg_ExternalConnectionInfo info;
  info.target_id = target_extension_id;
  info.source_endpoint = source_endpoint;
  info.source_url = source_url;
  info.guest_process_id = guest_process_id;
  info.guest_render_frame_routing_id = guest_render_frame_routing_id;

  return std::make_unique<ExtensionMsg_DispatchOnConnect>(
      MSG_ROUTING_NONE, target.worker_thread_id, port_id_, channel_name, source,
      info);
}

std::unique_ptr<IPC::Message>
ExtensionMessagePort::BuildDispatchOnDisconnectIPC(
    const std::string& error_message,
    const IPCTarget& target) {
  return std::make_unique<ExtensionMsg_DispatchOnDisconnect>(
      MSG_ROUTING_NONE, target.worker_thread_id, port_id_, error_message);
}

std::unique_ptr<IPC::Message> ExtensionMessagePort::BuildDeliverMessageIPC(
    const Message& message,
    const IPCTarget& target) {
  return std::make_unique<ExtensionMsg_DeliverMessage>(
      MSG_ROUTING_NONE, target.worker_thread_id, port_id_, message);
}

}  // namespace extensions

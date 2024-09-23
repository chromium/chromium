// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/extension_message_port.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/types/optional_util.h"
#include "base/types/pass_key.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/service_worker/service_worker_host.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace extensions {

namespace {

std::string PortIdToString(const extensions::PortId& port_id) {
  return base::StrCat({port_id.GetChannelId().first.ToString(), ":",
                       base::NumberToString(port_id.GetChannelId().second)});
}

using PassKey = base::PassKey<ExtensionMessagePort>;

const char kReceivingEndDoesntExistError[] =
    // TODO(lazyboy): Test these in service worker implementation.
    "Could not establish connection. Receiving end does not exist.";
const char kClosedWhileResponsePendingError[] =
    "A listener indicated an asynchronous response by returning true, but the "
    "message channel closed before a response was received";
const char kClosedWhenPageEntersBFCache[] =
    "The page keeping the extension port is moved into back/forward cache, so "
    "the message channel is closed.";

}  // namespace

// Helper class to detect when frames are destroyed.
class ExtensionMessagePort::FrameTracker : public content::WebContentsObserver,
                                           public ProcessManagerObserver {
 public:
  explicit FrameTracker(ExtensionMessagePort* port) : port_(port) {}

  FrameTracker(const FrameTracker&) = delete;
  FrameTracker& operator=(const FrameTracker&) = delete;

  ~FrameTracker() override = default;

  void TrackExtensionProcessFrames() {
    pm_observation_.Observe(ProcessManager::Get(port_->browser_context_));
  }

  void TrackTabFrames(content::WebContents* tab) { Observe(tab); }

 private:
  // content::WebContentsObserver overrides:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    port_->UnregisterFrame(render_frame_host);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (base::FeatureList::IsEnabled(
            features::kDisconnectExtensionMessagePortWhenPageEntersBFCache) &&
        navigation_handle->HasCommitted()) {
      // Close the channel and force all the ports from the channel to be
      // closed when the old RFH is going to be stored in BFCache. They will
      // not be able to receive any message sent through the port.
      content::RenderFrameHost* previous_rfh = content::RenderFrameHost::FromID(
          navigation_handle->GetPreviousRenderFrameHostId());
      if (previous_rfh &&
          previous_rfh->GetLifecycleState() ==
              content::RenderFrameHost::LifecycleState::kInBackForwardCache) {
        if (port_->UnregisterFramesUnderMainFrame(
                previous_rfh, kClosedWhenPageEntersBFCache)) {
          // Since the channel and the port is already closed, we don't have to
          // run the following block to unregister the frames any more.
          return;
        }
      }
    }

    // There are a number of possible scenarios for the navigation:
    // 1. Same-document navigation - Don't unregister the frame, since it can
    // still use the port.
    // 2. Cross-document navigation, reusing the RenderFrameHost - Unregister
    // the frame, since the new document is not allowed to use the port.
    // 3. Cross-document navigation, with a new RenderFrameHost - Since the
    // navigated-to document has a new RFH, the port can not be registered for
    // it, so it doesn't matter whether we unregister it or not. If the
    // navigated-from document is stored in the back-forward cache, don't
    // unregister the frame (see note below). If it is not cached, the frame
    // will be unregistered when the RFH is deleted.
    // 4. Restoring a cached frame from back-forward cache or activating a
    // prerendered frame - This is similar to (3) in that the navigation changes
    // RFH, with the difference that the RFH is not new and so may be
    // registered. Don't unregister the frame in this case since it may still
    // use the port.

    // Note that we don't just disconnect channels when a frame is bf-cached
    // since when such a document is later restored, there is no "load" and so a
    // message channel won't be immediately available to extensions.
    // Contrast this with a normal load where an extension is able to inject
    // scripts at "document_start" and set up message ports.
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsSameDocument() &&
        !navigation_handle->IsPageActivation()) {
      // Note: This unregisters the _new_ RenderFrameHost. In case a new RFH was
      // created for this navigation, this will be a no-op, since we haven't
      // seen it before. In case the RFH is reused for the navigation, this will
      // correctly unregister the frame, to avoid messages intended for the
      // previous document being sent to the new document. If the navigated-to
      // RFH is served from cache, keep the port alive.
      port_->UnregisterFrame(navigation_handle->GetRenderFrameHost());
    }
  }

  // extensions::ProcessManagerObserver overrides:
  void OnExtensionFrameUnregistered(
      const ExtensionId& extension_id,
      content::RenderFrameHost* render_frame_host) override {
    if (extension_id == port_->extension_id_)
      port_->UnregisterFrame(render_frame_host);
  }

  void OnStoppedTrackingServiceWorkerInstance(
      const WorkerId& worker_id) override {
    port_->UnregisterWorker(worker_id);
  }

  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      pm_observation_{this};
  raw_ptr<ExtensionMessagePort> port_;  // Owns this FrameTracker.
};

ExtensionMessagePort::ExtensionMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host,
    bool include_child_frames)
    : MessagePort(std::move(channel_delegate), port_id),
      extension_id_(extension_id),
      browser_context_(render_frame_host->GetProcess()->GetBrowserContext()),
      frame_tracker_(new FrameTracker(this)) {
  content::WebContents* tab =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  CHECK(tab);
  frame_tracker_->TrackTabFrames(tab);
  if (include_child_frames) {
    // TODO(crbug.com/40189370) We don't yet support MParch for
    // prerender so make sure `include_child_frames` is only provided for
    // primary main frames.
    CHECK(render_frame_host->IsInPrimaryMainFrame());
    render_frame_host->ForEachRenderFrameHostWithAction(
        [tab, this](content::RenderFrameHost* render_frame_host) {
          // RegisterFrame should only be called for frames associated with
          // `tab` and not any inner WebContents.
          if (content::WebContents::FromRenderFrameHost(render_frame_host) !=
              tab) {
            return content::RenderFrameHost::FrameIterationAction::
                kSkipChildren;
          }
          RegisterFrame(render_frame_host);
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        });
  } else {
    RegisterFrame(render_frame_host);
  }
}

// static
std::unique_ptr<ExtensionMessagePort> ExtensionMessagePort::CreateForExtension(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  auto port = std::make_unique<ExtensionMessagePort>(
      channel_delegate, port_id, extension_id, browser_context, PassKey());
  port->frame_tracker_ = std::make_unique<FrameTracker>(port.get());
  port->frame_tracker_->TrackExtensionProcessFrames();

  port->for_all_extension_contexts_ = true;

  auto* process_manager = ProcessManager::Get(browser_context);
  auto all_hosts =
      process_manager->GetRenderFrameHostsForExtension(extension_id);
  for (content::RenderFrameHost* render_frame_host : all_hosts) {
    port->RegisterFrame(render_frame_host);
  }

  std::vector<WorkerId> running_workers =
      process_manager->GetServiceWorkersForExtension(extension_id);
  for (const WorkerId& running_worker_id : running_workers)
    port->RegisterWorker(running_worker_id);

  return port;
}

// static
std::unique_ptr<ExtensionMessagePort> ExtensionMessagePort::CreateForEndpoint(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const ExtensionId& extension_id,
    const ChannelEndpoint& endpoint,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> message_port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        message_port_host) {
  auto port = std::make_unique<ExtensionMessagePort>(
      channel_delegate, port_id, extension_id, endpoint.browser_context(),
      PassKey());
  port->frame_tracker_ = std::make_unique<FrameTracker>(port.get());

  if (endpoint.is_for_render_frame()) {
    content::RenderFrameHost* render_frame_host = endpoint.GetRenderFrameHost();
    content::WebContents* tab =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    CHECK(tab);
    port->frame_tracker_->TrackTabFrames(tab);
    auto& receiver = port->frames_[render_frame_host->GetGlobalFrameToken()];
    receiver.Bind(std::move(message_port));
    receiver.set_disconnect_handler(base::BindOnce(
        &ExtensionMessagePort::Prune, base::Unretained(port.get())));
  } else {
    port->frame_tracker_->TrackExtensionProcessFrames();
    auto& receiver = port->service_workers_[endpoint.GetWorkerId()];
    receiver.Bind(std::move(message_port));
    receiver.set_disconnect_handler(base::BindOnce(
        &ExtensionMessagePort::Prune, base::Unretained(port.get())));
  }
  port->AddReceiver(std::move(message_port_host), endpoint.render_process_id(),
                    endpoint.port_context());
  return port;
}

ExtensionMessagePort::ExtensionMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    PassKey)
    : MessagePort(std::move(channel_delegate), port_id),
      extension_id_(extension_id),
      browser_context_(browser_context) {}

ExtensionMessagePort::~ExtensionMessagePort() = default;

void ExtensionMessagePort::Prune() {
  std::vector<content::GlobalRenderFrameHostToken> frames_to_unregister;
  for (auto& frame : frames_) {
    if (!frame.second.is_connected()) {
      frames_to_unregister.push_back(frame.first);
    }
  }
  for (const auto& frame_token : frames_to_unregister) {
    if (UnregisterFrame(frame_token)) {
      return;
    }
  }

  std::vector<WorkerId> workers_to_unregister;
  for (auto& worker : service_workers_) {
    if (!worker.second.is_connected()) {
      workers_to_unregister.push_back(worker.first);
    }
  }
  for (const auto& worker : workers_to_unregister) {
    if (UnregisterWorker(worker)) {
      return;
    }
  }
}

void ExtensionMessagePort::RemoveCommonFrames(const MessagePort& port) {
  // This should be called before OnConnect is called.
  CHECK(frames_.empty());
  // Avoid overlap in the set of frames to make sure that it does not matter
  // when UnregisterFrame is called.
  std::erase_if(
      pending_frames_,
      [&port](const content::GlobalRenderFrameHostToken& frame_token) {
        return port.HasFrame(frame_token);
      });
}

bool ExtensionMessagePort::HasFrame(
    const content::GlobalRenderFrameHostToken& frame_token) const {
  return base::Contains(frames_, frame_token) ||
         base::Contains(pending_frames_, frame_token);
}

bool ExtensionMessagePort::IsValidPort() {
  return !frames_.empty() || !service_workers_.empty() ||
         !pending_frames_.empty() || !pending_service_workers_.empty();
}

void ExtensionMessagePort::RevalidatePort() {
  // Checks whether the frames to which this port is tied at its construction
  // are still aware of this port's existence. Frames that don't know about
  // the port are removed from the set of frames. This should be used for opener
  // ports because the frame may be navigated before the port was initialized.

  // Only opener ports need to be revalidated, because these are created in the
  // renderer before the browser knows about them.
  DCHECK(!for_all_extension_contexts_);
  DCHECK_EQ(frames_.size() + service_workers_.size() + pending_frames_.size() +
                pending_service_workers_.size(),
            1U)
      << "RevalidatePort() should only be called for opener ports which "
         "correspond to a single 'context'.";

  // NOTE: There is only one opener target.
  if (!frames_.empty()) {
    if (!frames_.begin()->second.is_connected()) {
      UnregisterFrame(frames_.begin()->first);
    }
    return;
  }
  if (!service_workers_.empty()) {
    if (!service_workers_.begin()->second.is_connected()) {
      UnregisterWorker(service_workers_.begin()->first);
    }
    return;
  }
}

void ExtensionMessagePort::DispatchOnConnect(
    mojom::ChannelType channel_type,
    const std::string& channel_name,
    std::optional<base::Value::Dict> source_tab,
    const ExtensionApiFrameIdMap::FrameData& source_frame,
    int guest_process_id,
    int guest_render_frame_routing_id,
    const MessagingEndpoint& source_endpoint,
    const std::string& target_extension_id,
    const GURL& source_url,
    std::optional<url::Origin> source_origin) {
  mojom::TabConnectionInfoPtr source = mojom::TabConnectionInfo::New();

  // Source document ID should exist if and only if there is a source tab.
  DCHECK_EQ(!!source_tab, !!source_frame.document_id);
  if (source_tab) {
    source->tab = source_tab->Clone();
    source->document_id = source_frame.document_id.ToString();
    source->document_lifecycle = ToString(source_frame.document_lifecycle);
  }
  source->frame_id = source_frame.frame_id;

  mojom::ExternalConnectionInfoPtr info = mojom::ExternalConnectionInfo::New();
  info->target_id = target_extension_id;
  info->source_endpoint = source_endpoint;
  info->source_url = source_url;
  info->source_origin = std::move(source_origin);
  info->guest_process_id = guest_process_id;
  info->guest_render_frame_routing_id = guest_render_frame_routing_id;

  // `ShouldSkipFrameForBFCache` could mutate `pending_frames_` so we
  // take it before iterating on it.
  auto pending_frames = std::move(pending_frames_);
  for (const auto& frame_token : pending_frames) {
    auto* frame = content::RenderFrameHost::FromFrameToken(frame_token);
    if (!frame || ShouldSkipFrameForBFCache(frame)) {
      continue;
    }

    mojo::PendingAssociatedReceiver<mojom::MessagePort> message_port;
    mojo::PendingAssociatedRemote<mojom::MessagePortHost> message_port_host;

    auto& receiver = frames_[frame_token];
    receiver.Bind(message_port.InitWithNewEndpointAndPassRemote());
    receiver.set_disconnect_handler(
        base::BindOnce(&ExtensionMessagePort::Prune, base::Unretained(this)));
    AddReceiver(message_port_host.InitWithNewEndpointAndPassReceiver(),
                frame->GetProcess()->GetID(),
                PortContext::ForFrame(frame->GetRoutingID()));

    ExtensionWebContentsObserver::GetForWebContents(
        content::WebContents::FromRenderFrameHost(frame))
        ->GetLocalFrameChecked(frame)
        .DispatchOnConnect(
            port_id_, channel_type, channel_name, source.Clone(), info.Clone(),
            std::move(message_port), std::move(message_port_host),
            base::BindOnce(&ExtensionMessagePort::OnConnectResponse,
                           weak_ptr_factory_.GetWeakPtr()));
  }
  for (const auto& worker : pending_service_workers_) {
    auto* host = ServiceWorkerHost::GetWorkerFor(worker);
    if (host) {
      auto* service_worker_remote = host->GetServiceWorker();
      if (!service_worker_remote) {
        continue;
      }
      mojo::PendingAssociatedReceiver<mojom::MessagePort> message_port;
      mojo::PendingAssociatedRemote<mojom::MessagePortHost> message_port_host;

      auto& receiver = service_workers_[worker];
      receiver.Bind(message_port.InitWithNewEndpointAndPassRemote());
      receiver.set_disconnect_handler(
          base::BindOnce(&ExtensionMessagePort::Prune, base::Unretained(this)));
      AddReceiver(message_port_host.InitWithNewEndpointAndPassReceiver(),
                  worker.render_process_id,
                  PortContext::ForWorker(worker.thread_id, worker.version_id,
                                         worker.extension_id));
      service_worker_remote->DispatchOnConnect(
          port_id_, channel_type, channel_name, source.Clone(), info.Clone(),
          std::move(message_port), std::move(message_port_host),
          base::BindOnce(&ExtensionMessagePort::OnConnectResponse,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  pending_frames_.clear();
  pending_service_workers_.clear();
}

void ExtensionMessagePort::OnConnectResponse(bool success) {
  // For the unsuccessful case the port will be cleaned up in `Prune` when
  // the mojo channels are disconnected.
  if (success) {
    port_was_created_ = true;
  }
}

void ExtensionMessagePort::DispatchOnDisconnect(
    const std::string& error_message) {
  SendToPort(base::BindRepeating(
      [](const std::string& error_message, mojom::MessagePort* port) {
        port->DispatchDisconnect(error_message);
      },
      std::ref(error_message)));
}

void ExtensionMessagePort::DispatchOnMessage(const Message& message) {
  // We increment activity for every message that passes through the channel.
  // This is important for long-lived ports, which only keep an extension
  // alive so long as they are being actively used.
  IncrementLazyKeepaliveCount(Activity::MESSAGE);
  // Since we are now receiving a message, we can mark any asynchronous reply
  // that may have been pending for this port as no longer pending.
  asynchronous_reply_pending_ = false;
  SendToPort(base::BindRepeating(
      [](const Message& message, mojom::MessagePort* port) {
        port->DeliverMessage(message);
      },
      std::ref(message)));
  DecrementLazyKeepaliveCount(Activity::MESSAGE);
}

void ExtensionMessagePort::IncrementLazyKeepaliveCount(
    Activity::Type activity_type) {
  ProcessManager* pm = ProcessManager::Get(browser_context_);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && BackgroundInfo::HasLazyBackgroundPage(host->extension())) {
    pm->IncrementLazyKeepaliveCount(host->extension(), activity_type,
                                    PortIdToString(port_id_));
  }

  // Keep track of the background host, so when we decrement, we only do so if
  // the host hasn't reloaded.
  background_host_ptr_ = host;

  if (!IsServiceWorkerActivity(activity_type)) {
    return;
  }

  // Increment keepalive count for service workers of the extension managed by
  // this port.
  // TODO(crbug.com/41487026): Add a check to only increment count if
  // the port is in lazy context.
  for (const auto& worker_id :
       pm->GetServiceWorkersForExtension(extension_id_)) {
    base::Uuid request_uuid = pm->IncrementServiceWorkerKeepaliveCount(
        worker_id,
        should_have_strong_keepalive()
            ? content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout
            : content::ServiceWorkerExternalRequestTimeoutType::kDefault,
        activity_type, PortIdToString(port_id_));
    pending_keepalive_uuids_[worker_id].push_back(std::move(request_uuid));
  }
}

void ExtensionMessagePort::DecrementLazyKeepaliveCount(
    Activity::Type activity_type) {
  ProcessManager* pm = ProcessManager::Get(browser_context_);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id_);
  if (host && host == background_host_ptr_) {
    pm->DecrementLazyKeepaliveCount(host->extension(), activity_type,
                                    PortIdToString(port_id_));
    return;
  }

  if (!IsServiceWorkerActivity(activity_type)) {
    return;
  }

  // Decrement keepalive count for service workers of the extension managed by
  // this port.
  // TODO(crbug.com/41487026): Add a check to only decrement count if
  // the port is in lazy context.
  for (const auto& worker_id :
       pm->GetServiceWorkersForExtension(extension_id_)) {
    auto iter = pending_keepalive_uuids_.find(worker_id);
    if (iter == pending_keepalive_uuids_.end()) {
      // We may not have a pending keepalive if this worker wasn't created at
      // the time the message channel opened.
      continue;
    }
    base::Uuid request_uuid = std::move(iter->second.back());
    iter->second.pop_back();
    if (iter->second.empty()) {
      pending_keepalive_uuids_.erase(iter);
    }
    pm->DecrementServiceWorkerKeepaliveCount(
        worker_id, request_uuid, activity_type, PortIdToString(port_id_));
  }
}

void ExtensionMessagePort::NotifyResponsePending() {
  asynchronous_reply_pending_ = true;
}

void ExtensionMessagePort::OpenPort(int process_id,
                                    const PortContext& port_context) {
  DCHECK((port_context.is_for_render_frame() &&
          port_context.frame->routing_id != MSG_ROUTING_NONE) ||
         (port_context.is_for_service_worker() &&
          port_context.worker->thread_id != kMainThreadId) ||
         for_all_extension_contexts_);

  port_was_created_ = true;
}

void ExtensionMessagePort::ClosePort(int process_id,
                                     int routing_id,
                                     int worker_thread_id) {
  const bool is_for_service_worker = worker_thread_id != kMainThreadId;
  DCHECK(is_for_service_worker || routing_id != MSG_ROUTING_NONE);

  if (is_for_service_worker) {
    UnregisterWorker(process_id, worker_thread_id);
  } else if (auto* render_frame_host =
                 content::RenderFrameHost::FromID(process_id, routing_id)) {
    UnregisterFrame(render_frame_host);
  }
}

void ExtensionMessagePort::CloseChannel(
    std::optional<std::string> error_message) {
  std::string error;
  if (error_message.has_value()) {
    error = std::move(error_message).value();
  } else if (!port_was_created_) {
    error = kReceivingEndDoesntExistError;
  } else if (asynchronous_reply_pending_) {
    error = kClosedWhileResponsePendingError;
  }

  if (weak_channel_delegate_) {
    weak_channel_delegate_->CloseChannel(port_id_, error);
  }
}

void ExtensionMessagePort::RegisterFrame(
    content::RenderFrameHost* render_frame_host) {
  // Only register a RenderFrameHost whose RenderFrame has been created, to
  // ensure that we are notified of frame destruction. Without this check,
  // `pending_frames_` can contain a stale token because RenderFrameDeleted
  // is not triggered for `render_frame_host`.
  if (render_frame_host->IsRenderFrameLive()) {
    pending_frames_.insert(render_frame_host->GetGlobalFrameToken());
  }
}

bool ExtensionMessagePort::UnregisterFrame(
    content::RenderFrameHost* render_frame_host) {
  return UnregisterFrame(render_frame_host->GetGlobalFrameToken());
}

bool ExtensionMessagePort::UnregisterFrame(
    const content::GlobalRenderFrameHostToken& frame_token) {
  frames_.erase(frame_token);
  pending_frames_.erase(frame_token);
  if (!IsValidPort()) {
    CloseChannel();
    return true;
  }
  return false;
}

bool ExtensionMessagePort::UnregisterFramesUnderMainFrame(
    content::RenderFrameHost* main_frame,
    std::optional<std::string> error_message) {
  CHECK(pending_frames_.empty());
  if (std::erase_if(frames_,
                    [&main_frame](const auto& item) {
                      auto* frame =
                          content::RenderFrameHost::FromFrameToken(item.first);
                      return !frame ||
                             frame->GetOutermostMainFrame() == main_frame;
                    }) != 0 &&
      !IsValidPort()) {
    CloseChannel(error_message);
    return true;
  }

  return false;
}

void ExtensionMessagePort::RegisterWorker(const WorkerId& worker_id) {
  DCHECK(!worker_id.extension_id.empty());
  pending_service_workers_.insert(worker_id);
}

bool ExtensionMessagePort::UnregisterWorker(const WorkerId& worker_id) {
  if (extension_id_ != worker_id.extension_id)
    return false;
  service_workers_.erase(worker_id);
  pending_service_workers_.erase(worker_id);

  if (!IsValidPort()) {
    CloseChannel();
    return true;
  }
  return false;
}

void ExtensionMessagePort::UnregisterWorker(int render_process_id,
                                            int worker_thread_id) {
  DCHECK_NE(kMainThreadId, worker_thread_id);

  // Note: We iterate through *each* workers belonging to this port to find the
  // worker we are interested in. Since there will only be a handful of such
  // workers, this is OK.
  for (auto iter = service_workers_.begin(); iter != service_workers_.end();) {
    if (iter->first.render_process_id == render_process_id &&
        iter->first.thread_id == worker_thread_id) {
      service_workers_.erase(iter);
      break;
    } else {
      ++iter;
    }
  }

  if (!IsValidPort()) {
    CloseChannel();
  }
}

void ExtensionMessagePort::SendToPort(SendCallback send_callback) {
  // We should have called OnConnect before SentToPort.
  CHECK(pending_frames_.empty());
  CHECK(pending_service_workers_.empty());
  std::vector<content::GlobalRenderFrameHostToken> frame_targets;
  // Build the list of targets.
  for (const auto& item : frames_) {
    frame_targets.push_back(item.first);
  }

  for (const auto& target : frame_targets) {
    auto item = frames_.find(target);
    // `ShouldSkipFrameForBFCache` can mutate `frames_`, so verify the frame
    // still exists.
    if (item == frames_.end()) {
      continue;
    }
    auto* frame = content::RenderFrameHost::FromFrameToken(item->first);
    if (frame && !ShouldSkipFrameForBFCache(frame)) {
      send_callback.Run(item->second.get());
    }
  }

  for (const auto& running_worker : service_workers_) {
    send_callback.Run(running_worker.second.get());
  }
}

bool ExtensionMessagePort::IsServiceWorkerActivity(
    Activity::Type activity_type) {
  switch (activity_type) {
    case Activity::MESSAGE:
      return true;
    case Activity::MESSAGE_PORT:
      // long-lived  message channels (such as through runtime.connect()) only
      // increment keepalive when a message is sent so that a port doesn't count
      // as a single, long-running task.
      return is_for_onetime_channel() || should_have_strong_keepalive();
    default:
      // Extension message port should not check for other activity types.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool ExtensionMessagePort::ShouldSkipFrameForBFCache(
    content::RenderFrameHost* render_frame_host) {
  // Frames in the BackForwardCache are not allowed to receive messages (or
  // even have them queued). In such a case, we evict the page from the cache
  // and "drop" the message (See comment in `DidFinishNavigation()`).
  // Note: Since this will cause the frame to be deleted, we do this here
  // instead of in the loop above to avoid modifying `frames_` while it is
  // being iterated.
  //
  // This could cause the same page to be evicted multiple times if it has
  // multiple frames receiving this message. This is harmless as the reason is
  // the same in every case. Also multiple extensions may send messages before
  // the page is actually evicted. The last one will be the one the user
  // sees. It is not worth the effort to present all of them to the user. It's
  // unlikely they will see the same one every time and if they do, when they
  // fix that one, they will see the others.
  //
  // TODO (crbug.com/1382623): currently we only make use of the base URL,
  // it's also possible to get the full URL from extension ID so it could
  // provide more useful context.
  if (render_frame_host &&
      render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kInBackForwardCache)) {
    // The ExtensionMessagePort should be disconnected when the page enters
    // BFCache if `kDisconnectExtensionMessagePortWhenPageEntersBFCache` is
    // enabled, so no message will be sent to the BFCached target. There could
    // be some messages that were created before the ExtensionMessagePort is
    // disconnected, and they should be discarded.
    // TODO(crbug.com/40283601): clean up the flag.
    if (!base::FeatureList::IsEnabled(
            features::kDisconnectExtensionMessagePortWhenPageEntersBFCache)) {
      content::BackForwardCache::DisableForRenderFrameHost(
          render_frame_host,
          back_forward_cache::DisabledReason(
              back_forward_cache::DisabledReasonId::
                  kExtensionSentMessageToCachedFrame,
              /*context=*/extension_id_),
          ukm::UkmRecorder::GetSourceIdForExtensionUrl(
              base::PassKey<ExtensionMessagePort>(),
              Extension::GetBaseURLFromExtensionId(extension_id_)));
    }
    return true;
  }
  return false;
}

}  // namespace extensions

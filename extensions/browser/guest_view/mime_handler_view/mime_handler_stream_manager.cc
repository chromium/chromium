// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {
namespace {

class MimeHandlerStreamManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  MimeHandlerStreamManagerFactory();
  static MimeHandlerStreamManagerFactory* GetInstance();
  MimeHandlerStreamManager* Get(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

MimeHandlerStreamManagerFactory::MimeHandlerStreamManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "MimeHandlerStreamManager",
          BrowserContextDependencyManager::GetInstance()) {
}

// static
MimeHandlerStreamManagerFactory*
MimeHandlerStreamManagerFactory::GetInstance() {
  return base::Singleton<MimeHandlerStreamManagerFactory>::get();
}

MimeHandlerStreamManager* MimeHandlerStreamManagerFactory::Get(
    content::BrowserContext* context) {
  return static_cast<MimeHandlerStreamManager*>(
      GetServiceForBrowserContext(context, true));
}

KeyedService* MimeHandlerStreamManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new MimeHandlerStreamManager();
}

content::BrowserContext*
MimeHandlerStreamManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

}  // namespace

// A WebContentsObserver that observes for a particular RenderFrameHost either
// navigating or closing (including by crashing). This is necessary to ensure
// that streams that aren't claimed by a MimeHandlerViewGuest are not leaked, by
// aborting the stream if any of those events occurs.
class MimeHandlerStreamManager::EmbedderObserver
    : public content::WebContentsObserver {
 public:
  EmbedderObserver(MimeHandlerStreamManager* stream_manager,
                   const std::string& view_id,
                   int frame_tree_node_id,
                   int render_process_id,
                   int render_frame_id);

 private:
  // WebContentsObserver overrides.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  void AbortStream();

  bool IsTrackedRenderFrameHost(content::RenderFrameHost* render_frame_host);

  MimeHandlerStreamManager* const stream_manager_;
  const std::string view_id_;
  int frame_tree_node_id_;
  int render_process_id_;
  int render_frame_id_;
  // We get an initial  load notification for the URL the mime handler is
  // serving. We don't want to clean up the stream here. This field helps us
  // track the first load notification. Defaults to true.
  bool initial_load_for_frame_;
  // If a RFH is swapped with another RFH, this is set to the new RFH. This
  // ensures that we don't inadvarently clean up the stream when the old RFH
  // dies.
  content::RenderFrameHost* new_host_;
};

MimeHandlerStreamManager::MimeHandlerStreamManager() = default;
MimeHandlerStreamManager::~MimeHandlerStreamManager() = default;

// static
MimeHandlerStreamManager* MimeHandlerStreamManager::Get(
    content::BrowserContext* context) {
  return MimeHandlerStreamManagerFactory::GetInstance()->Get(context);
}

void MimeHandlerStreamManager::AddStream(
    const std::string& view_id,
    std::unique_ptr<StreamContainer> stream,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id) {
  streams_by_extension_id_[stream->extension_id()].insert(view_id);
  auto result = streams_.insert(std::make_pair(view_id, std::move(stream)));
  DCHECK(result.second);
  embedder_observers_[view_id] = std::make_unique<EmbedderObserver>(
      this, view_id, frame_tree_node_id, render_process_id, render_frame_id);
}

std::unique_ptr<StreamContainer> MimeHandlerStreamManager::ReleaseStream(
    const std::string& view_id) {
  auto stream = streams_.find(view_id);
  if (stream == streams_.end())
    return nullptr;

  std::unique_ptr<StreamContainer> result =
      base::WrapUnique(stream->second.release());
  streams_by_extension_id_[result->extension_id()].erase(view_id);
  streams_.erase(stream);
  embedder_observers_.erase(view_id);
  return result;
}

void MimeHandlerStreamManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  auto streams = streams_by_extension_id_.find(extension->id());
  if (streams == streams_by_extension_id_.end())
    return;

  for (const auto& view_id : streams->second) {
    streams_.erase(view_id);
    embedder_observers_.erase(view_id);
  }
  streams_by_extension_id_.erase(streams);
}

MimeHandlerStreamManager::EmbedderObserver::EmbedderObserver(
    MimeHandlerStreamManager* stream_manager,
    const std::string& view_id,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id)
    : stream_manager_(stream_manager),
      view_id_(view_id),
      frame_tree_node_id_(frame_tree_node_id),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      initial_load_for_frame_(true),
      new_host_(nullptr) {
  content::WebContents* web_contents = nullptr;
  if (frame_tree_node_id != -1) {
    web_contents =
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  } else {
    web_contents = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  }
  content::WebContentsObserver::Observe(web_contents);
}

void MimeHandlerStreamManager::EmbedderObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!IsTrackedRenderFrameHost(render_frame_host))
    return;

  // The MimeHandlerStreamManager::EmbedderObserver is initialized before the
  // final RenderFrameHost for the navigation has been chosen. When it is later
  // picked, a specualtive RenderFrameHost might be deleted. Do not abort the
  // stream in that case.
  if (frame_tree_node_id_ != -1 && !render_frame_host->IsCurrent())
    return;

  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::RenderProcessGone(
    base::TerminationStatus status) {
  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !IsTrackedRenderFrameHost(navigation_handle->GetRenderFrameHost())) {
    return;
  }

  // We get an initial load notification for the URL we are serving. We don't
  // want to clean up the stream here. Update the RenderFrameHost tracking.
  if (initial_load_for_frame_) {
    initial_load_for_frame_ = false;
    frame_tree_node_id_ = -1;
    render_frame_id_ = navigation_handle->GetRenderFrameHost()->GetRoutingID();
    render_process_id_ =
        navigation_handle->GetRenderFrameHost()->GetProcess()->GetID();
    return;
  }
  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the top level frame is navigating away, clean up the stream.
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    AbortStream();
  }
}

void MimeHandlerStreamManager::EmbedderObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // If the old_host is null, then it means that a subframe is being created.
  // Don't treat this like a host change.
  if (!old_host)
    return;

  // If this is an unrelated host, ignore.
  if ((frame_tree_node_id_ != -1 &&
       old_host->GetFrameTreeNodeId() != frame_tree_node_id_) ||
      (render_frame_id_ != -1 &&
       (old_host->GetRoutingID() != render_frame_id_ ||
        (old_host->GetProcess()->GetID() != render_process_id_)))) {
    return;
  }

  new_host_ = new_host;
  // Update the RFID, RPIDs to those of the new RFH. This ensures
  // that if the new RFH gets deleted before loading the stream, we will
  // abort it.
  DCHECK((frame_tree_node_id_ == -1) ||
         (frame_tree_node_id_ == new_host_->GetFrameTreeNodeId()));
  render_frame_id_ = new_host_->GetRoutingID();
  render_process_id_ = new_host_->GetProcess()->GetID();
  // No need to keep this around anymore since we have valid render frame IDs
  // now.
  frame_tree_node_id_ = -1;
}

void MimeHandlerStreamManager::EmbedderObserver::WebContentsDestroyed() {
  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::AbortStream() {
  Observe(nullptr);
  // This will cause the stream to be destroyed.
  stream_manager_->ReleaseStream(view_id_);
}

bool MimeHandlerStreamManager::EmbedderObserver::IsTrackedRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  // We don't want to abort the stream if the frame we were tracking changed to
  // new_host_.
  if (new_host_ && (render_frame_host != new_host_))
    return false;

  if (frame_tree_node_id_ != -1) {
    return render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id_;
  } else {
    DCHECK((render_frame_id_ != -1) && (render_process_id_ != -1));
    return render_frame_host->GetRoutingID() == render_frame_id_ &&
           render_frame_host->GetProcess()->GetID() == render_process_id_;
  }
}

}  // namespace extensions

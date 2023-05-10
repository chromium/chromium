// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frames_manager.h"

#import <set>

#import "base/ios/device_util.h"
#import "components/js_injection/browser/js_communication_host.h"
#import "content/public/browser/navigation_handle.h"
#import "content/public/browser/page.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/js_messaging/content_web_frame.h"
#import "ios/web/content/js_messaging/ios_web_message_host_factory.h"
#import "ios/web/content/web_state/content_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWebFramesManager::ContentWebFramesManager(
    ContentWebState* content_web_state)
    : content::WebContentsObserver(content_web_state->GetWebContents()),
      content_web_state_(content_web_state),
      js_communication_host_(
          std::make_unique<js_injection::JsCommunicationHost>(
              content_web_state->GetWebContents())) {
  auto message_host_factory = std::make_unique<IOSWebMessageHostFactory>();
  js_communication_host_->AddWebMessageHostFactory(
      std::move(message_host_factory), u"webkitMessageHandler", {"*"});
}

ContentWebFramesManager::~ContentWebFramesManager() = default;

void ContentWebFramesManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentWebFramesManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::set<WebFrame*> ContentWebFramesManager::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : available_frame_hosts_) {
    frames.insert(WebFrameForContentId(it));
  }
  return frames;
}

WebFrame* ContentWebFramesManager::GetMainWebFrame() {
  auto web_id_it = content_to_web_id_map_.find(main_frame_content_id_);
  if (web_id_it == content_to_web_id_map_.end()) {
    return nullptr;
  }

  return GetFrameWithId(web_id_it->second);
}

WebFrame* ContentWebFramesManager::GetFrameWithId(const std::string& frame_id) {
  if (frame_id.empty()) {
    return nullptr;
  }
  auto web_frames_it = web_frames_.find(frame_id);
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

void ContentWebFramesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/1423527): Ensure that the random id chosen here is either
  // injected into the frame or directly attached to JavaScript messages
  // received from the frame, since features expect this.
  std::string web_frame_id = ios::device_util::GetRandomId();
  auto web_frame = std::make_unique<ContentWebFrame>(
      web_frame_id, render_frame_host, content_web_state_);
  web_frames_[web_frame_id] = std::move(web_frame);
  content_to_web_id_map_[render_frame_host->GetGlobalId()] = web_frame_id;
}

void ContentWebFramesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  content::GlobalRenderFrameHostId content_id =
      render_frame_host->GetGlobalId();
  auto web_id_it = content_to_web_id_map_.find(content_id);
  DCHECK(web_id_it != content_to_web_id_map_.end());

  if (available_frame_hosts_.count(content_id)) {
    for (auto& observer : observers_) {
      observer.WebFrameBecameUnavailable(this, web_id_it->second);
    }
    available_frame_hosts_.erase(content_id);
  }

  if (main_frame_content_id_ == content_id) {
    main_frame_content_id_ = content::GlobalRenderFrameHostId();
  }

  web_frames_.erase(web_id_it->second);
  content_to_web_id_map_.erase(web_id_it);
}

void ContentWebFramesManager::PrimaryPageChanged(content::Page& page) {
  main_frame_content_id_ = page.GetMainDocument().GetGlobalId();
}

void ContentWebFramesManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  // Some navigations (e.g., downloads, 204 responses) do not have a
  // RenderFrameHost.
  if (!render_frame_host) {
    return;
  }

  content::GlobalRenderFrameHostId content_id =
      render_frame_host->GetGlobalId();
  if (available_frame_hosts_.count(content_id)) {
    return;
  }

  available_frame_hosts_.insert(content_id);

  // Notify observers here rather than in `RenderFrameCreated`, to
  // ensure that frames are no longer in a speculative lifecycle
  // phase where JavaScript injection is not yet allowed. crbug.com/1183639
  // tracks delaying `RenderFrameCreated` until frames are past the
  // speculative state, which is not intended to be exposed to embedders.
  WebFrame* web_frame = WebFrameForContentId(content_id);
  for (auto& observer : observers_) {
    observer.WebFrameBecameAvailable(this, web_frame);
  }
}

WebFrame* ContentWebFramesManager::WebFrameForContentId(
    content::GlobalRenderFrameHostId content_id) {
  auto web_id_it = content_to_web_id_map_.find(content_id);
  DCHECK(web_id_it != content_to_web_id_map_.end());
  auto web_frame_it = web_frames_.find(web_id_it->second);
  DCHECK(web_frame_it != web_frames_.end());

  return web_frame_it->second.get();
}

}  // namespace web

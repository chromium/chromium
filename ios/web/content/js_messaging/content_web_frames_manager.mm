// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frames_manager.h"

#import <set>

#import "base/ios/device_util.h"
#import "content/public/browser/page.h"
#import "content/public/browser/web_contents.h"
#import "ios/web/content/js_messaging/content_web_frame.h"
#import "ios/web/content/web_state/content_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWebFramesManager::ContentWebFramesManager(
    ContentWebState* content_web_state)
    : content::WebContentsObserver(content_web_state->GetWebContents()),
      content_web_state_(content_web_state) {}

ContentWebFramesManager::~ContentWebFramesManager() = default;

void ContentWebFramesManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentWebFramesManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::set<WebFrame*> ContentWebFramesManager::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : web_frames_) {
    frames.insert(it.second.get());
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
  WebFrame* new_frame = web_frame.get();
  web_frames_[web_frame_id] = std::move(web_frame);
  content_to_web_id_map_[render_frame_host->GetGlobalId()] = web_frame_id;

  for (auto& observer : observers_) {
    observer.WebFrameBecameAvailable(this, new_frame);
  }
}

void ContentWebFramesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  content::GlobalRenderFrameHostId content_id =
      render_frame_host->GetGlobalId();
  auto web_id_it = content_to_web_id_map_.find(content_id);
  DCHECK(web_id_it != content_to_web_id_map_.end());

  for (auto& observer : observers_) {
    observer.WebFrameBecameUnavailable(this, web_id_it->second);
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

}  // namespace web

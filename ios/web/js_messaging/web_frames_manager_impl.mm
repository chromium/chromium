// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frames_manager_impl.h"

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebFramesManagerImpl::WebFramesManagerImpl() : weak_factory_(this) {}

WebFramesManagerImpl::~WebFramesManagerImpl() = default;

bool WebFramesManagerImpl::AddFrame(std::unique_ptr<WebFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->GetFrameId().empty());
  if (frame->IsMainFrame()) {
    if (main_web_frame_) {
      // A main frame is already registered, ignore duplicate registration
      // message.
      return false;
    }
    main_web_frame_ = frame.get();
  }
  DCHECK(web_frames_.count(frame->GetFrameId()) == 0);
  std::string frame_id = frame->GetFrameId();
  web_frames_[frame_id] = std::move(frame);
  return true;
}

void WebFramesManagerImpl::RemoveFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  // If the removed frame is a main frame, it should be the current one.
  DCHECK(web_frames_.count(frame_id) == 0 ||
         !web_frames_[frame_id]->IsMainFrame() ||
         main_web_frame_ == web_frames_[frame_id].get());
  if (web_frames_.count(frame_id) == 0) {
    return;
  }
  if (main_web_frame_ && main_web_frame_->GetFrameId() == frame_id) {
    main_web_frame_ = nullptr;
  }
  // The web::WebFrame destructor can call some callbacks that will try to
  // access the frame via GetFrameWithId. This can lead to a reentrancy issue
  // on |web_frames_|.
  // To avoid this issue, keep the frame alive during the map operation and
  // destroy it after.
  auto keep_frame_alive = std::move(web_frames_[frame_id]);
  web_frames_.erase(frame_id);
}

#pragma mark - WebFramesManager

std::set<WebFrame*> WebFramesManagerImpl::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : web_frames_) {
    frames.insert(it.second.get());
  }
  return frames;
}

WebFrame* WebFramesManagerImpl::GetMainWebFrame() {
  return main_web_frame_;
}

WebFrame* WebFramesManagerImpl::GetFrameWithId(const std::string& frame_id) {
  DCHECK(!frame_id.empty());
  auto web_frames_it = web_frames_.find(frame_id);
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

}  // namespace

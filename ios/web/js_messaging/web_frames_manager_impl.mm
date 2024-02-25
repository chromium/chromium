// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_manager_impl.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace web {

#pragma mark - WebFramesManagerImpl

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
  WebFrame* added_frame = frame.get();
  web_frames_[frame_id] = std::move(frame);

  for (auto& observer : observers_) {
    observer.WebFrameBecameAvailable(this, added_frame);
  }
  return true;
}

void WebFramesManagerImpl::RemoveFrameWithId(const std::string& frame_id) {
  for (auto& observer : observers_) {
    observer.WebFrameBecameUnavailable(this, frame_id);
  }

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
  // on `web_frames_`.
  // To avoid this issue, keep the frame alive during the map operation and
  // destroy it after.
  auto keep_frame_alive = std::move(web_frames_[frame_id]);
  web_frames_.erase(frame_id);
}

void WebFramesManagerImpl::RemoveAllWebFrames() {
  std::set<std::string> frame_ids;
  for (const auto& it : web_frames_) {
    frame_ids.insert(it.first);
  }
  for (std::string frame_id : frame_ids) {
    RemoveFrameWithId(frame_id);
  }
}

#pragma mark - WebFramesManager

void WebFramesManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebFramesManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

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
  if (frame_id.empty()) {
    return nullptr;
  }

  auto web_frames_it = web_frames_.find(base::ToLowerASCII(frame_id));
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

}  // namespace

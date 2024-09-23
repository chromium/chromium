// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_frames_manager.h"

#import "base/strings/string_util.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace web {

FakeWebFramesManager::FakeWebFramesManager() {}
FakeWebFramesManager::~FakeWebFramesManager() {}

void FakeWebFramesManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeWebFramesManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::set<WebFrame*> FakeWebFramesManager::GetAllWebFrames() {
  std::set<WebFrame*> frames;
  for (const auto& it : web_frames_) {
    frames.insert(it.second.get());
  }
  return frames;
}

WebFrame* FakeWebFramesManager::GetMainWebFrame() {
  return main_web_frame_;
}

WebFrame* FakeWebFramesManager::GetFrameWithId(const std::string& frame_id) {
  auto web_frames_it = web_frames_.find(base::ToLowerASCII(frame_id));
  return web_frames_it == web_frames_.end() ? nullptr
                                            : web_frames_it->second.get();
}

void FakeWebFramesManager::AddWebFrame(std::unique_ptr<WebFrame> frame) {
  DCHECK(frame);
  if (frame->IsMainFrame()) {
    main_web_frame_ = frame.get();
  }
  WebFrame* added_frame = frame.get();
  web_frames_[frame->GetFrameId()] = std::move(frame);

  for (auto& observer : observers_) {
    observer.WebFrameBecameAvailable(this, added_frame);
  }
}

void FakeWebFramesManager::RemoveWebFrame(const std::string& frame_id) {
  for (auto& observer : observers_) {
    observer.WebFrameBecameUnavailable(this, frame_id);
  }

  // If the removed frame is a main frame, it should be the current one.
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

}  // namespace web

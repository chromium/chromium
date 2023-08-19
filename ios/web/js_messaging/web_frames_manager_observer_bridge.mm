// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"

namespace web {

WebFramesManagerObserverBridge::WebFramesManagerObserverBridge(
    id<CRWWebFramesManagerObserver> observer)
    : observer_(observer) {}

WebFramesManagerObserverBridge::~WebFramesManagerObserverBridge() = default;

void WebFramesManagerObserverBridge::WebFrameBecameAvailable(
    WebFramesManager* web_frames_manager,
    WebFrame* web_frame) {
  SEL selector = @selector(webFramesManager:frameBecameAvailable:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webFramesManager:web_frames_manager
           frameBecameAvailable:web_frame];
  }
}

void WebFramesManagerObserverBridge::WebFrameBecameUnavailable(
    WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  SEL selector = @selector(webFramesManager:frameBecameUnavailable:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webFramesManager:web_frames_manager
         frameBecameUnavailable:frame_id];
  }
}

}  // namespace web

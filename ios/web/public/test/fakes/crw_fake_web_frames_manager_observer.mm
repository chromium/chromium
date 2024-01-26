// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_frames_manager_observer.h"

#import "base/memory/raw_ptr.h"

@implementation CRWFakeWebFramesManagerObserver {
  raw_ptr<web::WebFrame> _lastAvailableFrame;
  raw_ptr<web::WebFramesManager> _lastWebFramesManager;
  std::string _lastUnvailableFrameId;
}

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  _lastWebFramesManager = webFramesManager;
  _lastAvailableFrame = webFrame;
}

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameUnavailable:(const std::string&)frameId {
  _lastWebFramesManager = webFramesManager;
  _lastUnvailableFrameId = frameId;
}

- (web::WebFrame*)lastAvailableFrame {
  return _lastAvailableFrame;
}

- (web::WebFramesManager*)lastWebFramesManager {
  return _lastWebFramesManager;
}

- (const std::string&)lastUnavailableFrameId {
  return _lastUnvailableFrameId;
}

@end

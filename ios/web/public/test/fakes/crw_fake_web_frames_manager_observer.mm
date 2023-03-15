// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_frames_manager_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWebFramesManagerObserver {
  web::WebFrame* _lastAvailableFrame;
  web::WebFramesManager* _lastWebFramesManager;
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

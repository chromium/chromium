// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_FRAMES_MANAGER_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_FRAMES_MANAGER_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"

// Test implementation of CRWWebFramesManagerObserver protocol.
@interface CRWFakeWebFramesManagerObserver
    : NSObject <CRWWebFramesManagerObserver>

// Last frames manager passed to `webFramesManager:frameBecameAvailable:` or
// `webFramesManager:frameBecameUnavailable:`.
@property(nonatomic, readonly) web::WebFramesManager* lastWebFramesManager;
// Last frame passed to `webFramesManager:frameBecameAvailable:`.
@property(nonatomic, readonly) web::WebFrame* lastAvailableFrame;
// Last frameId passed to `webFramesManager:frameBecameUnavailable:`.
@property(nonatomic, readonly) const std::string& lastUnavailableFrameId;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_FRAMES_MANAGER_OBSERVER_H_

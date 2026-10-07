// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_backend.h"

// Backend implementation managing bidirectional streaming with a live model
// over WebSockets using Apple's native NSURLSessionWebSocketTask.
//
// Chromium on iOS does not use Chromium's C++ net/websockets stack for browser
// feature WebSockets (where enable_websockets is disabled). Native Apple
// NSURLSessionWebSocketTask is used for direct WebSocket communication.
@interface TTCWebSocketBackend : NSObject <TTCBackend>

// Designated initializer allowing injection of a custom NSURLSession (used for
// testing with mock sessions). If `session` is nil, a default NSURLSession is
// created lazily upon calling `-connect`.
// @param session An optional custom NSURLSession for dependency injection, or
// nil.
- (instancetype)initWithSession:(NSURLSession*)session
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer creating a default NSURLSession and loading
// configuration via `ios::provider::GetTTCConfig()`.
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_

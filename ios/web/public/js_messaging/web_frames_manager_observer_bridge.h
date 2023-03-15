// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#import "ios/web/public/js_messaging/web_frames_manager.h"

// Observes web frames manager events from Objective-C. To use as a
// web::WebFramesManager::Observer, wrap in a
// web::WebFramesManagerObserverBridge.
@protocol CRWWebFramesManagerObserver <NSObject>
@optional

// Invoked by WebFramesManagerObserverBridge::WebFrameBecameAvailable.
- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame;

// Invoked by WebFramesManagerObserverBridge::WebFrameBecameUnavailable.
- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameUnavailable:(const std::string&)frameId;

@end

namespace web {

// Bridge to use an id<CRWWebFramesManagerObserver> as a
// web::WebFramesManager::Observer.
class WebFramesManagerObserverBridge : public WebFramesManager::Observer {
 public:
  // It it the responsibility of calling code to add/remove the instance
  // from the WebFramesManager observer list.
  WebFramesManagerObserverBridge(id<CRWWebFramesManagerObserver> observer);

  WebFramesManagerObserverBridge(const WebFramesManagerObserverBridge&) =
      delete;
  WebFramesManagerObserverBridge& operator=(
      const WebFramesManagerObserverBridge&) = delete;

  ~WebFramesManagerObserverBridge() override;

  // web::WebFramesManager::Observer:
  void WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                               WebFrame* web_frame) override;
  void WebFrameBecameUnavailable(WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

 private:
  __weak id<CRWWebFramesManagerObserver> observer_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_OBSERVER_BRIDGE_H_

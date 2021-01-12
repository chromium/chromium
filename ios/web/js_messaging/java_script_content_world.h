// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_

#include <set>

#import <WebKit/WebKit.h>

namespace web {

class JavaScriptFeature;

// Represents a content world which can be configured with a given set of
// JavaScriptFeatures. An isolated world prevents the loaded web page’s
// JavaScript from interacting with the browser's feature JavaScript. This can
// improve the security and robustness of the feature JavaScript.
class JavaScriptContentWorld {
 public:
  ~JavaScriptContentWorld();
  JavaScriptContentWorld(const JavaScriptContentWorld&) = delete;

  // Creates a content world for features which will interact with the page
  // content world shared by the webpage's JavaScript.
  explicit JavaScriptContentWorld(
      WKUserContentController* user_content_controller);

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // Creates a content world for features which will interact with the given
  // |content_world|.
  JavaScriptContentWorld(WKUserContentController* user_content_controller,
                         WKContentWorld* content_world)
      API_AVAILABLE(ios(14.0));
#endif  // defined(__IPHONE14_0)

  // Adds |feature| by configuring the feature scripts and communication
  // callbacks.
  void AddFeature(const JavaScriptFeature* feature);

  // Returns true if and only if |feature| has been added to this content world.
  bool HasFeature(const JavaScriptFeature* feature);

 private:
  // The features which have already been configured for |content_world_|.
  std::set<const JavaScriptFeature*> features_;

  // The associated user content controller for configuring injected scripts and
  // communication.
  WKUserContentController* user_content_controller_ = nil;

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // The associated WKContentWorld. May be null which represents the main world
  // which the page content itself uses. (The same content world can also be
  // represented by [WKContentWorld pageWorld] on iOS 14 and later.)
  WKContentWorld* content_world_ API_AVAILABLE(ios(14.0)) = nullptr;
#endif  // defined(__IPHONE14_0)
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_

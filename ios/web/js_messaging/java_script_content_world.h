// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_

#include <map>
#include <memory>
#include <set>

#import <WebKit/WebKit.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class BrowserState;
class JavaScriptFeature;

// Represents a content world which can be configured with a given set of
// JavaScriptFeatures. An isolated world prevents the loaded web page’s
// JavaScript from interacting with the browser's feature JavaScript. This can
// improve the security and robustness of a feature's JavaScript.
// NOTE: Destruction of a JavaScriptContentWorld can not completely clean up
// the state added to the WKUserContentController associated with
// `browser_state` because WKUserContentController does not expose API to remove
// specific WKUserScript instances.
class JavaScriptContentWorld {
 public:
  ~JavaScriptContentWorld();
  JavaScriptContentWorld(const JavaScriptContentWorld&) = delete;

  // Creates a content world for features which will interact with the given
  // `content_world`. `browser_state` and `content_world` must not be null.
  JavaScriptContentWorld(BrowserState* browser_state,
                         WKContentWorld* content_world);

  // Returns the associated WKContentWorld.
  WKContentWorld* GetWKContentWorld();

  // Adds `feature` by configuring the feature scripts and communication
  // callbacks.
  void AddFeature(const JavaScriptFeature* feature);

  // Returns true if and only if `feature` has been added to this content world.
  bool HasFeature(const JavaScriptFeature* feature);

 private:
  // Processes the response of a script message and forwards it to `handler`.
  void ScriptMessageReceived(JavaScriptFeature::ScriptMessageHandler handler,
                             BrowserState* browser_state,
                             WKScriptMessage* script_message);

  // The features which have already been configured for `content_world_`.
  std::set<const JavaScriptFeature*> features_;

  // The associated browser state for configuring injected scripts and
  // communication.
  raw_ptr<BrowserState> browser_state_;

  // The associated user content controller for configuring injected scripts and
  // script message handler JavaScript->native communication.
  WKUserContentController* user_content_controller_;

  std::map<const JavaScriptFeature*,
           std::unique_ptr<ScopedWKScriptMessageHandler>>
      script_message_handlers_;

  // The associated underlying WKContentWorld.
  WKContentWorld* content_world_;

  base::WeakPtrFactory<JavaScriptContentWorld> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_CONTENT_WORLD_H_

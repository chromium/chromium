// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_JAVA_SCRIPT_FEATURE_H_

#import <WebKit/WebKit.h>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class BrowserState;

// A feature which notifies the native application code of the creation and
// destruction of webpage frames based on JavaScript messages from the webpage.
class WebFramesManagerJavaScriptFeature : public base::SupportsUserData::Data,
                                          public JavaScriptFeature {
 public:
  WebFramesManagerJavaScriptFeature(BrowserState* browser_state);
  ~WebFramesManagerJavaScriptFeature() override;

  // Returns the WebFramesManagerJavaScriptFeature associated with
  // `browser_state`, creating one if necessary. `browser_state` must not be
  // null.
  static WebFramesManagerJavaScriptFeature* FromBrowserState(
      BrowserState* browser_state);

  // Configures message handlers for the creation and destruction of frames.
  // `user_content_controller` is used directly (instead of using the built-in
  // JavaScriptFeature message handling) because constructing WebFrame instances
  // requires access to the WKScriptMessage's WKFrameInfo instance.
  void ConfigureHandlers(WKUserContentController* user_content_controller);

 private:
  friend class WebFramesManagerJavaScriptFeatureTest;

  WebFramesManagerJavaScriptFeature(const WebFramesManagerJavaScriptFeature&) =
      delete;
  WebFramesManagerJavaScriptFeature& operator=(
      const WebFramesManagerJavaScriptFeature&) = delete;

  // Handles a message from JavaScript to register a new WebFrame.
  void FrameAvailableMessageReceived(WKScriptMessage* script_message);
  // Handles a message from JavaScript to remove a WebFrame.
  void FrameUnavailableMessageReceived(WKScriptMessage* script_message);

  // The browser state associated with this instance of the feature.
  BrowserState* browser_state_;

  // This feature uses ScopedWKScriptMessageHandler directly instead of the
  // message handling built into JavaScriptFeature because creating WebFrames
  // requires a pointer to the WKFrameInfo object (which is intentionally hidden
  // from JavaScriptFeature::ScriptMessageReceived).
  std::unique_ptr<ScopedWKScriptMessageHandler> frame_available_handler_;
  std::unique_ptr<ScopedWKScriptMessageHandler> frame_unavailable_handler_;

  base::WeakPtrFactory<WebFramesManagerJavaScriptFeature> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_JAVA_SCRIPT_FEATURE_H_

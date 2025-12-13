// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class WebFrame;

// A feature to intercept clipboard API calls.
class ClipboardJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ClipboardJavaScriptFeature* GetInstance();

  ClipboardJavaScriptFeature(const ClipboardJavaScriptFeature&) = delete;
  ClipboardJavaScriptFeature& operator=(const ClipboardJavaScriptFeature&) =
      delete;

 private:
  friend class base::NoDestructor<ClipboardJavaScriptFeature>;
  friend class ClipboardJavaScriptFeatureTest;

  ClipboardJavaScriptFeature();
  ~ClipboardJavaScriptFeature() override;

  // JavaScriptFeature overrides.
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  // Handles messages sent from the injected JavaScript.
  // This message is sent whenever a clipboard API is called.
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

  // Handles a "read" or "write" command from the web page.
  void HandleClipboardRequest(WebState* web_state,
                              WebFrame* web_frame,
                              int request_id,
                              const std::string& command);

  // Calls the JavaScript function `__gCrWeb.clipboard.resolveRequest` to settle
  // the promise for the given `request_id`.
  // `web_frame` is the frame where the script should be executed.
  // `command` is the clipboard command ("read" or "write").
  // `allowed` indicates whether the clipboard operation was permitted.
  void ResolveClipboardRequest(int request_id,
                               base::WeakPtr<WebFrame> web_frame,
                               const std::string& command,
                               bool allowed);
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CLIPBOARD_CLIPBOARD_JAVA_SCRIPT_FEATURE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_MESSAGING_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_WEBUI_WEB_UI_MESSAGING_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// Listens for script command messages and forwards them to the associated
// WebState.
class WebUIMessagingJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static WebUIMessagingJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<WebUIMessagingJavaScriptFeature>;

  // JavaScriptFeature overrides
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& script_message) override;

  WebUIMessagingJavaScriptFeature();
  ~WebUIMessagingJavaScriptFeature() override;

  WebUIMessagingJavaScriptFeature(const WebUIMessagingJavaScriptFeature&) =
      delete;
  WebUIMessagingJavaScriptFeature& operator=(
      const WebUIMessagingJavaScriptFeature&) = delete;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_WEB_UI_MESSAGING_JAVA_SCRIPT_FEATURE_H_

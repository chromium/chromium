// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_SCRIPT_COMMAND_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_MESSAGING_SCRIPT_COMMAND_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// Listens for script command messages and forwards them to the associated
// WebState.
class ScriptCommandJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ScriptCommandJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<ScriptCommandJavaScriptFeature>;

  // JavaScriptFeature overrides
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& script_message) override;

  ScriptCommandJavaScriptFeature();
  ~ScriptCommandJavaScriptFeature() override;

  ScriptCommandJavaScriptFeature(const ScriptCommandJavaScriptFeature&) =
      delete;
  ScriptCommandJavaScriptFeature& operator=(
      const ScriptCommandJavaScriptFeature&) = delete;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_SCRIPT_COMMAND_JAVA_SCRIPT_FEATURE_H_

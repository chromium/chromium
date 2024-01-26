// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "ios/web/public/js_messaging/script_message.h"

namespace web {

class WebFrame;
class WebState;

// The text added to the page by `kJavaScriptFeatureTestScript` on document
// load.
extern const char kFakeJavaScriptFeatureLoadedText[];

// The test message handler name.
extern const char kFakeJavaScriptFeatureScriptHandlerName[];

extern const char kFakeJavaScriptFeaturePostMessageReplyValue[];

// A JavaScriptFeature which exposes functions to modify the DOM and trigger a
// post message.
class FakeJavaScriptFeature : public JavaScriptFeature {
 public:
  FakeJavaScriptFeature(ContentWorld content_world);
  ~FakeJavaScriptFeature() override;

  // Executes `kJavaScriptFeatureTestScriptReplaceDivContents` in `web_frame`.
  void ReplaceDivContents(WebFrame* web_frame);

  // Executes `kJavaScriptFeatureTestScriptReplyWithPostMessage` with
  // `parameters` in `web_frame`.
  void ReplyWithPostMessage(WebFrame* web_frame,
                            const base::Value::List& parameters);

  // Executes `kJavaScriptFeatureTestScriptReplyWithPostMessage` with
  // `parameters` in `web_frame` using __gCrWeb.common.sendWebKitMessage.
  void ReplyWithPostMessageCommonJS(WebFrame* web_frame,
                                    const base::Value::List& parameters);

  // Returns the number of errors received
  void GetErrorCount(WebFrame* web_frame,
                     base::OnceCallback<void(const base::Value*)> callback);

  WebState* last_received_web_state() const { return last_received_web_state_; }

  const ScriptMessage* last_received_message() const {
    return last_received_message_.get();
  }

 private:
  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

  raw_ptr<WebState> last_received_web_state_ = nullptr;
  std::unique_ptr<const ScriptMessage> last_received_message_;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_

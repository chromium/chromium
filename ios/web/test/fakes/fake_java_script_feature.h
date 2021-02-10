// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

@class WKScriptMessage;

namespace web {

class BrowserState;
class WebFrame;

// The text added to the page by |kJavaScriptFeatureTestScript| on document
// load.
extern const char kFakeJavaScriptFeatureLoadedText[];

// The test message handler name.
extern const char kFakeJavaScriptFeatureScriptHandlerName[];

extern const char kFakeJavaScriptFeaturePostMessageReplyValue[];

// A JavaScriptFeature which exposes functions to modify the DOM and trigger a
// post message.
class FakeJavaScriptFeature : public JavaScriptFeature {
 public:
  FakeJavaScriptFeature(JavaScriptFeature::ContentWorld content_world);
  ~FakeJavaScriptFeature() override;

  // Executes |kJavaScriptFeatureTestScriptReplaceDivContents| in |web_frame|.
  void ReplaceDivContents(WebFrame* web_frame);

  // Executes |kJavaScriptFeatureTestScriptReplyWithPostMessage| with
  // |parameters| in |web_frame|.
  void ReplyWithPostMessage(WebFrame* web_frame,
                            const std::vector<base::Value>& parameters);

  // Returns the number of errors received
  void GetErrorCount(WebFrame* web_frame,
                     base::OnceCallback<void(const base::Value*)> callback);

  BrowserState* last_received_browser_state() const {
    return last_received_browser_state_;
  }

  WKScriptMessage* last_received_message() const {
    return last_received_message_;
  }

 private:
  // JavaScriptFeature:
  base::Optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(BrowserState* browser_state,
                             WKScriptMessage* message) override;

  BrowserState* last_received_browser_state_ = nullptr;
  WKScriptMessage* last_received_message_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_

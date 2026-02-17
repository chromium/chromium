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
  FakeJavaScriptFeature(ContentWorld content_world, OriginFilter origin_filter);
  FakeJavaScriptFeature(ContentWorld content_world);
  ~FakeJavaScriptFeature() override;

  // Executes `kJavaScriptFeatureTestScriptReplaceDivContents` in `web_frame`.
  void ReplaceDivContents(WebFrame* web_frame);

  // Executes `kJavaScriptFeatureTestScriptReplyWithPostMessage` with
  // `parameters` in `web_frame`.
  void ReplyWithPostMessage(WebFrame* web_frame,
                            const base::ListValue& parameters);

  // Returns the number of errors received
  void GetErrorCount(WebFrame* web_frame,
                     base::OnceCallback<void(const base::Value*)> callback);

  WebState* last_received_web_state() const { return last_received_web_state_; }

  const ScriptMessage* last_received_message() const {
    return last_received_message_.get();
  }

  // Number of message received from the web page.
  int received_message_count() const { return received_message_count_; }

  // Sets the feature to reply to messages from the page.
  void SetReplyToMessages(bool reply);

  // Sets the reply for the next message received.
  void SetResponseToNextMessage(std::string response);

 private:
  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  bool GetFeatureRepliesToMessages() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

  void ScriptMessageReceivedWithReply(
      WebState* web_state,
      const ScriptMessage& message,
      ScriptMessageReplyCallback callback) override;

  raw_ptr<WebState, DanglingUntriaged> last_received_web_state_ = nullptr;
  std::unique_ptr<const ScriptMessage> last_received_message_;
  int received_message_count_ = 0;
  bool reply_to_messages_ = false;
  std::optional<std::string> response_to_next_message_;

  // FakeJavaScriptFeature has its own WeakPtrFactory to bind
  // ScriptMessageReceivedWithReply.
  base::WeakPtrFactory<FakeJavaScriptFeature> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_JAVA_SCRIPT_FEATURE_H_

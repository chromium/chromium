// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_java_script_feature.h"

#import "base/time/time.h"

namespace web {

// Filenames of the Javascript injected by FakeJavaScriptFeature which creates
// a text node on document load with the text
// `kFakeJavaScriptFeatureLoadedText`, exposes the function
// `kScriptReplaceDivContents` and tracks the count of received errors.
const char kJavaScriptFeatureInjectOnceTestScript[] =
    "java_script_feature_test_inject_once";
const char kJavaScriptFeatureReinjectTestScript[] =
    "java_script_feature_test_reinject";

const char kFakeJavaScriptFeatureLoadedText[] = "injected_script_loaded";

// The function exposed by the feature JS which replaces the contents of the div
// with `id="div"| with the text "updated".
const char kScriptReplaceDivContents[] =
    "javaScriptFeatureTest.replaceDivContents";

const char kFakeJavaScriptFeatureScriptHandlerName[] = "FakeHandlerName";

const char kFakeJavaScriptFeaturePostMessageReplyValue[] = "some text";

// The function exposed by the feature JS which returns the parameter value as a
// postMessage to the script message handler with name
// `kFakeJavaScriptFeatureScriptHandlerName`.
const char kScriptReplyWithPostMessage[] =
    "javaScriptFeatureTest.replyWithPostMessage";

// The function exposed by the feature JS which returns the parameter value as a
// postMessage to the script message handler with name
// `kFakeJavaScriptFeatureScriptHandlerName`.
const char kScriptReplyWithPostMessageAndPostReply[] =
    "javaScriptFeatureTest.replyWithPostMessageAndPostReply";

// The function exposed by the feature JS which returns the count of errors
// received in the JS error listener.
const char kGetErrorCount[] = "javaScriptFeatureTest.getErrorCount";

// Timeout for response of kGetErrorCount.
const int kGetErrorCountTimeout = 1;

FakeJavaScriptFeature::FakeJavaScriptFeature(ContentWorld content_world,
                                             OriginFilter origin_filter)
    : JavaScriptFeature(
          content_world,
          {FeatureScript::CreateWithFilename(
               kJavaScriptFeatureInjectOnceTestScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
               FeatureScript::PlaceholderReplacementsCallback(),
               origin_filter),
           FeatureScript::CreateWithFilename(
               kJavaScriptFeatureReinjectTestScript,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation,
               FeatureScript::PlaceholderReplacementsCallback(),
               origin_filter)},
          {},
          origin_filter),
      weak_factory_(this) {}

FakeJavaScriptFeature::FakeJavaScriptFeature(ContentWorld content_world)
    : FakeJavaScriptFeature(content_world, OriginFilter::kPublic) {}
FakeJavaScriptFeature::~FakeJavaScriptFeature() = default;

void FakeJavaScriptFeature::ReplaceDivContents(WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, kScriptReplaceDivContents, {});
}

void FakeJavaScriptFeature::ReplyWithPostMessage(
    WebFrame* web_frame,
    const base::ListValue& parameters) {
  if (reply_to_messages_) {
    CallJavaScriptFunction(web_frame, kScriptReplyWithPostMessageAndPostReply,
                           parameters);
  } else {
    CallJavaScriptFunction(web_frame, kScriptReplyWithPostMessage, parameters);
  }
}

void FakeJavaScriptFeature::GetErrorCount(
    WebFrame* web_frame,
    base::OnceCallback<void(const base::Value*)> callback) {
  CallJavaScriptFunction(web_frame, kGetErrorCount, {}, std::move(callback),
                         base::Seconds(kGetErrorCountTimeout));
}

std::optional<std::string> FakeJavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return std::string(kFakeJavaScriptFeatureScriptHandlerName);
}

void FakeJavaScriptFeature::SetReplyToMessages(bool reply) {
  reply_to_messages_ = reply;
}

bool FakeJavaScriptFeature::GetFeatureRepliesToMessages() const {
  return reply_to_messages_;
}

void FakeJavaScriptFeature::SetResponseToNextMessage(std::string response) {
  response_to_next_message_ = response;
}

void FakeJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& message) {
  last_received_web_state_ = web_state;
  last_received_message_ = std::make_unique<const ScriptMessage>(message);
  received_message_count_++;
}

void FakeJavaScriptFeature::ScriptMessageReceivedWithReply(
    WebState* web_state,
    const ScriptMessage& message,
    ScriptMessageReplyCallback callback) {
  last_received_web_state_ = web_state;
  last_received_message_ = std::make_unique<const ScriptMessage>(message);
  received_message_count_++;

  if (response_to_next_message_) {
    base::Value response(*response_to_next_message_);
    std::move(callback).Run(&response, nil);
    response_to_next_message_.reset();
  } else {
    std::move(callback).Run(nullptr, @"Error");
  }
}

}  // namespace web

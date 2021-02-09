// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Filename of the Javascript injected by FakeJavaScriptFeature which creates
// a text node on document load with the text
// |kFakeJavaScriptFeatureLoadedText| and exposes
// the function |kScriptReplaceDivContents|.
const char kJavaScriptFeatureTestScript[] = "java_script_feature_test_js";

const char kFakeJavaScriptFeatureLoadedText[] = "injected_script_loaded";

// The function exposed by kJavaScriptFeatureTestScript which replaces the
// contents of the div with |id="div"| with the text "updated".
const char kScriptReplaceDivContents[] =
    "javaScriptFeatureTest.replaceDivContents";

const char kFakeJavaScriptFeatureScriptHandlerName[] = "FakeHandlerName";

const char kFakeJavaScriptFeaturePostMessageReplyValue[] = "some text";

// The function exposed by kJavaScriptFeatureTestScript which returns the
// parameter value as a postMessage to the script message handler with name
// |kFakeJavaScriptFeatureScriptHandlerName|.
const char kScriptReplyWithPostMessage[] =
    "javaScriptFeatureTest.replyWithPostMessage";

FakeJavaScriptFeature::FakeJavaScriptFeature(
    JavaScriptFeature::ContentWorld content_world)
    : JavaScriptFeature(content_world,
                        {FeatureScript::CreateWithFilename(
                            kJavaScriptFeatureTestScript,
                            FeatureScript::InjectionTime::kDocumentEnd,
                            FeatureScript::TargetFrames::kAllFrames)},
                        {}) {}
FakeJavaScriptFeature::~FakeJavaScriptFeature() = default;

void FakeJavaScriptFeature::ReplaceDivContents(WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, kScriptReplaceDivContents, {});
}

void FakeJavaScriptFeature::ReplyWithPostMessage(
    WebFrame* web_frame,
    const std::vector<base::Value>& parameters) {
  CallJavaScriptFunction(web_frame, kScriptReplyWithPostMessage, parameters);
}

std::vector<std::string> FakeJavaScriptFeature::GetScriptMessageHandlerNames()
    const {
  return {std::string(kFakeJavaScriptFeatureScriptHandlerName)};
}

void FakeJavaScriptFeature::ScriptMessageReceived(BrowserState* browser_state,
                                                  WKScriptMessage* message) {
  last_received_browser_state_ = browser_state;
  last_received_message_ = message;
}

}  // namespace web

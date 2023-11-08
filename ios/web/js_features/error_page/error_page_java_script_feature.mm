// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/error_page/error_page_java_script_feature.h"

#import "base/values.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace web {

namespace {
// The name of the message handler for messages from the error page. Must be
// kept in sync with components/neterror/resources/error_page_controller_ios.js.
const char kWebUIMessageHandlerName[] = "ErrorPageMessage";

// The NSUserDefault key to store the easter egg game on error page high score.
NSString* const kEasterEggHighScore = @"EasterEggHighScore";
}  // namespace

// static
ErrorPageJavaScriptFeature* ErrorPageJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ErrorPageJavaScriptFeature> instance;
  return instance.get();
}

ErrorPageJavaScriptFeature::ErrorPageJavaScriptFeature()
    // This feature must be in the page content world in order to listen for
    // messages from the Error Page JavaScript.
    : JavaScriptFeature(ContentWorld::kPageContentWorld, {}) {}

ErrorPageJavaScriptFeature::~ErrorPageJavaScriptFeature() = default;

std::optional<std::string>
ErrorPageJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWebUIMessageHandlerName;
}

void ErrorPageJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  // WebUI messages are only handled if sent from the main frame.
  if (!script_message.is_main_frame()) {
    return;
  }

  std::optional<GURL> url = script_message.request_url();
  // Messages must be from an error page.
  if (!url || ![CRWErrorPageHelper isErrorPageFileURL:url.value()]) {
    return;
  }

  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = script_message.body()->GetDict();

  const std::string* command = dict.FindString("command");
  if (!command) {
    return;
  }

  if (*command == "updateEasterEggHighScore") {
    const std::string* high_score_string = dict.FindString("highScore");
    if (!high_score_string) {
      return;
    }
    int high_score;
    if (!base::StringToInt(*high_score_string, &high_score)) {
      return;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:high_score
                                               forKey:kEasterEggHighScore];
  } else if (*command == "resetEasterEggHighScore") {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kEasterEggHighScore];
  } else if (*command == "trackEasterEgg") {
    WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
    if (!frame) {
      return;
    }

    int high_score = [[NSUserDefaults standardUserDefaults]
        integerForKey:kEasterEggHighScore];

    auto parameters = base::Value::List().Append(high_score);
    frame->CallJavaScriptFunction(
        "errorPageController.initializeEasterEggHighScore", parameters);
  }
}

}  // namespace web

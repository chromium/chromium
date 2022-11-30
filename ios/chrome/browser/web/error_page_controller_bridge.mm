// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/error_page_controller_bridge.h"

#import <Foundation/Foundation.h>

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Prefix for the errorPageController activity event commands.
// Must be kept in sync with
// components/neterror/resources/error_page_controller_ios.js.
const char kCommandPrefix[] = "errorPageController";

// The NSUserDefault key to store the easter egg game on error page high score.
NSString* const kEasterEggHighScore = @"EasterEggHighScore";
}  // namespace

ErrorPageControllerBridge::ErrorPageControllerBridge(web::WebState* web_state)
    : web_state_(web_state) {}

ErrorPageControllerBridge::~ErrorPageControllerBridge() {}

void ErrorPageControllerBridge::StartHandlingJavascriptCommands() {
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::BindRepeating(&ErrorPageControllerBridge::OnErrorPageCommand,
                          base::Unretained(this)),
      kCommandPrefix);
}

void ErrorPageControllerBridge::OnErrorPageCommand(
    const base::Value& message,
    const GURL& url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  const std::string* command = message.FindStringKey("command");
  if (!command) {
    return;
  }
  if (*command == "errorPageController.updateEasterEggHighScore") {
    const std::string* high_score_string = message.FindStringKey("highScore");
    if (!high_score_string) {
      return;
    }
    int high_score;
    if (!base::StringToInt(*high_score_string, &high_score)) {
      return;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:high_score
                                               forKey:kEasterEggHighScore];
    return;
  }
  if (*command == "errorPageController.resetEasterEggHighScore") {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kEasterEggHighScore];
    return;
  }
  if (*command == "errorPageController.trackEasterEgg") {
    int high_score = [[NSUserDefaults standardUserDefaults]
        integerForKey:kEasterEggHighScore];
    std::vector<base::Value> parameters;
    parameters.push_back(base::Value(high_score));
    sender_frame->CallJavaScriptFunction(
        "errorPageController.initializeEasterEggHighScore", parameters);
    return;
  }
}

#pragma mark WebStateObserver

void ErrorPageControllerBridge::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  subscription_ = base::CallbackListSubscription();
  web_state_->RemoveObserver(this);
}

void ErrorPageControllerBridge::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(ErrorPageControllerBridge)

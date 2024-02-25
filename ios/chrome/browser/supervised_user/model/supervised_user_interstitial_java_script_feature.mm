// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_interstitial_java_script_feature.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/security_interstitials/core/controller_client.h"
#import "components/supervised_user/core/browser/supervised_user_interstitial.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
// The name of the message handler for messages from the error page. Must be
// kept in sync with components/neterror/resources/error_page_controller_ios.js.
const char kWebUIMessageHandlerName[] = "SupervisedUserInterstitialMessage";

std::optional<security_interstitials::SecurityInterstitialCommand>
GetEnumCommand(const std::string& command) {
  if (command == "requestUrlAccessRemote") {
    return security_interstitials::SecurityInterstitialCommand::
        CMD_REQUEST_SITE_ACCESS_PERMISSION;
  } else if (command == "back") {
    return security_interstitials::SecurityInterstitialCommand::
        CMD_DONT_PROCEED;
  }
  return std::nullopt;
}

}  // namespace

// static
SupervisedUserInterstitialJavaScriptFeature*
SupervisedUserInterstitialJavaScriptFeature::GetInstance() {
  static base::NoDestructor<SupervisedUserInterstitialJavaScriptFeature>
      instance;
  return instance.get();
}

SupervisedUserInterstitialJavaScriptFeature::
    SupervisedUserInterstitialJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld, {}) {}

SupervisedUserInterstitialJavaScriptFeature::
    ~SupervisedUserInterstitialJavaScriptFeature() = default;

std::optional<std::string>
SupervisedUserInterstitialJavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return kWebUIMessageHandlerName;
}

void SupervisedUserInterstitialJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = script_message.body()->GetDict();
  // Expected valid message body struct is:
  // `{"command": "requestUrlAccessRemote"}` or `{"command": "back"}`.
  const std::string* command = dict.FindString("command");
  if (!command) {
    return;
  }

  auto command_enum = GetEnumCommand(*command);
  if (!command_enum.has_value()) {
    return;
  }

  security_interstitials::IOSBlockingPageTabHelper* blocking_tab_helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state);
  if (!blocking_tab_helper) {
    return;
  }
  blocking_tab_helper->OnBlockingPageCommandReceived(command_enum.value());
}

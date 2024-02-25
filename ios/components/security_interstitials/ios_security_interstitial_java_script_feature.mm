// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_security_interstitial_java_script_feature.h"

#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace security_interstitials {

namespace {
// The name of the message handler for messages from the error page. Must be
// kept in sync with components/neterror/resources/error_page_controller_ios.js.
const char kWebUIMessageHandlerName[] = "IOSInterstitialMessage";
}  // namespace

// static
IOSSecurityInterstitialJavaScriptFeature*
IOSSecurityInterstitialJavaScriptFeature::GetInstance() {
  static base::NoDestructor<IOSSecurityInterstitialJavaScriptFeature> instance;
  return instance.get();
}

IOSSecurityInterstitialJavaScriptFeature::
    IOSSecurityInterstitialJavaScriptFeature()
    // This feature must be in the page content world in order to listen for
    // messages from the Error Page JavaScript.
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld, {}) {}

IOSSecurityInterstitialJavaScriptFeature::
    ~IOSSecurityInterstitialJavaScriptFeature() = default;

std::optional<std::string>
IOSSecurityInterstitialJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWebUIMessageHandlerName;
}

void IOSSecurityInterstitialJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  // Interstitial messages are only sent from the main frame.
  if (!script_message.is_main_frame()) {
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

  int command_id;
  if (!base::StringToInt(*command, &command_id)) {
    return;
  }

  IOSBlockingPageTabHelper* blocking_tab_helper =
      IOSBlockingPageTabHelper::FromWebState(web_state);
  if (!blocking_tab_helper) {
    return;
  }
  blocking_tab_helper->OnBlockingPageCommandReceived(
      static_cast<SecurityInterstitialCommand>(command_id));
}

}  // namespace security_interstitials

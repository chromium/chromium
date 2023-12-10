// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/web_ui_messaging_java_script_feature.h"

#import "base/values.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/web_state_impl.h"

namespace {
const char kWebUIMessageHandlerName[] = "WebUIMessage";
}  // namespace

namespace web {

// static
WebUIMessagingJavaScriptFeature*
WebUIMessagingJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebUIMessagingJavaScriptFeature> instance;
  return instance.get();
}

WebUIMessagingJavaScriptFeature::WebUIMessagingJavaScriptFeature()
    // This feature must be in the page content world in order to listen for
    // messages from WebUI JavaScript.
    : JavaScriptFeature(ContentWorld::kPageContentWorld, {}) {}

WebUIMessagingJavaScriptFeature::~WebUIMessagingJavaScriptFeature() = default;

std::optional<std::string>
WebUIMessagingJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWebUIMessageHandlerName;
}

void WebUIMessagingJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  // WebUI messages are only handled if sent from the main frame.
  if (!script_message.is_main_frame()) {
    return;
  }

  std::optional<GURL> url = script_message.request_url();
  // Messages must be from an app specific url.
  if (!url || !web::GetWebClient()->IsAppSpecificURL(url.value())) {
    return;
  }

  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  const base::Value::Dict& dict = script_message.body()->GetDict();

  const std::string* message_content = dict.FindString("message");
  if (!message_content) {
    return;
  }

  const base::Value::List* arguments = dict.FindList("arguments");
  if (!arguments) {
    return;
  }

  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state);
  if (!web_state_impl) {
    return;
  }

  web_state_impl->HandleWebUIMessage(url.value(), *message_content, *arguments);
}

}  // namespace web

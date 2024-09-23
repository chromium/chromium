// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_message_handler_java_script_feature.h"

#import "base/logging.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

const char kWebViewMessageHandlerJavaScriptFeatureKeyName[] =
    "web_view_message_handler_java_script_feature";

const char kScriptName[] = "cwv_messaging";

const char kWebViewMessageHandlerName[] = "CWVWebViewMessage";

const char kScriptMessageCommandKey[] = "command";
const char kScriptMessagePayloadKey[] = "payload";

}  // namespace

// static
WebViewMessageHandlerJavaScriptFeature*
WebViewMessageHandlerJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebViewMessageHandlerJavaScriptFeature> instance;
  return instance.get();
}

WebViewMessageHandlerJavaScriptFeature::WebViewMessageHandlerJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}
WebViewMessageHandlerJavaScriptFeature::
    ~WebViewMessageHandlerJavaScriptFeature() = default;

// static
WebViewMessageHandlerJavaScriptFeature*
WebViewMessageHandlerJavaScriptFeature::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK(browser_state);

  WebViewMessageHandlerJavaScriptFeature* feature =
      static_cast<WebViewMessageHandlerJavaScriptFeature*>(
          browser_state->GetUserData(
              kWebViewMessageHandlerJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new WebViewMessageHandlerJavaScriptFeature();
    browser_state->SetUserData(kWebViewMessageHandlerJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

void WebViewMessageHandlerJavaScriptFeature::RegisterHandler(
    std::string& command,
    WebViewMessageHandlerCallback handler) {
  DCHECK(handlers_.find(command) == handlers_.end());

  handlers_[command] = std::move(handler);
}

void WebViewMessageHandlerJavaScriptFeature::UnregisterHandler(
    std::string& command) {
  DCHECK(handlers_.find(command) != handlers_.end());

  handlers_.erase(command);
}

std::optional<std::string>
WebViewMessageHandlerJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWebViewMessageHandlerName;
}

void WebViewMessageHandlerJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }
  base::Value::Dict message_body = std::move(script_message.body()->GetDict());

  // Pass messages from the non-static instances to the static instance during
  // transition.
  // TODO(crbug.com/40899585): Remove static instance of feature.
  WebViewMessageHandlerJavaScriptFeature* static_instance =
      WebViewMessageHandlerJavaScriptFeature::GetInstance();
  if (this != static_instance) {
    static_instance->NotifyHandlers(message_body);
  }

  NotifyHandlers(message_body);
}

void WebViewMessageHandlerJavaScriptFeature::NotifyHandlers(
    const base::Value::Dict& message_body) {
  const std::string* command =
      message_body.FindString(kScriptMessageCommandKey);
  if (!command) {
    return;
  }

  if (handlers_.find(*command) == handlers_.end()) {
    return;
  }

  const base::Value::Dict* payload =
      message_body.FindDict(kScriptMessagePayloadKey);
  if (!payload) {
    return;
  }

  handlers_[*command].Run(std::move(*payload));
}

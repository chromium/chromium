// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_

#import <map>
#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/supports_user_data.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class BrowserState;
}  // namespace web

// A feature which listens for messages sent from the webpage to the
// 'CWVWebViewMessage' message handler and routes them to the callback for the
// associated command. `command` and `payload` are required top level keys. The
// value of the `command` key must be a string matching a registered callback.
// `payload` key must be a dictionary which will be sent to the callback mapped
// to the value of `command`.
//
// Example call from JavaScript:
//
//  let message = {
//    'command': 'myFeatureMessage',
//    'payload' : {'key1':'value1', 'key2':42}
//  }
//  window.webkit.messageHandlers['CWVWebViewMessage'].postMessage(message);
//
class WebViewMessageHandlerJavaScriptFeature
    : public base::SupportsUserData::Data,
      public web::JavaScriptFeature {
 public:
  WebViewMessageHandlerJavaScriptFeature();
  ~WebViewMessageHandlerJavaScriptFeature() override;

  WebViewMessageHandlerJavaScriptFeature(
      const WebViewMessageHandlerJavaScriptFeature&) = delete;
  WebViewMessageHandlerJavaScriptFeature& operator=(
      const WebViewMessageHandlerJavaScriptFeature&) = delete;

  static WebViewMessageHandlerJavaScriptFeature* GetInstance();

  // Returns the WebViewMessageHandlerJavaScriptFeature associated with
  // `browser_state`, creating one if necessary. `browser_state` must not be
  // null.
  static WebViewMessageHandlerJavaScriptFeature* FromBrowserState(
      web::BrowserState* browser_state);

  using WebViewMessageHandlerCallback =
      base::RepeatingCallback<void(const base::Value::Dict& payload)>;
  void RegisterHandler(std::string& command,
                       WebViewMessageHandlerCallback handler);
  void UnregisterHandler(std::string& command);

 private:
  friend class base::NoDestructor<WebViewMessageHandlerJavaScriptFeature>;

  // JavaScriptFeature overrides
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;

  // Notifies any handler registered in `handlers_` for the command specified in
  // `message_body`.
  void NotifyHandlers(const base::Value::Dict& message_body);

  std::map<std::string, WebViewMessageHandlerCallback> handlers_;
};

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_

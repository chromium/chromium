// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_
#define IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/webui/web_ui_ios.h"

namespace web {
class WebFrame;
}

namespace web {

class WebUIIOSImpl : public web::WebUIIOS,
                     public base::SupportsWeakPtr<WebUIIOSImpl> {
 public:
  explicit WebUIIOSImpl(WebState* web_state);

  WebUIIOSImpl(const WebUIIOSImpl&) = delete;
  WebUIIOSImpl& operator=(const WebUIIOSImpl&) = delete;

  ~WebUIIOSImpl() override;

  // WebUIIOS implementation:
  WebState* GetWebState() const override;
  WebUIIOSController* GetController() const override;
  void SetController(std::unique_ptr<WebUIIOSController> controller) override;
  void AddMessageHandler(
      std::unique_ptr<WebUIIOSMessageHandler> handler) override;
  void RegisterMessageCallback(const std::string& message,
                               MessageCallback callback) override;
  void ProcessWebUIIOSMessage(const GURL& source_url,
                              const std::string& message,
                              const base::Value& args) override;
  void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;
  void ResolveJavascriptCallback(const base::Value& callback_id,
                                 const base::Value& response) override;
  void RejectJavascriptCallback(const base::Value& callback_id,
                                const base::Value& response) override;
  void FireWebUIListener(const std::string& event_name,
                         const std::vector<const base::Value*>& args) override;

 private:
  void OnJsMessage(const base::Value& message,
                   const GURL& page_url,
                   bool user_is_interacting,
                   web::WebFrame* sender_frame);

  // Executes JavaScript asynchronously on the page.
  void ExecuteJavascript(const std::u16string& javascript);

  // A map of message name -> message handling callback.
  using MessageCallbackMap = std::map<std::string, MessageCallback>;
  MessageCallbackMap message_callbacks_;

  // The WebUIIOSMessageHandlers we own.
  std::vector<std::unique_ptr<WebUIIOSMessageHandler>> handlers_;

  // Subscription for JS message.
  base::CallbackListSubscription subscription_;

  // Non-owning pointer to the WebState this WebUIIOS is associated with.
  WebState* web_state_;

  std::unique_ptr<WebUIIOSController> controller_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_

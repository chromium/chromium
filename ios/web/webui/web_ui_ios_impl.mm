// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/web_ui_ios_impl.h"

#include <stddef.h>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebUIIOSController;

namespace {
const char kCommandPrefix[] = "webui";
}

namespace web {

// static
base::string16 WebUIIOS::GetJavascriptCall(
    const std::string& function_name,
    const std::vector<const base::Value*>& arg_list) {
  base::string16 parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += base::char16(',');

    base::JSONWriter::Write(*arg_list[i], &json);
    parameters += base::UTF8ToUTF16(json);
  }
  return base::ASCIIToUTF16(function_name) + base::char16('(') + parameters +
         base::char16(')') + base::char16(';');
}

WebUIIOSImpl::WebUIIOSImpl(WebState* web_state) : web_state_(web_state) {
  DCHECK(web_state);
  subscription_ = web_state->AddScriptCommandCallback(
      base::BindRepeating(&WebUIIOSImpl::OnJsMessage, base::Unretained(this)),
      kCommandPrefix);
}

WebUIIOSImpl::~WebUIIOSImpl() {
  controller_.reset();
}

// WebUIIOSImpl, public:
// ----------------------------------------------------------

WebState* WebUIIOSImpl::GetWebState() const {
  return web_state_;
}

WebUIIOSController* WebUIIOSImpl::GetController() const {
  return controller_.get();
}

void WebUIIOSImpl::SetController(
    std::unique_ptr<WebUIIOSController> controller) {
  controller_ = std::move(controller);
}

void WebUIIOSImpl::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::ResolveJavascriptCallback(const base::Value& callback_id,
                                             const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  base::Value request_successful = base::Value(true);
  std::vector<const base::Value*> args{&callback_id, &request_successful,
                                       &response};
  ExecuteJavascript(GetJavascriptCall("cr.webUIResponse", args));
}

void WebUIIOSImpl::RejectJavascriptCallback(const base::Value& callback_id,
                                            const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  base::Value request_successful = base::Value(false);
  std::vector<const base::Value*> args{&callback_id, &request_successful,
                                       &response};
  ExecuteJavascript(GetJavascriptCall("cr.webUIResponse", args));
}

void WebUIIOSImpl::RegisterMessageCallback(const std::string& message,
                                           const MessageCallback& callback) {
  message_callbacks_.insert(std::make_pair(message, callback));
}

void WebUIIOSImpl::OnJsMessage(const base::DictionaryValue& message,
                               const GURL& page_url,
                               bool user_is_interacting,
                               web::WebFrame* sender_frame) {
  // Chrome message are only handled if sent from the main frame.
  if (!sender_frame->IsMainFrame())
    return;

  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kNone;
  const GURL current_url = web_state_->GetCurrentURL(&trust_level);
  if (web::GetWebClient()->IsAppSpecificURL(current_url)) {
    std::string message_content;
    const base::ListValue* arguments = nullptr;
    if (!message.GetString("message", &message_content)) {
      DLOG(WARNING) << "JS message parameter not found: message";
      return;
    }
    if (!message.GetList("arguments", &arguments)) {
      DLOG(WARNING) << "JS message parameter not found: arguments";
      return;
    }
    ProcessWebUIIOSMessage(current_url, message_content, *arguments);
  }
}

void WebUIIOSImpl::ProcessWebUIIOSMessage(const GURL& source_url,
                                          const std::string& message,
                                          const base::ListValue& args) {
  if (controller_->OverrideHandleWebUIIOSMessage(source_url, message, args))
    return;

  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(message);
  if (callback != message_callbacks_.end()) {
    // Forward this message and content on.
    callback->second.Run(&args);
  }
}

// WebUIIOSImpl, protected:
// -------------------------------------------------------

void WebUIIOSImpl::AddMessageHandler(
    std::unique_ptr<WebUIIOSMessageHandler> handler) {
  DCHECK(!handler->web_ui());
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(std::move(handler));
}

void WebUIIOSImpl::ExecuteJavascript(const base::string16& javascript) {
  web_state_->ExecuteJavaScript(javascript);
}

}  // namespace web

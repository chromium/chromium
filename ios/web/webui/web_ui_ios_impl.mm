// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/web_ui_ios_impl.h"

#import <stddef.h>

#import <string_view>

#import "base/json/json_writer.h"
#import "base/logging.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

using web::WebUIIOSController;

namespace web {

// static
std::u16string WebUIIOS::GetJavascriptCall(
    std::string_view function_name,
    base::span<const base::ValueView> arg_list) {
  std::u16string parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += u',';

    base::JSONWriter::Write(arg_list[i], &json);
    parameters += base::UTF8ToUTF16(json);
  }
  return base::ASCIIToUTF16(function_name) + u'(' + parameters + u");";
}

WebUIIOSImpl::WebUIIOSImpl(WebState* web_state) : web_state_(web_state) {}

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
    std::string_view function_name,
    base::span<const base::ValueView> args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::ResolveJavascriptCallback(const base::ValueView callback_id,
                                             const base::ValueView response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  base::Value request_successful = base::Value(true);
  base::ValueView args[] = {callback_id, request_successful, response};
  ExecuteJavascript(GetJavascriptCall("cr.webUIResponse", args));
}

void WebUIIOSImpl::RejectJavascriptCallback(const base::ValueView callback_id,
                                            const base::ValueView response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  base::Value request_successful = base::Value(false);
  base::ValueView args[] = {callback_id, request_successful, response};
  ExecuteJavascript(GetJavascriptCall("cr.webUIResponse", args));
}

void WebUIIOSImpl::FireWebUIListenerSpan(
    base::span<const base::ValueView> values) {
  ExecuteJavascript(GetJavascriptCall("cr.webUIListenerCallback", values));
}

void WebUIIOSImpl::RegisterMessageCallback(std::string_view message,
                                           MessageCallback callback) {
  message_callbacks_.emplace(message, std::move(callback));
}

void WebUIIOSImpl::ProcessWebUIIOSMessage(const GURL& source_url,
                                          std::string_view message,
                                          const base::Value::List& args) {
  if (controller_->OverrideHandleWebUIIOSMessage(source_url, message))
    return;

  // Look up the callback for this message.
  auto message_callback_it = message_callbacks_.find(message);
  if (message_callback_it != message_callbacks_.end()) {
    // Forward this message and content on.
    message_callback_it->second.Run(args);
    return;
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

void WebUIIOSImpl::ExecuteJavascript(const std::u16string& javascript) {
  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  main_frame->ExecuteJavaScript(javascript);
}

}  // namespace web

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_web_ui.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {

TestWebUI::TestWebUI() = default;

TestWebUI::~TestWebUI() {
  ClearTrackedCalls();
}

void TestWebUI::ClearTrackedCalls() {
  call_data_.clear();
}

void TestWebUI::HandleReceivedMessage(const std::string& handler_name,
                                      const base::ListValue* args) {
  const auto callbacks_map_it = message_callbacks_.find(handler_name);
  if (callbacks_map_it != message_callbacks_.end()) {
    // Create a copy of the callbacks before running them. Without this, it
    // could be possible for the callback's handler to register a new message
    // handler during iteration of the vector, resulting in undefined behavior.
    std::vector<MessageCallback> callbacks_to_run = callbacks_map_it->second;
    for (auto& callback : callbacks_to_run)
      callback.Run(args->GetList());
    return;
  }

  const auto deprecated_callbacks_map_it =
      deprecated_message_callbacks_.find(handler_name);
  if (deprecated_callbacks_map_it != deprecated_message_callbacks_.end()) {
    // Create a copy of the callbacks before running them. Without this, it
    // could be possible for the callback's handler to register a new message
    // handler during iteration of the vector, resulting in undefined behavior.
    std::vector<DeprecatedMessageCallback> callbacks_to_run =
        deprecated_callbacks_map_it->second;
    for (auto& callback : callbacks_to_run)
      callback.Run(args);
  }
}

WebContents* TestWebUI::GetWebContents() {
  return web_contents_;
}

WebUIController* TestWebUI::GetController() {
  return controller_.get();
}

void TestWebUI::SetController(std::unique_ptr<WebUIController> controller) {
  controller_ = std::move(controller);
}

float TestWebUI::GetDeviceScaleFactor() {
  return 1.0f;
}

const std::u16string& TestWebUI::GetOverriddenTitle() {
  return temp_string_;
}

int TestWebUI::GetBindings() {
  return bindings_;
}

void TestWebUI::SetBindings(int bindings) {
  bindings_ = bindings;
}

const std::vector<std::string>& TestWebUI::GetRequestableSchemes() {
  NOTIMPLEMENTED();
  return std::move(std::vector<std::string>());
}

void TestWebUI::AddRequestableScheme(const char* scheme) {
  NOTIMPLEMENTED();
  return;
}

void TestWebUI::AddMessageHandler(
    std::unique_ptr<WebUIMessageHandler> handler) {
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(std::move(handler));
}

void TestWebUI::RegisterMessageCallback(base::StringPiece message,
                                        MessageCallback callback) {
  message_callbacks_[static_cast<std::string>(message)].push_back(
      std::move(callback));
}

void TestWebUI::RegisterDeprecatedMessageCallback(
    base::StringPiece message,
    const DeprecatedMessageCallback& callback) {
  deprecated_message_callbacks_[static_cast<std::string>(message)].push_back(
      callback);
}

bool TestWebUI::CanCallJavascript() {
  return true;
}

void TestWebUI::CallJavascriptFunctionUnsafe(const std::string& function_name) {
  call_data_.push_back(base::WrapUnique(new CallData(function_name)));
  OnJavascriptCall(*call_data_.back());
}

void TestWebUI::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1) {
  call_data_.push_back(base::WrapUnique(new CallData(function_name)));
  call_data_.back()->TakeAsArg1(base::Value::ToUniquePtrValue(arg1.Clone()));
  OnJavascriptCall(*call_data_.back());
}

void TestWebUI::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2) {
  call_data_.push_back(base::WrapUnique(new CallData(function_name)));
  call_data_.back()->TakeAsArg1(base::Value::ToUniquePtrValue(arg1.Clone()));
  call_data_.back()->TakeAsArg2(base::Value::ToUniquePtrValue(arg2.Clone()));
  OnJavascriptCall(*call_data_.back());
}

void TestWebUI::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3) {
  call_data_.push_back(base::WrapUnique(new CallData(function_name)));
  call_data_.back()->TakeAsArg1(base::Value::ToUniquePtrValue(arg1.Clone()));
  call_data_.back()->TakeAsArg2(base::Value::ToUniquePtrValue(arg2.Clone()));
  call_data_.back()->TakeAsArg3(base::Value::ToUniquePtrValue(arg3.Clone()));
  OnJavascriptCall(*call_data_.back());
}

void TestWebUI::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3,
                                             const base::Value& arg4) {
  call_data_.push_back(base::WrapUnique(new CallData(function_name)));
  call_data_.back()->TakeAsArg1(base::Value::ToUniquePtrValue(arg1.Clone()));
  call_data_.back()->TakeAsArg2(base::Value::ToUniquePtrValue(arg2.Clone()));
  call_data_.back()->TakeAsArg3(base::Value::ToUniquePtrValue(arg3.Clone()));
  call_data_.back()->TakeAsArg4(base::Value::ToUniquePtrValue(arg4.Clone()));
  OnJavascriptCall(*call_data_.back());
}

void TestWebUI::CallJavascriptFunctionUnsafe(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  NOTREACHED();
}

void TestWebUI::OnJavascriptCall(const CallData& call_data) {
  for (JavascriptCallObserver& observer : javascript_call_observers_)
    observer.OnJavascriptFunctionCalled(call_data);
}

std::vector<std::unique_ptr<WebUIMessageHandler>>*
TestWebUI::GetHandlersForTesting() {
  return &handlers_;
}

TestWebUI::CallData::CallData(const std::string& function_name)
    : function_name_(function_name) {
}

TestWebUI::CallData::~CallData() {
}

void TestWebUI::CallData::TakeAsArg1(std::unique_ptr<base::Value> arg) {
  arg1_ = std::move(arg);
}

void TestWebUI::CallData::TakeAsArg2(std::unique_ptr<base::Value> arg) {
  arg2_ = std::move(arg);
}

void TestWebUI::CallData::TakeAsArg3(std::unique_ptr<base::Value> arg) {
  arg3_ = std::move(arg);
}

void TestWebUI::CallData::TakeAsArg4(std::unique_ptr<base::Value> arg) {
  arg4_ = std::move(arg);
}

}  // namespace content

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/report_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "v8/include/v8.h"

#include <iostream>

namespace auction_worklet {

Console::Console(AuctionV8Helper* v8_helper) : v8_helper_(v8_helper) {}
Console::~Console() = default;

v8::Local<v8::ObjectTemplate> Console::GetConsoleTemplate() {
  v8::Local<v8::ObjectTemplate> console_template =
      v8::ObjectTemplate::New(v8_helper_->isolate());

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  RegisterConsoleMethod(v8_this, "debug", &Console::ConsoleDebug,
                        console_template);
  RegisterConsoleMethod(v8_this, "error", &Console::ConsoleError,
                        console_template);
  RegisterConsoleMethod(v8_this, "info", &Console::ConsoleInfo,
                        console_template);
  RegisterConsoleMethod(v8_this, "log", &Console::ConsoleLog, console_template);
  RegisterConsoleMethod(v8_this, "warn", &Console::ConsoleWarn,
                        console_template);

  return console_template;
}

void Console::RegisterConsoleMethod(
    v8::Local<v8::External> v8_this,
    const char* name,
    ConsoleFn function,
    v8::Local<v8::ObjectTemplate> console_template) {
  v8::Local<v8::FunctionTemplate> function_obj =
      v8::FunctionTemplate::New(v8_helper_->isolate(), function, v8_this);
  function_obj->RemovePrototype();
  console_template->Set(v8_helper_->CreateStringFromLiteral(name),
                        function_obj);
}

// static
void Console::ConsoleDebug(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Console* console =
      static_cast<Console*>(v8::External::Cast(*args.Data())->Value());
  console->DoConsoleOut("[Debug]", args);
}

// static
void Console::ConsoleError(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Console* console =
      static_cast<Console*>(v8::External::Cast(*args.Data())->Value());
  console->DoConsoleOut("[Error]", args);
}

// static
void Console::ConsoleInfo(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Console* console =
      static_cast<Console*>(v8::External::Cast(*args.Data())->Value());
  console->DoConsoleOut("[Info]", args);
}

// static
void Console::ConsoleLog(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Console* console =
      static_cast<Console*>(v8::External::Cast(*args.Data())->Value());
  console->DoConsoleOut("[Log]", args);
}

// static
void Console::ConsoleWarn(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Console* console =
      static_cast<Console*>(v8::External::Cast(*args.Data())->Value());
  console->DoConsoleOut("[Warn]", args);
}

void Console::DoConsoleOut(const std::string& prefix,
                           const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!v8_helper_->console_buffer())
    return;

  std::string result =
      base::StrCat({v8_helper_->console_script_name(), " ", prefix, ": "});
  for (int i = 0; i < args.Length(); ++i) {
    v8::String::Utf8Value val_utf8(v8_helper_->isolate(), args[i]);
    if (i != 0)
      result += ' ';
    if (*val_utf8)
      result += std::string(*val_utf8, val_utf8.length());
  }

  v8_helper_->console_buffer()->push_back(std::move(result));
}

}  // namespace auction_worklet

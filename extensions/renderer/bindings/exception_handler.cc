// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/exception_handler.h"

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {


struct ExceptionHandlerPerContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] = "extension_exception_handler";

  v8::Global<v8::Function> custom_handler;
};

constexpr char ExceptionHandlerPerContextData::kPerContextDataKey[];

}  // namespace

ExceptionHandler::ExceptionHandler(
    const binding::AddConsoleError& add_console_error)
    : add_console_error_(add_console_error) {}
ExceptionHandler::~ExceptionHandler() {}

void ExceptionHandler::HandleException(v8::Local<v8::Context> context,
                                       const std::string& message,
                                       v8::TryCatch* try_catch) {
  DCHECK(try_catch->HasCaught());

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> message_value;
  {
    v8::TryCatch inner_try_catch(isolate);
    inner_try_catch.SetVerbose(true);
    v8::Local<v8::Value> stack_trace_value;
    if (try_catch->StackTrace(context).ToLocal(&stack_trace_value)) {
      message_value = stack_trace_value;
    } else if (!try_catch->Message().IsEmpty()) {
      message_value = try_catch->Message()->Get();
    }
  }

  std::string full_message =
      !message_value.IsEmpty()
          ? base::StringPrintf("%s: %s", message.c_str(),
                               gin::V8ToString(isolate, message_value).c_str())
          : message;
  HandleException(context, full_message, try_catch->Exception());
  try_catch->Reset();  // Reset() to avoid handling the error more than once.
}

void ExceptionHandler::HandleException(v8::Local<v8::Context> context,
                                       const std::string& full_message,
                                       v8::Local<v8::Value> exception_value) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Function> handler = GetCustomHandler(context);
  if (!handler.IsEmpty()) {
    v8::Local<v8::Value> arguments[] = {
        gin::StringToV8(isolate, full_message), exception_value,
    };
    // Hopefully, handling an exception doesn't throw an exception - but it's
    // possible. Handle this gracefully, and log errors normally.
    v8::TryCatch handler_try_catch(isolate);
    handler_try_catch.SetVerbose(true);
    JSRunner::Get(context)->RunJSFunction(handler, context,
                                          base::size(arguments), arguments);
  } else {
    add_console_error_.Run(context, full_message);
  }
}

void ExceptionHandler::SetHandlerForContext(v8::Local<v8::Context> context,
                                            v8::Local<v8::Function> handler) {
  ExceptionHandlerPerContextData* data =
      GetPerContextData<ExceptionHandlerPerContextData>(context,
                                                        kCreateIfMissing);
  DCHECK(data);
  data->custom_handler.Reset(context->GetIsolate(), handler);
}

v8::Local<v8::Function> ExceptionHandler::GetCustomHandler(
    v8::Local<v8::Context> context) {
  ExceptionHandlerPerContextData* data =
      GetPerContextData<ExceptionHandlerPerContextData>(context,
                                                        kDontCreateIfMissing);
  return data ? data->custom_handler.Get(context->GetIsolate())
              : v8::Local<v8::Function>();
}

}  // namespace extensions

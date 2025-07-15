// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/exception_handler.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/per_context_data.h"
#include "gin/public/wrappable_pointer_tags.h"
#include "gin/wrappable.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-cppgc.h"

namespace extensions {

namespace {

struct ExceptionHandlerPerContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] = "extension_exception_handler";

  v8::Global<v8::Function> custom_handler;
};

constexpr char ExceptionHandlerPerContextData::kPerContextDataKey[];

// A helper class to wrap an ExceptionHandler WeakPtr in a v8::Value.
class WrappedExceptionHandler
    : public gin::Wrappable<WrappedExceptionHandler> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {
      {gin::kEmbedderNativeGin},
      gin::kWrappedExceptionHandler};

  const gin::WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }

  base::WeakPtr<ExceptionHandler> exception_handler;
};

}  // namespace

ExceptionHandler::ExceptionHandler(
    const binding::AddConsoleError& add_console_error)
    : add_console_error_(add_console_error) {}
ExceptionHandler::~ExceptionHandler() = default;

v8::Local<v8::Value> ExceptionHandler::GetV8Wrapper(v8::Isolate* isolate) {
  auto* wrapper = cppgc::MakeGarbageCollected<WrappedExceptionHandler>(
      isolate->GetCppHeap()->GetAllocationHandle());
  wrapper->exception_handler = weak_factory_.GetWeakPtr();
  return wrapper->GetWrapper(isolate).ToLocalChecked();
}

ExceptionHandler* ExceptionHandler::FromV8Wrapper(v8::Isolate* isolate,
                                                  v8::Local<v8::Value> value) {
  WrappedExceptionHandler* handler;
  if (!gin::ConvertFromV8(isolate, value, &handler))
    return nullptr;
  return handler->exception_handler.get();
}

void ExceptionHandler::HandleException(v8::Local<v8::Context> context,
                                       const std::string& message,
                                       v8::TryCatch* try_catch) {
  DCHECK(try_catch->HasCaught());

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
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
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
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
    JSRunner::Get(context)->RunJSFunction(handler, context, arguments);
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
  data->custom_handler.Reset(v8::Isolate::GetCurrent(), handler);
}

void ExceptionHandler::RunExtensionCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> extension_callback,
    v8::LocalVector<v8::Value> callback_arguments,
    const std::string& message) {
  v8::TryCatch try_catch(v8::Isolate::GetCurrent());

  // TODO(devlin): JSRunner::RunJSFunction() isn't guaranteed to run
  // synchronously, so if JS is suspended at this moment, the `try_catch` here
  // is insufficient.
  JSRunner::Get(context)->RunJSFunction(extension_callback, context,
                                        callback_arguments);

  // Since arbitrary JS has ran, the context may have been invalidated. If it
  // was, bail.
  if (!binding::IsContextValid(context))
    return;

  if (try_catch.HasCaught())
    HandleException(context, message, &try_catch);
}

v8::Local<v8::Function> ExceptionHandler::GetCustomHandler(
    v8::Local<v8::Context> context) {
  ExceptionHandlerPerContextData* data =
      GetPerContextData<ExceptionHandlerPerContextData>(context,
                                                        kDontCreateIfMissing);
  return data ? data->custom_handler.Get(v8::Isolate::GetCurrent())
              : v8::Local<v8::Function>();
}

}  // namespace extensions

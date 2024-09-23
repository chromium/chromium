// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_activity_logger.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_util.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

bool g_log_for_testing = false;

ScriptContext* GetContextByV8Context(v8::Local<v8::Context> context) {
  if (worker_thread_util::IsWorkerThread()) {
    return Dispatcher::GetWorkerScriptContextSet()->GetContextByV8Context(
        context);
  }

  return ScriptContextSet::GetContextByV8Context(context);
}

}  // namespace

APIActivityLogger::APIActivityLogger(IPCMessageSender* ipc_sender,
                                     ScriptContext* context)
    : ObjectBackedNativeHandler(context), ipc_sender_(ipc_sender) {}

APIActivityLogger::~APIActivityLogger() = default;

void APIActivityLogger::AddRoutes() {
  RouteHandlerFunction(
      "LogEvent",
      base::BindRepeating(&APIActivityLogger::LogForJS, base::Unretained(this),
                          IPCMessageSender::ActivityLogCallType::EVENT));
  RouteHandlerFunction(
      "LogAPICall",
      base::BindRepeating(&APIActivityLogger::LogForJS, base::Unretained(this),
                          IPCMessageSender::ActivityLogCallType::APICALL));
}

// static
bool APIActivityLogger::IsLoggingEnabled() {
  const Dispatcher* dispatcher = ExtensionsRendererClient::Get()->dispatcher();
  return (dispatcher &&  // dispatcher can be null in unittests.
          dispatcher->activity_logging_enabled()) ||
         g_log_for_testing;
}

// static
void APIActivityLogger::LogAPICall(
    IPCMessageSender* ipc_sender,
    v8::Local<v8::Context> context,
    const std::string& call_name,
    const v8::LocalVector<v8::Value>& arguments) {
  if (!IsLoggingEnabled())
    return;

  ScriptContext* script_context = GetContextByV8Context(context);
  if (!script_context)
    return;

  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  ActivityLogConverterStrategy strategy;
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);

  base::Value::List value_args;
  value_args.reserve(arguments.size());
  // TODO(devlin): This doesn't protect against custom properties, so it might
  // not perfectly reflect the passed arguments.
  for (const auto& arg : arguments) {
    std::unique_ptr<base::Value> converted_arg =
        converter->FromV8Value(arg, context);
    if (!converted_arg)
      converted_arg = std::make_unique<base::Value>();
    value_args.Append(
        base::Value::FromUniquePtrValue(std::move(converted_arg)));
  }

  ipc_sender->SendActivityLogIPC(script_context,
                                 script_context->GetExtensionID(),
                                 IPCMessageSender::ActivityLogCallType::APICALL,
                                 call_name, std::move(value_args),
                                 /*extra=*/std::string());
}

void APIActivityLogger::LogEvent(IPCMessageSender* ipc_sender,
                                 ScriptContext* script_context,
                                 const std::string& event_name,
                                 base::Value::List arguments) {
  if (!IsLoggingEnabled())
    return;

  ipc_sender->SendActivityLogIPC(script_context,
                                 script_context->GetExtensionID(),
                                 IPCMessageSender::ActivityLogCallType::EVENT,
                                 event_name, std::move(arguments),
                                 /*extra=*/std::string());
}

void APIActivityLogger::set_log_for_testing(bool log) {
  g_log_for_testing = log;
}

void APIActivityLogger::LogForJS(
    const IPCMessageSender::ActivityLogCallType call_type,
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_GT(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsArray());

  if (!IsLoggingEnabled())
    return;

  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  ScriptContext* script_context = GetContextByV8Context(context);
  if (!script_context) {
    return;
  }

  std::string extension_id = *v8::String::Utf8Value(isolate, args[0]);
  std::string call_name = *v8::String::Utf8Value(isolate, args[1]);
  std::string extra;
  if (args.Length() == 4) {  // Extras are optional.
    CHECK(args[3]->IsString());
    extra = *v8::String::Utf8Value(isolate, args[3]);
  }

  // Get the array of call arguments.
  base::Value::List arguments;
  v8::Local<v8::Array> arg_array = v8::Local<v8::Array>::Cast(args[2]);
  if (arg_array->Length() > 0) {
    arguments.reserve(arg_array->Length());
    std::unique_ptr<content::V8ValueConverter> converter =
        content::V8ValueConverter::Create();
    ActivityLogConverterStrategy strategy;
    converter->SetFunctionAllowed(true);
    converter->SetStrategy(&strategy);
    for (size_t i = 0; i < arg_array->Length(); ++i) {
      // TODO(crbug.com/40605992): Possibly replace ToLocalChecked here with
      // actual error handling.
      std::unique_ptr<base::Value> converted_arg = converter->FromV8Value(
          arg_array->Get(context, i).ToLocalChecked(), context);
      if (!converted_arg)
        converted_arg = std::make_unique<base::Value>();
      arguments.Append(
          base::Value::FromUniquePtrValue(std::move(converted_arg)));
    }
  }

  ipc_sender_->SendActivityLogIPC(script_context, extension_id, call_type,
                                  call_name, std::move(arguments), extra);
}

}  // namespace extensions

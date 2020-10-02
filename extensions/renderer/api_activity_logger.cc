// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_activity_logger.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {
bool g_log_for_testing = false;
}

APIActivityLogger::APIActivityLogger(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

APIActivityLogger::~APIActivityLogger() {}

void APIActivityLogger::AddRoutes() {
  RouteHandlerFunction("LogEvent",
                       base::BindRepeating(&APIActivityLogger::LogForJS,
                                           base::Unretained(this), EVENT));
  RouteHandlerFunction("LogAPICall",
                       base::BindRepeating(&APIActivityLogger::LogForJS,
                                           base::Unretained(this), APICALL));
}

// static
bool APIActivityLogger::IsLoggingEnabled() {
  const Dispatcher* dispatcher =
      ExtensionsRendererClient::Get()->GetDispatcher();
  return (dispatcher &&  // dispatcher can be null in unittests.
          dispatcher->activity_logging_enabled()) ||
         g_log_for_testing;
}

// static
void APIActivityLogger::LogAPICall(
    v8::Local<v8::Context> context,
    const std::string& call_name,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  if (!IsLoggingEnabled())
    return;

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  ActivityLogConverterStrategy strategy;
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);

  base::Value::ListStorage value_args;
  value_args.reserve(arguments.size());
  // TODO(devlin): This doesn't protect against custom properties, so it might
  // not perfectly reflect the passed arguments.
  for (const auto& arg : arguments) {
    std::unique_ptr<base::Value> converted_arg =
        converter->FromV8Value(arg, context);
    if (!converted_arg)
      converted_arg = std::make_unique<base::Value>();
    value_args.push_back(
        base::Value::FromUniquePtrValue(std::move(converted_arg)));
  }

  LogInternal(APICALL, script_context->GetExtensionID(), call_name,
              std::make_unique<base::ListValue>(std::move(value_args)),
              std::string());
}

void APIActivityLogger::LogEvent(ScriptContext* script_context,
                                 const std::string& event_name,
                                 std::unique_ptr<base::ListValue> arguments) {
  if (!IsLoggingEnabled())
    return;

  LogInternal(EVENT, script_context->GetExtensionID(), event_name,
              std::move(arguments), std::string());
}

void APIActivityLogger::set_log_for_testing(bool log) {
  g_log_for_testing = log;
}

void APIActivityLogger::LogForJS(
    const CallType call_type,
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

  std::string extension_id = *v8::String::Utf8Value(isolate, args[0]);
  std::string call_name = *v8::String::Utf8Value(isolate, args[1]);
  std::string extra;
  if (args.Length() == 4) {  // Extras are optional.
    CHECK(args[3]->IsString());
    extra = *v8::String::Utf8Value(isolate, args[3]);
  }

  // Get the array of call arguments.
  base::Value::ListStorage arguments;
  v8::Local<v8::Array> arg_array = v8::Local<v8::Array>::Cast(args[2]);
  if (arg_array->Length() > 0) {
    arguments.reserve(arg_array->Length());
    std::unique_ptr<content::V8ValueConverter> converter =
        content::V8ValueConverter::Create();
    ActivityLogConverterStrategy strategy;
    converter->SetFunctionAllowed(true);
    converter->SetStrategy(&strategy);
    for (size_t i = 0; i < arg_array->Length(); ++i) {
      // TODO(crbug.com/913942): Possibly replace ToLocalChecked here with
      // actual error handling.
      std::unique_ptr<base::Value> converted_arg = converter->FromV8Value(
          arg_array->Get(context, i).ToLocalChecked(), context);
      if (!converted_arg)
        converted_arg = std::make_unique<base::Value>();
      arguments.push_back(
          base::Value::FromUniquePtrValue(std::move(converted_arg)));
    }
  }

  LogInternal(call_type, extension_id, call_name,
              std::make_unique<base::ListValue>(std::move(arguments)), extra);
}

// static
void APIActivityLogger::LogInternal(const CallType call_type,
                                    const std::string& extension_id,
                                    const std::string& call_name,
                                    std::unique_ptr<base::ListValue> arguments,
                                    const std::string& extra) {
  DCHECK(IsLoggingEnabled());
  ExtensionHostMsg_APIActionOrEvent_Params params;
  params.api_call = call_name;
  params.arguments.Swap(arguments.get());
  params.extra = extra;
  if (call_type == APICALL) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddAPIActionToActivityLog(extension_id, params));
  } else if (call_type == EVENT) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddEventToActivityLog(extension_id, params));
  }
}

}  // namespace extensions

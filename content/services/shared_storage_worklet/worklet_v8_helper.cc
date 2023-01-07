// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/worklet_v8_helper.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace shared_storage_worklet {

namespace {

std::string FormatValue(v8::Isolate* isolate, v8::Local<v8::Value> val) {
  if (val.IsEmpty()) {
    return "\"\"";
  } else {
    v8::String::Utf8Value val_utf8(isolate, val);
    if (*val_utf8 == nullptr)
      return std::string();
    return std::string(*val_utf8, val_utf8.length());
  }
}

std::string FormatExceptionMessage(v8::Local<v8::Context> context,
                                   v8::Local<v8::Message> message) {
  if (message.IsEmpty()) {
    return "Unknown exception.";
  } else {
    v8::Isolate* isolate = message->GetIsolate();
    int line_num;
    return base::StrCat(
        {FormatValue(isolate, message->GetScriptResourceName()),
         !context.IsEmpty() && message->GetLineNumber(context).To(&line_num)
             ? std::string(":") + base::NumberToString(line_num)
             : std::string(),
         " ", FormatValue(isolate, message->Get()), "."});
  }
}

v8::MaybeLocal<v8::String> CreateUtf8String(v8::Isolate* isolate,
                                            base::StringPiece utf8_string) {
  if (!base::IsStringUTF8(utf8_string))
    return v8::MaybeLocal<v8::String>();
  return v8::String::NewFromUtf8(isolate, utf8_string.data(),
                                 v8::NewStringType::kNormal,
                                 utf8_string.length());
}

}  // namespace

WorkletV8Helper::HandleScope::HandleScope(v8::Isolate* isolate)
    : isolate_scope_(isolate), handle_scope_(isolate) {}

WorkletV8Helper::HandleScope::~HandleScope() = default;

// static
v8::MaybeLocal<v8::Value> WorkletV8Helper::InvokeFunction(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    base::span<v8::Local<v8::Value>> args,
    std::string* error_message) {
  v8::Isolate* isolate = context->GetIsolate();

  v8::TryCatch try_catch(isolate);

  v8::MaybeLocal<v8::Value> func_result =
      function->Call(context, context->Global(), args.size(), args.data());

  if (func_result.IsEmpty()) {
    *error_message = FormatExceptionMessage(context, try_catch.Message());
    return v8::MaybeLocal<v8::Value>();
  }

  return func_result;
}

v8::MaybeLocal<v8::Value> WorkletV8Helper::CompileAndRunScript(
    v8::Local<v8::Context> context,
    const std::string& src,
    const GURL& src_url,
    std::string* error_message) {
  v8::Isolate* isolate = context->GetIsolate();

  v8::MaybeLocal<v8::String> src_string = CreateUtf8String(isolate, src);
  if (src_string.IsEmpty()) {
    *error_message = "Invalid script content.";
    return {};
  }

  v8::MaybeLocal<v8::String> origin_string =
      CreateUtf8String(isolate, src_url.spec());
  if (origin_string.IsEmpty()) {
    *error_message = "Invalid script origin.";
    return {};
  }

  // Compile script.
  v8::TryCatch try_catch(isolate);
  v8::ScriptCompiler::Source script_source(
      src_string.ToLocalChecked(),
      v8::ScriptOrigin(isolate, origin_string.ToLocalChecked()));
  v8::MaybeLocal<v8::Script> maybe_compile_result = v8::ScriptCompiler::Compile(
      context, &script_source, v8::ScriptCompiler::kNoCompileOptions,
      v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason);

  v8::Local<v8::Script> compile_result;
  if (!maybe_compile_result.ToLocal(&compile_result)) {
    *error_message = FormatExceptionMessage(context, try_catch.Message());
    return {};
  }

  v8::Local<v8::Value> run_result;
  if (!compile_result->Run(context).ToLocal(&run_result)) {
    *error_message = FormatExceptionMessage(context, try_catch.Message());
    return {};
  }

  return run_result;
}

}  // namespace shared_storage_worklet

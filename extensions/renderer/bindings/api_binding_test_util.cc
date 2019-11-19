// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_test_util.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/converter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Common call function implementation. Calls the given |function| with the
// specified |receiver| and arguments. If the call succeeds (doesn't throw an
// error), populates |out_value| with the returned result. If the call does
// throw, populates |out_error| with the thrown error.
// Returns true if the function runs without throwing an error.
bool RunFunctionImpl(v8::Local<v8::Function> function,
                     v8::Local<v8::Context> context,
                     v8::Local<v8::Value> receiver,
                     int argc,
                     v8::Local<v8::Value> argv[],
                     v8::Local<v8::Value>* out_value,
                     std::string* out_error) {
  v8::TryCatch try_catch(context->GetIsolate());
  v8::MaybeLocal<v8::Value> maybe_result =
      function->Call(context, receiver, argc, argv);
  if (try_catch.HasCaught()) {
    *out_error =
        gin::V8ToString(context->GetIsolate(), try_catch.Message()->Get());
    return false;
  }
  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result)) {
    *out_error = "Could not convert result to v8::Local.";
    return false;
  }
  *out_value = result;
  return true;
}

}  // namespace

std::string ReplaceSingleQuotes(base::StringPiece str) {
  std::string result;
  base::ReplaceChars(str.as_string(), "'", "\"", &result);
  return result;
}

std::unique_ptr<base::Value> ValueFromString(base::StringPiece str) {
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(ReplaceSingleQuotes(str));
  EXPECT_TRUE(value) << str;
  return value;
}

std::unique_ptr<base::ListValue> ListValueFromString(base::StringPiece str) {
  return base::ListValue::From(ValueFromString(str));
}

std::unique_ptr<base::DictionaryValue> DictionaryValueFromString(
    base::StringPiece str) {
  return base::DictionaryValue::From(ValueFromString(str));
}

std::string ValueToString(const base::Value& value) {
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(value, &json));
  return json;
}

std::string V8ToString(v8::Local<v8::Value> value,
                       v8::Local<v8::Context> context) {
  if (value.IsEmpty())
    return "empty";
  if (value->IsNull())
    return "null";
  if (value->IsUndefined())
    return "undefined";
  if (value->IsFunction())
    return "function";
  std::unique_ptr<base::Value> json = V8ToBaseValue(value, context);
  if (!json)
    return "unserializable";
  return ValueToString(*json);
}

v8::Local<v8::Value> V8ValueFromScriptSource(v8::Local<v8::Context> context,
                                             base::StringPiece source) {
  v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(
      context, gin::StringToV8(context->GetIsolate(), source));
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script))
    return v8::Local<v8::Value>();
  return script->Run(context).ToLocalChecked();
}

v8::Local<v8::Function> FunctionFromString(v8::Local<v8::Context> context,
                                           base::StringPiece source) {
  v8::Local<v8::Value> value = V8ValueFromScriptSource(context, source);
  v8::Local<v8::Function> function;
  EXPECT_TRUE(gin::ConvertFromV8(context->GetIsolate(), value, &function));
  return function;
}

std::unique_ptr<base::Value> V8ToBaseValue(v8::Local<v8::Value> value,
                                           v8::Local<v8::Context> context) {
  return content::V8ValueConverter::Create()->FromV8Value(value, context);
}

v8::Local<v8::Value> RunFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> receiver,
                                 int argc,
                                 v8::Local<v8::Value> argv[]) {
  std::string error;
  v8::Local<v8::Value> result;
  EXPECT_TRUE(
      RunFunctionImpl(function, context, receiver, argc, argv, &result, &error))
      << error;
  EXPECT_FALSE(result.IsEmpty());
  return result;
}

v8::Local<v8::Value> RunFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 int argc,
                                 v8::Local<v8::Value> argv[]) {
  return RunFunction(function, context, v8::Undefined(context->GetIsolate()),
                     argc, argv);
}

v8::Local<v8::Value> RunFunctionOnGlobal(v8::Local<v8::Function> function,
                                         v8::Local<v8::Context> context,
                                         int argc,
                                         v8::Local<v8::Value> argv[]) {
  return RunFunction(function, context, context->Global(), argc, argv);
}

void RunFunctionOnGlobalAndIgnoreResult(v8::Local<v8::Function> function,
                                        v8::Local<v8::Context> context,
                                        int argc,
                                        v8::Local<v8::Value> argv[]) {
  RunFunction(function, context, context->Global(), argc, argv);
}

v8::Global<v8::Value> RunFunctionOnGlobalAndReturnHandle(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  return v8::Global<v8::Value>(
      context->GetIsolate(),
      RunFunction(function, context, context->Global(), argc, argv));
}

void RunFunctionAndExpectError(v8::Local<v8::Function> function,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value> receiver,
                               int argc,
                               v8::Local<v8::Value> argv[],
                               const std::string& expected_error) {
  std::string error;
  v8::Local<v8::Value> result;
  EXPECT_FALSE(RunFunctionImpl(function, context, receiver, argc, argv, &result,
                               &error));
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_EQ(expected_error, error);
}

void RunFunctionAndExpectError(v8::Local<v8::Function> function,
                               v8::Local<v8::Context> context,
                               int argc,
                               v8::Local<v8::Value> argv[],
                               const std::string& expected_error) {
  RunFunctionAndExpectError(function, context,
                            v8::Undefined(context->GetIsolate()), argc, argv,
                            expected_error);
}

v8::Local<v8::Value> GetPropertyFromObject(v8::Local<v8::Object> object,
                                           v8::Local<v8::Context> context,
                                           base::StringPiece key) {
  v8::Local<v8::Value> result;
  EXPECT_TRUE(object->Get(context, gin::StringToV8(context->GetIsolate(), key))
                  .ToLocal(&result));
  return result;
}

std::unique_ptr<base::Value> GetBaseValuePropertyFromObject(
    v8::Local<v8::Object> object,
    v8::Local<v8::Context> context,
    base::StringPiece key) {
  return V8ToBaseValue(GetPropertyFromObject(object, context, key), context);
}

std::string GetStringPropertyFromObject(v8::Local<v8::Object> object,
                                        v8::Local<v8::Context> context,
                                        base::StringPiece key) {
  return V8ToString(GetPropertyFromObject(object, context, key), context);
}

}  // namespace extensions

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace extensions {

// Returns a string with all single quotes replaced with double quotes. Useful
// to write JSON strings without needing to escape quotes.
std::string ReplaceSingleQuotes(std::string_view str);

// Returns a base::Value parsed from |str|. Will ADD_FAILURE on error.
base::Value ValueFromString(std::string_view str);

// As above, but returning a Value::List.
base::Value::List ListValueFromString(std::string_view str);

// As above, but returning a Value::Dict.
base::Value::Dict DictValueFromString(std::string_view str);

// Converts the given |value| to a JSON string. EXPECTs the conversion to
// succeed.
std::string ValueToString(const base::ValueView&);

// Converts the given |value| to a string. Returns "empty", "undefined", "null",
// or "function" for unserializable values. Note this differs from
// gin::V8ToString, which only accepts v8::String values.
std::string V8ToString(v8::Local<v8::Value> value,
                       v8::Local<v8::Context> context);

// Returns a v8::Value result from compiling and running |source|, or an empty
// local on failure.
v8::Local<v8::Value> V8ValueFromScriptSource(v8::Local<v8::Context> context,
                                             std::string_view source);

// Returns a v8::Function parsed from the given |source|. EXPECTs the conversion
// to succeed.
v8::Local<v8::Function> FunctionFromString(v8::Local<v8::Context> context,
                                           std::string_view source);

// Converts the given |value| to a base::Value and returns the result.
std::unique_ptr<base::Value> V8ToBaseValue(v8::Local<v8::Value> value,
                                           v8::Local<v8::Context> context);

// Calls the given |function| with the specified |receiver| and arguments, and
// returns the result. EXPECTs no errors to be thrown.
v8::Local<v8::Value> RunFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> receiver,
                                 int argc,
                                 v8::Local<v8::Value> argv[]);

// Like RunFunction(), but uses v8::Undefined for the receiver.
v8::Local<v8::Value> RunFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 int argc,
                                 v8::Local<v8::Value> argv[]);

// Like RunFunction(), but uses the |context|'s Global for the receiver.
v8::Local<v8::Value> RunFunctionOnGlobal(v8::Local<v8::Function> function,
                                         v8::Local<v8::Context> context,
                                         int argc,
                                         v8::Local<v8::Value> argv[]);

// Like RunFunctionOnGlobal(), but doesn't return the result. This is useful
// for binding in places a result isn't expected.
void RunFunctionOnGlobalAndIgnoreResult(v8::Local<v8::Function> function,
                                        v8::Local<v8::Context> context,
                                        int argc,
                                        v8::Local<v8::Value> argv[]);

// Like RunFunctionOnGlobal(), but returns a persistent handle for the result.
v8::Global<v8::Value> RunFunctionOnGlobalAndReturnHandle(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]);

// Calls the given |function| with the specified |receiver| and arguments, but
// EXPECTs the function to throw the |expected_error|.
void RunFunctionAndExpectError(v8::Local<v8::Function> function,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value> receiver,
                               int argc,
                               v8::Local<v8::Value> argv[],
                               const std::string& expected_error);

// Like RunFunctionAndExpectError(), but uses v8::Undefined for the receiver.
void RunFunctionAndExpectError(v8::Local<v8::Function> function,
                               v8::Local<v8::Context> context,
                               int argc,
                               v8::Local<v8::Value> argv[],
                               const std::string& expected_error);

// Returns the property with the given |key| from the |object|. EXPECTs the
// operation not throw an error, but doesn't assume the key is present.
v8::Local<v8::Value> GetPropertyFromObject(v8::Local<v8::Object> object,
                                           v8::Local<v8::Context> context,
                                           std::string_view key);

// As above, but converts the result to a base::Value.
std::unique_ptr<base::Value> GetBaseValuePropertyFromObject(
    v8::Local<v8::Object> object,
    v8::Local<v8::Context> context,
    std::string_view key);

// As above, but returns a JSON-serialized version of the value, or
// "undefined", "null", "function", or "empty".
std::string GetStringPropertyFromObject(v8::Local<v8::Object> object,
                                        v8::Local<v8::Context> context,
                                        std::string_view key);

// A utility to determine if a V8 value is a certain type.
template <typename T>
struct ValueTypeChecker {};

template <>
struct ValueTypeChecker<v8::Function> {
  static bool IsType(v8::Local<v8::Value> value);
};

template <>
struct ValueTypeChecker<v8::Object> {
  static bool IsType(v8::Local<v8::Value> value);
};

template <>
struct ValueTypeChecker<v8::Promise> {
  static bool IsType(v8::Local<v8::Value> value);
};

template <>
struct ValueTypeChecker<v8::Array> {
  static bool IsType(v8::Local<v8::Value> value);
};

// Attempts to convert `value` to the expected type `T`, and populate `out`
// with the result. Returns true on success; adds test failures and returns
// false on error. (We do both so that it fits well into an EXPECT_TRUE() but
// also prints out more detailed failure information).
template <typename T>
bool GetValueAs(v8::Local<v8::Value> value, v8::Local<T>* out) {
  if (value.IsEmpty()) {
    ADD_FAILURE() << "Value is empty.";
    return false;
  }

  if (!ValueTypeChecker<T>::IsType(value)) {
    // TODO(devlin): Move the code to print out the type of a v8::Value from
    // argument_spec.cc into a common place, so we can print out
    // "Failed to convert value. Actual type <some type>".
    ADD_FAILURE() << "Value is incorrect type.";
    return false;
  }

  *out = value.As<T>();
  return true;
}

// Returns true if the given `value` is both non-empty and is the expected
// type `T`.
template <typename T>
bool V8ValueIs(v8::Local<v8::Value> value) {
  return !value.IsEmpty() && ValueTypeChecker<T>::IsType(value);
}

// Like GetPropertyFromObject(), but attempts to convert the value to the
// expected type `T`. Returns true and populates `out` on success; adds
// test failures and returns false on failure.
template <typename T>
bool GetPropertyFromObjectAs(v8::Local<v8::Object> object,
                             v8::Local<v8::Context> context,
                             std::string_view key,
                             v8::Local<T>* out) {
  v8::Local<v8::Value> value = GetPropertyFromObject(object, context, key);
  return GetValueAs(value, out);
}

}  // extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_

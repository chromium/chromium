// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace extensions {

// Returns a string with all single quotes replaced with double quotes. Useful
// to write JSON strings without needing to escape quotes.
std::string ReplaceSingleQuotes(base::StringPiece str);

// Returns a base::Value parsed from |str|. EXPECTs the conversion to succeed.
std::unique_ptr<base::Value> ValueFromString(base::StringPiece str);

// As above, but returning a ListValue.
std::unique_ptr<base::ListValue> ListValueFromString(base::StringPiece str);

// As above, but returning a DictionaryValue.
std::unique_ptr<base::DictionaryValue> DictionaryValueFromString(
    base::StringPiece str);

// Converts the given |value| to a JSON string. EXPECTs the conversion to
// succeed.
std::string ValueToString(const base::Value& value);

// Converts the given |value| to a string. Returns "empty", "undefined", "null",
// or "function" for unserializable values. Note this differs from
// gin::V8ToString, which only accepts v8::String values.
std::string V8ToString(v8::Local<v8::Value> value,
                       v8::Local<v8::Context> context);

// Returns a v8::Value result from compiling and running |source|, or an empty
// local on failure.
v8::Local<v8::Value> V8ValueFromScriptSource(v8::Local<v8::Context> context,
                                             base::StringPiece source);

// Returns a v8::Function parsed from the given |source|. EXPECTs the conversion
// to succeed.
v8::Local<v8::Function> FunctionFromString(v8::Local<v8::Context> context,
                                           base::StringPiece source);

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
                                           base::StringPiece key);

// As above, but converts the result to a base::Value.
std::unique_ptr<base::Value> GetBaseValuePropertyFromObject(
    v8::Local<v8::Object> object,
    v8::Local<v8::Context> context,
    base::StringPiece key);

// As above, but returns a JSON-serialized version of the value, or
// "undefined", "null", "function", or "empty".
std::string GetStringPropertyFromObject(v8::Local<v8::Object> object,
                                        v8::Local<v8::Context> context,
                                        base::StringPiece key);

}  // extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_UTIL_H_

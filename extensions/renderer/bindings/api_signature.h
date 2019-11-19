// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8.h"

namespace base {
class Value;
class ListValue;
}

namespace extensions {
class APITypeReferenceMap;
class ArgumentSpec;

// A representation of the expected signature for an API method, along with the
// ability to match provided arguments and convert them to base::Values.
class APISignature {
 public:
  explicit APISignature(const base::ListValue& specification);
  explicit APISignature(std::vector<std::unique_ptr<ArgumentSpec>> signature);
  ~APISignature();

  struct V8ParseResult {
    // Appease the Chromium style plugin (out of line ctor/dtor).
    V8ParseResult();
    ~V8ParseResult();
    V8ParseResult(V8ParseResult&& other);
    V8ParseResult& operator=(V8ParseResult&& other);

    bool succeeded() const { return arguments.has_value(); }

    // The parsed v8 arguments. These may differ from the original v8 arguments
    // since it will include null-filled optional arguments. Populated if
    // parsing was successful. Note that the callback, if any, is included in
    // this list.
    base::Optional<std::vector<v8::Local<v8::Value>>> arguments;

    // Whether the asynchronous response is handled by a callback or a promise.
    binding::AsyncResponseType async_type = binding::AsyncResponseType::kNone;

    // The parse error, if parsing failed.
    base::Optional<std::string> error;
  };

  struct JSONParseResult {
    // Appease the Chromium style plugin (out of line ctor/dtor).
    JSONParseResult();
    ~JSONParseResult();
    JSONParseResult(JSONParseResult&& other);
    JSONParseResult& operator=(JSONParseResult&& other);

    bool succeeded() const { return !!arguments; }

    // The parsed JSON arguments, with null-filled optional arguments filled in.
    // Populated if parsing was successful. Does not include the callback (if
    // any).
    std::unique_ptr<base::ListValue> arguments;

    // The callback, if one was provided.
    v8::Local<v8::Function> callback;

    // Whether the asynchronous response is handled by a callback or a promise.
    binding::AsyncResponseType async_type = binding::AsyncResponseType::kNone;

    // The parse error, if parsing failed.
    base::Optional<std::string> error;
  };

  // Parses |arguments| against this signature, returning the result and
  // performing no argument conversion.
  V8ParseResult ParseArgumentsToV8(
      v8::Local<v8::Context> context,
      const std::vector<v8::Local<v8::Value>>& arguments,
      const APITypeReferenceMap& type_refs) const;

  // Parses |arguments| against this signature, returning the result after
  // converting to base::Values.
  JSONParseResult ParseArgumentsToJSON(
      v8::Local<v8::Context> context,
      const std::vector<v8::Local<v8::Value>>& arguments,
      const APITypeReferenceMap& type_refs) const;

  // Converts |arguments| to base::Values, ignoring the defined signature.
  // This is used when custom bindings modify the passed arguments to a form
  // that doesn't match the documented signature. Since we ignore the schema,
  // this parsing will never fail.
  JSONParseResult ConvertArgumentsIgnoringSchema(
      v8::Local<v8::Context> context,
      const std::vector<v8::Local<v8::Value>>& arguments) const;

  // Validates the provided |arguments| as if they were returned as a response
  // to an API call. This validation is much stricter than the versions above,
  // since response arguments are not allowed to have optional inner parameters.
  bool ValidateResponse(v8::Local<v8::Context> context,
                        const std::vector<v8::Local<v8::Value>>& arguments,
                        const APITypeReferenceMap& type_refs,
                        std::string* error) const;

  // Returns a developer-readable string of the expected signature. For
  // instance, if this signature expects a string 'someStr' and an optional int
  // 'someInt', this would return "string someStr, optional integer someInt".
  std::string GetExpectedSignature() const;

  void set_promise_support(binding::PromiseSupport promise_support) {
    promise_support_ = promise_support;
  }

  bool has_callback() const { return has_callback_; }

 private:
  // The list of expected arguments.
  std::vector<std::unique_ptr<ArgumentSpec>> signature_;

  binding::PromiseSupport promise_support_ =
      binding::PromiseSupport::kDisallowed;

  bool has_callback_ = false;

  // A developer-readable signature string, lazily set.
  mutable std::string expected_signature_;

  DISALLOW_COPY_AND_ASSIGN(APISignature);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_

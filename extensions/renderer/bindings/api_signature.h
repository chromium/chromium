// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "v8/include/v8.h"

namespace extensions {
class APITypeReferenceMap;
class ArgumentSpec;
class BindingAccessChecker;

// Whether promises are allowed to be used for a given call to an API.
enum class PromisesAllowed {
  kAllowed,
  kDisallowed,
};

// A representation of the expected signature for an API, along with the
// ability to match provided arguments and convert them to base::Values.
// This is primarily used for API methods, but can also be used for API event
// signatures.
class APISignature {
 public:
  // Struct that bundles all the details about an asynchronous return.
  struct ReturnsAsync {
    ReturnsAsync();
    ~ReturnsAsync();

    // The list of expected arguments for the asynchronous return. Can be
    // nullopt if response validation isn't enabled, as it is only used when
    // validating a response from the API.
    std::optional<std::vector<std::unique_ptr<ArgumentSpec>>> signature;
    // Indicates if passing the callback when calling the API is optional for
    // contexts or APIs which do not support promises (passing the callback is
    // always inheriently optional if promises are supported).
    bool optional = false;
    // Indicates if this API supports allowing promises for the asynchronous
    // return. Note that this is distinct from whether an actual call to the API
    // is allowed to use the promise based form, as that also depends on if the
    // calling context being checked by the access_checker.
    binding::APIPromiseSupport promise_support =
        binding::APIPromiseSupport::kUnsupported;
  };

  APISignature(std::vector<std::unique_ptr<ArgumentSpec>> signature,
               std::unique_ptr<APISignature::ReturnsAsync> returns_async,
               BindingAccessChecker* access_checker);

  APISignature(const APISignature&) = delete;
  APISignature& operator=(const APISignature&) = delete;

  ~APISignature();

  // Creates an APISignature object from the raw Value representations of an
  // API schema.
  static std::unique_ptr<APISignature> CreateFromValues(
      const base::Value& specification_list,
      const base::Value* returns_async,
      BindingAccessChecker* access_checker);

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
    std::optional<v8::LocalVector<v8::Value>> arguments;

    // Whether the asynchronous response is handled by a callback or a promise.
    binding::AsyncResponseType async_type = binding::AsyncResponseType::kNone;

    // The parse error, if parsing failed.
    std::optional<std::string> error;
  };

  struct JSONParseResult {
    // Appease the Chromium style plugin (out of line ctor/dtor).
    JSONParseResult();
    ~JSONParseResult();
    JSONParseResult(JSONParseResult&& other);
    JSONParseResult& operator=(JSONParseResult&& other);

    bool succeeded() const { return arguments_list.has_value(); }

    // The parsed JSON arguments, with null-filled optional arguments filled in.
    // Populated if parsing was successful. Does not include the callback (if
    // any).
    std::optional<base::Value::List> arguments_list;

    // The callback, if one was provided.
    v8::Local<v8::Function> callback;

    // Whether the asynchronous response is handled by a callback or a promise.
    binding::AsyncResponseType async_type = binding::AsyncResponseType::kNone;

    // The parse error, if parsing failed.
    std::optional<std::string> error;
  };

  // Parses |arguments| against this signature, returning the result and
  // performing no argument conversion.
  V8ParseResult ParseArgumentsToV8(v8::Local<v8::Context> context,
                                   const v8::LocalVector<v8::Value>& arguments,
                                   const APITypeReferenceMap& type_refs) const;

  // Parses |arguments| against this signature, returning the result after
  // converting to base::Values.
  JSONParseResult ParseArgumentsToJSON(
      v8::Local<v8::Context> context,
      const v8::LocalVector<v8::Value>& arguments,
      const APITypeReferenceMap& type_refs) const;

  // Converts |arguments| to base::Values, ignoring the defined signature.
  // This is used when custom bindings modify the passed arguments to a form
  // that doesn't match the documented signature. Since we ignore the schema,
  // this parsing will never fail.
  JSONParseResult ConvertArgumentsIgnoringSchema(
      v8::Local<v8::Context> context,
      const v8::LocalVector<v8::Value>& arguments) const;

  // Validates the provided |arguments| as if they were returned as a response
  // to an API call. This validation is much stricter than the versions above,
  // since response arguments are not allowed to have optional inner parameters.
  bool ValidateResponse(v8::Local<v8::Context> context,
                        const v8::LocalVector<v8::Value>& arguments,
                        const APITypeReferenceMap& type_refs,
                        std::string* error) const;

  // Same as `ValidateResponse`, but verifies the given `arguments` against the
  // `signature_` instead of the `returns_async_` types. This can be used when
  // validating that APIs return proper values to an event (which has a
  // signature, but no return).
  bool ValidateCall(v8::Local<v8::Context> context,
                    const v8::LocalVector<v8::Value>& arguments,
                    const APITypeReferenceMap& type_refs,
                    std::string* error) const;

  // Returns a developer-readable string of the expected signature. For
  // instance, if this signature expects a string 'someStr' and an optional int
  // 'someInt', this would return "string someStr, optional integer someInt".
  std::string GetExpectedSignature() const;

  bool has_async_return() const { return returns_async_ != nullptr; }
  bool has_async_return_signature() const {
    return has_async_return() && returns_async_->signature.has_value();
  }

 private:
  // Checks if promises are allowed to be used for a call to an API from a given
  // |context|.
  PromisesAllowed CheckPromisesAllowed(v8::Local<v8::Context> context) const;

  // The list of expected arguments for the API signature.
  std::vector<std::unique_ptr<ArgumentSpec>> signature_;

  // The details of any asynchronous return an API method may have. This will be
  // nullptr if the the API doesn't have an asynchronous return.
  std::unique_ptr<APISignature::ReturnsAsync> returns_async_;

  // The associated access checker; required to outlive this object.
  raw_ptr<const BindingAccessChecker, DanglingUntriaged> access_checker_;

  // A developer-readable method signature string, lazily set.
  mutable std::string expected_signature_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_SIGNATURE_H_

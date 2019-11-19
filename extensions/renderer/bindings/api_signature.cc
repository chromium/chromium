// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_signature.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/argument_spec.h"
#include "gin/arguments.h"

namespace extensions {

namespace {

bool HasCallback(const std::vector<std::unique_ptr<ArgumentSpec>>& signature) {
  // TODO(devlin): This is how extension APIs have always determined if a
  // function has a callback, but it seems a little silly. In the long run (once
  // signatures are generated), it probably makes sense to indicate this
  // differently.
  return !signature.empty() &&
         signature.back()->type() == ArgumentType::FUNCTION;
}

// A class to help with argument parsing. Note that this uses v8::Locals and
// const&s because it's an implementation detail of the APISignature; this
// should *only* be used directly on the stack!
class ArgumentParser {
 public:
  ArgumentParser(v8::Local<v8::Context> context,
                 const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
                 const std::vector<v8::Local<v8::Value>>& arguments,
                 const APITypeReferenceMap& type_refs,
                 binding::PromiseSupport promise_support)
      : context_(context),
        signature_(signature),
        provided_arguments_(arguments),
        type_refs_(type_refs),
        promise_support_(promise_support) {}

 protected:
  v8::Isolate* GetIsolate() { return context_->GetIsolate(); }

  // Common implementation for parsing arguments to either V8 values or
  // base::Values.
  bool ParseArgumentsImpl(bool signature_has_callback);

  std::string TakeError() { return std::move(error_); }
  binding::AsyncResponseType async_type() const { return async_type_; }

 private:
  // API methods can have multiple possible signatures. For instance, an API
  // method that takes (optional int, string) could be invoked with either
  // an int and string, or just a string. ResolveArguments() takes the
  // |provided| arguments and the |expected| signature, and populates |result|
  // with a normalized array of values such that each entry in |result| is
  // positionally correct with the signature. Omitted arguments will be
  // empty v8::Local<v8::Value> handles in the array.
  // |allow_omitted_final_argument| indicates that the final argument is allowed
  // to be omitted, even if it is not flagged as optional. This is used to allow
  // callers to omit the final "callback" argument if promises can be used
  // instead.
  // Returns true if the arguments were successfully resolved.
  // Note: This only checks arguments against their basic types, not other
  // values (like specific required properties or values).
  bool ResolveArguments(
      base::span<const v8::Local<v8::Value>> provided,
      base::span<const std::unique_ptr<ArgumentSpec>> expected,
      std::vector<v8::Local<v8::Value>>* result,
      size_t index,
      bool allow_omitted_final_argument);

  // Attempts to match the next argument to the given |spec|.
  // If the next argument does not match and |spec| is optional, uses a null
  // value.
  // Returns true on success.
  bool ParseArgument(const ArgumentSpec& spec, v8::Local<v8::Value> value);

  // Attempts to parse the callback from the given |spec|. Returns true on
  // success.
  bool ParseCallback(const ArgumentSpec& spec, v8::Local<v8::Value> value);

  // Adds a null value to the parsed arguments.
  virtual void AddNull() = 0;
  virtual void AddNullCallback() = 0;
  // Returns a base::Value to be populated during argument matching.
  virtual std::unique_ptr<base::Value>* GetBaseBuffer() = 0;
  // Returns a v8::Value to be populated during argument matching.
  virtual v8::Local<v8::Value>* GetV8Buffer() = 0;
  // Adds the argument parsed into the appropriate buffer.
  virtual void AddParsedArgument() = 0;
  // Adds the parsed callback.
  virtual void SetCallback(v8::Local<v8::Function> callback) = 0;

  v8::Local<v8::Context> context_;
  const std::vector<std::unique_ptr<ArgumentSpec>>& signature_;
  const std::vector<v8::Local<v8::Value>>& provided_arguments_;
  const APITypeReferenceMap& type_refs_;
  binding::PromiseSupport promise_support_;
  binding::AsyncResponseType async_type_ = binding::AsyncResponseType::kNone;
  std::string error_;

  // An error to pass while parsing arguments to avoid having to allocate a new
  // std::string on the stack multiple times.
  std::string parse_error_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentParser);
};

class V8ArgumentParser : public ArgumentParser {
 public:
  V8ArgumentParser(v8::Local<v8::Context> context,
                   const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
                   const std::vector<v8::Local<v8::Value>>& arguments,
                   const APITypeReferenceMap& type_refs,
                   binding::PromiseSupport promise_support)
      : ArgumentParser(context,
                       signature,
                       arguments,
                       type_refs,
                       promise_support) {}

  APISignature::V8ParseResult ParseArguments(bool signature_has_callback);

 private:
  void AddNull() override { values_.push_back(v8::Null(GetIsolate())); }
  void AddNullCallback() override { values_.push_back(v8::Null(GetIsolate())); }
  std::unique_ptr<base::Value>* GetBaseBuffer() override { return nullptr; }
  v8::Local<v8::Value>* GetV8Buffer() override { return &last_arg_; }
  void AddParsedArgument() override {
    DCHECK(!last_arg_.IsEmpty());
    values_.push_back(last_arg_);
    last_arg_.Clear();
  }
  void SetCallback(v8::Local<v8::Function> callback) override {
    values_.push_back(callback);
  }

  v8::Local<v8::Value> last_arg_;
  std::vector<v8::Local<v8::Value>> values_;

  DISALLOW_COPY_AND_ASSIGN(V8ArgumentParser);
};

class BaseValueArgumentParser : public ArgumentParser {
 public:
  BaseValueArgumentParser(
      v8::Local<v8::Context> context,
      const std::vector<std::unique_ptr<ArgumentSpec>>& signature,
      const std::vector<v8::Local<v8::Value>>& arguments,
      const APITypeReferenceMap& type_refs,
      binding::PromiseSupport promise_support)
      : ArgumentParser(context,
                       signature,
                       arguments,
                       type_refs,
                       promise_support),
        list_value_(std::make_unique<base::ListValue>()) {}

  APISignature::JSONParseResult ParseArguments(bool signature_has_callback);

 private:
  void AddNull() override {
    list_value_->Append(std::make_unique<base::Value>());
  }
  void AddNullCallback() override {
    // The base::Value conversion doesn't include the callback directly, so we
    // don't add a null parameter here.
  }
  std::unique_ptr<base::Value>* GetBaseBuffer() override { return &last_arg_; }
  v8::Local<v8::Value>* GetV8Buffer() override { return nullptr; }
  void AddParsedArgument() override {
    // The corresponding base::Value is expected to have been stored in
    // |last_arg_| already.
    DCHECK(last_arg_);
    list_value_->Append(std::move(last_arg_));
    last_arg_.reset();
  }
  void SetCallback(v8::Local<v8::Function> callback) override {
    callback_ = callback;
  }

  std::unique_ptr<base::ListValue> list_value_;
  std::unique_ptr<base::Value> last_arg_;
  v8::Local<v8::Function> callback_;

  DISALLOW_COPY_AND_ASSIGN(BaseValueArgumentParser);
};

bool ArgumentParser::ParseArgumentsImpl(bool signature_has_callback) {
  if (provided_arguments_.size() > signature_.size()) {
    error_ = api_errors::NoMatchingSignature();
    return false;
  }

  // We allow the final argument to be omitted if the signature expects a
  // callback and promise-based APIs are supported. If the caller omits this
  // callback, the invocation is assumed to expect to a promise.
  bool allow_omitted_final_argument =
      signature_has_callback &&
      promise_support_ == binding::PromiseSupport::kAllowed;

  std::vector<v8::Local<v8::Value>> resolved_arguments(signature_.size());
  if (!ResolveArguments(provided_arguments_, signature_, &resolved_arguments,
                        0u, allow_omitted_final_argument)) {
    error_ = api_errors::NoMatchingSignature();
    return false;
  }
  DCHECK_EQ(resolved_arguments.size(), signature_.size());

  size_t end_size =
      signature_has_callback ? signature_.size() - 1 : signature_.size();
  for (size_t i = 0; i < end_size; ++i) {
    if (!ParseArgument(*signature_[i], resolved_arguments[i]))
      return false;
  }

  if (signature_has_callback &&
      !ParseCallback(*signature_.back(), resolved_arguments.back())) {
    return false;
  }

  return true;
}

bool ArgumentParser::ResolveArguments(
    base::span<const v8::Local<v8::Value>> provided,
    base::span<const std::unique_ptr<ArgumentSpec>> expected,
    std::vector<v8::Local<v8::Value>>* result,
    size_t index,
    bool allow_omitted_final_argument) {
  // If the provided arguments and expected arguments are both empty, it means
  // we've successfully matched all provided arguments to the expected
  // signature.
  if (provided.empty() && expected.empty())
    return true;

  // If there are more provided arguments than expected arguments, there's no
  // possible signature that could match.
  if (provided.size() > expected.size())
    return false;

  DCHECK(!expected.empty());

  // If there are more provided arguments (and more expected arguments, as
  // guaranteed above), check if the next argument could match the next expected
  // argument.
  if (!provided.empty()) {
    // The argument could potentially match if it is either null or undefined
    // and an optional argument, or if it's the correct expected type.
    bool can_match = false;
    if (expected[0]->optional() && provided[0]->IsNullOrUndefined()) {
      can_match = true;
      // For null/undefined, just use an empty handle. It'll be normalized to
      // null in ParseArgument().
      (*result)[index] = v8::Local<v8::Value>();
    } else if (expected[0]->IsCorrectType(provided[0], type_refs_, &error_)) {
      can_match = true;
      (*result)[index] = provided[0];
    }

    // If the provided argument could potentially match the next expected
    // argument, assume it does, and try to match the remaining arguments.
    // This recursion is safe because it's bounded by the number of arguments
    // present in the signature. Additionally, though this is 2^n complexity,
    // <n> is bounded by the number of expected arguments, which is almost
    // always small. Further, it is only when parameters are optional, which is
    // also not the default.
    if (can_match &&
        ResolveArguments(provided.subspan(1), expected.subspan(1), result,
                         index + 1, allow_omitted_final_argument)) {
      return true;
    }
  }

  // One of three cases happened:
  // - There are no more provided arguments.
  // - The next provided argument could not match the expected argument.
  // - The next provided argument could match the expected argument, but
  //   subsequent arguments did not.
  // In all of these cases, if the expected argument was optional, assume it
  // was omitted, and try matching subsequent arguments.
  if (expected[0]->optional()) {
    // Assume the expected argument was omitted.
    (*result)[index] = v8::Local<v8::Value>();
    // See comments above for recursion notes.
    if (ResolveArguments(provided, expected.subspan(1), result, index + 1,
                         allow_omitted_final_argument))
      return true;
  }

  // A required argument was not matched. There is only one case in which this
  // is allowed: a required callback when Promises are supported instead; if
  // this is the case, |allow_omitted_final_argument| is true.
  if (allow_omitted_final_argument && expected.size() == 1) {
    (*result)[index] = v8::Local<v8::Value>();
    return true;
  }

  return false;
}

bool ArgumentParser::ParseArgument(const ArgumentSpec& spec,
                                   v8::Local<v8::Value> value) {
  if (value.IsEmpty()) {
    // ResolveArguments() should only allow empty values for optional arguments.
    DCHECK(spec.optional());
    AddNull();
    return true;
  }

  // ResolveArguments() should verify that all arguments are at least the
  // correct type.
  DCHECK(spec.IsCorrectType(value, type_refs_, &error_));
  if (!spec.ParseArgument(context_, value, type_refs_, GetBaseBuffer(),
                          GetV8Buffer(), &parse_error_)) {
    error_ = api_errors::ArgumentError(spec.name(), parse_error_);
    return false;
  }

  AddParsedArgument();
  return true;
}

bool ArgumentParser::ParseCallback(const ArgumentSpec& spec,
                                   v8::Local<v8::Value> value) {
  if (value.IsEmpty()) {
    if (promise_support_ == binding::PromiseSupport::kAllowed) {
      // If the callback is omitted and promises are supported, assume the
      // async response type is a promise.
      async_type_ = binding::AsyncResponseType::kPromise;
    } else {
      // Otherwise, we should only get to this point if the callback argument is
      // optional.
      DCHECK(spec.optional());
      AddNullCallback();
      async_type_ = binding::AsyncResponseType::kNone;
    }
    return true;
  }

  // Note: callbacks are set through SetCallback() rather than through the
  // buffered argument.
  if (!spec.ParseArgument(context_, value, type_refs_, nullptr, nullptr,
                          &parse_error_)) {
    error_ = api_errors::ArgumentError(spec.name(), parse_error_);
    return false;
  }

  SetCallback(value.As<v8::Function>());
  async_type_ = binding::AsyncResponseType::kCallback;
  return true;
}

APISignature::V8ParseResult V8ArgumentParser::ParseArguments(
    bool signature_has_callback) {
  APISignature::V8ParseResult result;
  if (!ParseArgumentsImpl(signature_has_callback)) {
    result.error = TakeError();
  } else {
    result.arguments = std::move(values_);
    result.async_type = async_type();
  }

  return result;
}

APISignature::JSONParseResult BaseValueArgumentParser::ParseArguments(
    bool signature_has_callback) {
  APISignature::JSONParseResult result;
  if (!ParseArgumentsImpl(signature_has_callback)) {
    result.error = TakeError();
  } else {
    result.arguments = std::move(list_value_);
    result.callback = callback_;
    result.async_type = async_type();
  }
  return result;
}

}  // namespace

APISignature::V8ParseResult::V8ParseResult() = default;
APISignature::V8ParseResult::~V8ParseResult() = default;
APISignature::V8ParseResult::V8ParseResult(V8ParseResult&& other) = default;
APISignature::V8ParseResult& APISignature::V8ParseResult::operator=(
    V8ParseResult&& other) = default;

APISignature::JSONParseResult::JSONParseResult() = default;
APISignature::JSONParseResult::~JSONParseResult() = default;
APISignature::JSONParseResult::JSONParseResult(JSONParseResult&& other) =
    default;
APISignature::JSONParseResult& APISignature::JSONParseResult::operator=(
    JSONParseResult&& other) = default;

APISignature::APISignature(const base::ListValue& specification) {
  signature_.reserve(specification.GetSize());
  for (const auto& value : specification) {
    const base::DictionaryValue* param = nullptr;
    CHECK(value.GetAsDictionary(&param));
    signature_.push_back(std::make_unique<ArgumentSpec>(*param));
  }

  has_callback_ = HasCallback(signature_);
}

APISignature::APISignature(std::vector<std::unique_ptr<ArgumentSpec>> signature)
    : signature_(std::move(signature)),
      has_callback_(HasCallback(signature_)) {}

APISignature::~APISignature() {}

APISignature::V8ParseResult APISignature::ParseArgumentsToV8(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const APITypeReferenceMap& type_refs) const {
  return V8ArgumentParser(context, signature_, arguments, type_refs,
                          promise_support_)
      .ParseArguments(has_callback_);
}

APISignature::JSONParseResult APISignature::ParseArgumentsToJSON(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const APITypeReferenceMap& type_refs) const {
  return BaseValueArgumentParser(context, signature_, arguments, type_refs,
                                 promise_support_)
      .ParseArguments(has_callback_);
}

APISignature::JSONParseResult APISignature::ConvertArgumentsIgnoringSchema(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments) const {
  // We don't currently handle promises in parsing signatures while ignoring
  // the schema.
  DCHECK_EQ(binding::PromiseSupport::kDisallowed, promise_support_);

  size_t size = arguments.size();
  v8::Local<v8::Function> callback;
  // TODO(devlin): This is what the current bindings do, but it's quite terribly
  // incorrect. We only hit this flow when an API method has a hook to update
  // the arguments post-validation, and in some cases, the arguments returned by
  // that hook do *not* match the signature of the API method (e.g.
  // fileSystem.getDisplayPath); see also note in api_bindings.cc for why this
  // is bad. But then here, we *rely* on the signature to determine whether or
  // not the last parameter is a callback, even though the hooks may not return
  // the arguments in the signature. This is very broken.
  if (HasCallback(signature_)) {
    CHECK(!arguments.empty());
    v8::Local<v8::Value> value = arguments.back();
    --size;
    // Bindings should ensure that the value here is appropriate, but see the
    // comment above for limitations.
    DCHECK(value->IsFunction() || value->IsUndefined() || value->IsNull());
    if (value->IsFunction())
      callback = value.As<v8::Function>();
  }

  auto json = std::make_unique<base::ListValue>();
  json->Reserve(size);

  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  converter->SetFunctionAllowed(true);
  converter->SetConvertNegativeZeroToInt(true);
  converter->SetStripNullFromObjects(true);

  for (size_t i = 0; i < size; ++i) {
    std::unique_ptr<base::Value> converted =
        converter->FromV8Value(arguments[i], context);
    if (!converted) {
      // JSON.stringify inserts non-serializable values as "null" when
      // handling arrays, and this behavior is emulated in V8ValueConverter for
      // array values. Since JS bindings parsed arguments from a single array,
      // it accepted unserializable argument entries (which were converted to
      // null). Duplicate that behavior here.
      converted = std::make_unique<base::Value>();
    }
    json->Append(std::move(converted));
  }

  JSONParseResult result;
  result.arguments = std::move(json);
  result.callback = callback;
  result.async_type = callback.IsEmpty()
                          ? binding::AsyncResponseType::kNone
                          : binding::AsyncResponseType::kCallback;
  return result;
}

bool APISignature::ValidateResponse(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const APITypeReferenceMap& type_refs,
    std::string* error) const {
  size_t expected_size = signature_.size();
  size_t actual_size = arguments.size();
  if (actual_size > expected_size) {
    *error = api_errors::TooManyArguments();
    return false;
  }

  // Easy validation: arguments go in order, and must match the expected schema.
  // Anything less is failure.
  std::string parse_error;
  for (size_t i = 0; i < actual_size; ++i) {
    DCHECK(!arguments[i].IsEmpty());
    const ArgumentSpec& spec = *signature_[i];
    if (arguments[i]->IsNullOrUndefined()) {
      if (!spec.optional()) {
        *error = api_errors::MissingRequiredArgument(spec.name().c_str());
        return false;
      }
      continue;
    }

    if (!spec.ParseArgument(context, arguments[i], type_refs, nullptr, nullptr,
                            &parse_error)) {
      *error = api_errors::ArgumentError(spec.name(), parse_error);
      return false;
    }
  }

  // Responses may omit trailing optional parameters (which would then be
  // undefined for the caller).
  // NOTE(devlin): It might be nice to see if we could require all arguments to
  // be present, no matter what. For one, it avoids this loop, and it would also
  // unify what a "not found" value was (some APIs use undefined, some use
  // null).
  for (size_t i = actual_size; i < expected_size; ++i) {
    if (!signature_[i]->optional()) {
      *error =
          api_errors::MissingRequiredArgument(signature_[i]->name().c_str());
      return false;
    }
  }

  return true;
}

std::string APISignature::GetExpectedSignature() const {
  if (!expected_signature_.empty() || signature_.empty())
    return expected_signature_;

  std::vector<std::string> pieces;
  pieces.reserve(signature_.size());
  const char* kOptionalPrefix = "optional ";
  for (const auto& spec : signature_) {
    pieces.push_back(
        base::StringPrintf("%s%s %s", spec->optional() ? kOptionalPrefix : "",
                           spec->GetTypeName().c_str(), spec->name().c_str()));
  }
  expected_signature_ = base::JoinString(pieces, ", ");

  return expected_signature_;
}

}  // namespace extensions

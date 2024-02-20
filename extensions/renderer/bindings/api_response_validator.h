// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "v8/include/v8.h"

namespace extensions {
class APITypeReferenceMap;

// A class to validate the responses to API calls sent by the browser. This
// helps ensure that the browser returns values that match the expected schema
// (which corresponds to the public documentation).
// TODO(devlin): This is now used for both API method responses and event
// arguments. Rename to APISignatureValidator?
class APIResponseValidator {
 public:
  // Allow overriding the default failure behavior.
  class TestHandler {
   public:
    using HandlerMethod =
        base::RepeatingCallback<void(const std::string&, const std::string&)>;

    explicit TestHandler(HandlerMethod method);

    TestHandler(const TestHandler&) = delete;
    TestHandler& operator=(const TestHandler&) = delete;

    ~TestHandler();

    // Ignores the given `signature` for testing purposes.
    void IgnoreSignature(std::string signature);

    // Forwards the failure call to the handler `method_`.
    void HandleFailure(const std::string& signature_name,
                       const std::string& error);

    // Returns true if the given `signature_name` should be ignored for
    // testing purposes.
    bool ShouldIgnoreSignature(const std::string& signature_name) const;

   private:
    HandlerMethod method_;

    std::set<std::string> signatures_to_ignore_;
  };

  // The origin of the callback passed to the response.
  enum class CallbackType {
    // NOTE(devlin): There's deliberately not a kNoCallback value here, since
    // ValidateResponse() is only invoked if there was some callback provided.
    // This is important, since some API implementations can adjust behavior
    // based on whether a callback is provided.

    // The callback was directly provided by the author script.
    kCallerProvided,
    // The callback was provided by the API, such as through custom bindings.
    kAPIProvided,
  };

  explicit APIResponseValidator(const APITypeReferenceMap* type_refs);

  APIResponseValidator(const APIResponseValidator&) = delete;
  APIResponseValidator& operator=(const APIResponseValidator&) = delete;

  ~APIResponseValidator();

  // Validates a response against the expected schema. By default, this will
  // NOTREACHED() in cases of validation failure.
  void ValidateResponse(v8::Local<v8::Context> context,
                        const std::string& method_name,
                        const v8::LocalVector<v8::Value>& response_arguments,
                        const std::string& api_error,
                        CallbackType callback_type);

  // Validates a collection of event arguments against the expected schema.
  // By default, this will NOTREACHED() in cases of validation failure.
  void ValidateEvent(v8::Local<v8::Context> context,
                     const std::string& event_name,
                     const v8::LocalVector<v8::Value>& event_args);

 private:
  // The type reference map; guaranteed to outlive this object.
  raw_ptr<const APITypeReferenceMap> type_refs_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_

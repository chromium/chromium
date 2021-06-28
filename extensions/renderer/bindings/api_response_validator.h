// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "v8/include/v8.h"

namespace extensions {
class APITypeReferenceMap;

// A class to validate the responses to API calls sent by the browser. This
// helps ensure that the browser returns values that match the expected schema
// (which corresponds to the public documentation).
class APIResponseValidator {
 public:
  // Allow overriding the default failure behavior.
  class TestHandler {
   public:
    using HandlerMethod =
        base::RepeatingCallback<void(const std::string&, const std::string&)>;

    explicit TestHandler(HandlerMethod method);
    ~TestHandler();

   private:
    HandlerMethod method_;

    DISALLOW_COPY_AND_ASSIGN(TestHandler);
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
  ~APIResponseValidator();

  // Validates a response against the expected schema. By default, this will
  // NOTREACHED() in cases of validation failure.
  void ValidateResponse(
      v8::Local<v8::Context> context,
      const std::string& method_name,
      const std::vector<v8::Local<v8::Value>> response_arguments,
      const std::string& api_error,
      CallbackType callback_type);

 private:
  // The type reference map; guaranteed to outlive this object.
  const APITypeReferenceMap* type_refs_;

  DISALLOW_COPY_AND_ASSIGN(APIResponseValidator);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_RESPONSE_VALIDATOR_H_

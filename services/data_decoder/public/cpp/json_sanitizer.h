// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_JSON_SANITIZER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_JSON_SANITIZER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"

namespace data_decoder {

// Sanitizes and normalizes JSON by parsing it in a safe environment and
// re-serializing it. Parsing the sanitized JSON should result in a value
// identical to parsing the original JSON.
//
// This allows parsing the sanitized JSON with the regular JSONParser while
// reducing the risk versus parsing completely untrusted JSON. It also minifies
// the resulting JSON, which might save some space.
class JsonSanitizer {
 public:
  struct Result {
    Result();
    Result(Result&&);
    ~Result();

    static Result Error(const std::string& error);

    base::Optional<std::string> value;
    base::Optional<std::string> error;
  };

  // Starts sanitizing the passed in unsafe JSON string. The passed |callback|
  // will be called with the result of the sanitization or an error message, but
  // not before the method returns.
  using Callback = base::OnceCallback<void(Result)>;
  static void Sanitize(const std::string& json, Callback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(JsonSanitizer);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_JSON_SANITIZER_H_

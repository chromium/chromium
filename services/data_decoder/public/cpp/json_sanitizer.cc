// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/json_sanitizer.h"

namespace data_decoder {

JsonSanitizer::Result::Result() = default;

JsonSanitizer::Result::Result(Result&&) = default;

JsonSanitizer::Result::~Result() = default;

// static
JsonSanitizer::Result JsonSanitizer::Result::Error(const std::string& error) {
  Result result;
  result.error = error;
  return result;
}

}  // namespace data_decoder

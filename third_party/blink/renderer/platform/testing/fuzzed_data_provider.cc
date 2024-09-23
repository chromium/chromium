// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

FuzzedDataProvider::FuzzedDataProvider(const uint8_t* bytes, size_t num_bytes)
    : provider_(bytes, num_bytes) {}

String FuzzedDataProvider::ConsumeRandomLengthString(size_t max_length) {
  std::string str = provider_.ConsumeRandomLengthString(max_length);
  // FromUTF8 will return a null string if the input data contains invalid UTF-8
  // sequences. Fall back to latin1 in those cases.
  return String::FromUTF8WithLatin1Fallback(str);
}

std::string FuzzedDataProvider::ConsumeRemainingBytes() {
  WebVector<char> bytes = provider_.ConsumeRemainingBytes<char>();
  return std::string(bytes.data(), bytes.size());
}

}  // namespace blink

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/base64.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "third_party/simdutf/simdutf.h"

namespace net {

bool SimdutfBase64Decode(std::string_view input,
                         std::string* output,
                         base::Base64DecodePolicy policy) {
  CHECK(base::FeatureList::IsEnabled(features::kSimdutfBase64Support));
  if (policy == base::Base64DecodePolicy::kStrict) {
    if (input.size() % 4 != 0) {
      // The input is not properly padded.
      return false;
    }
    if (std::ranges::any_of(input, [](char c) {
          return std::ranges::contains(base::kInfraAsciiWhitespace, c);
        })) {
      return false;
    }
  }

  std::string decode_buf;
  simdutf::result result{};
  decode_buf.resize_and_overwrite(
      simdutf::maximal_binary_length_from_base64(input.data(), input.size()),
      [input, &result](char* buffer, size_t available) {
        result = simdutf::base64_to_binary(input.data(), input.size(), buffer);
        if (result.error != simdutf::error_code::SUCCESS) {
          // On failure, count can be an input position, not an output size.
          return size_t{0};
        }
        // If this fails, the decoder has already written out of bounds.
        CHECK_LE(result.count, available);
        return result.count;
      });
  if (result.error != simdutf::error_code::SUCCESS) {
    return false;
  }

  *output = std::move(decode_buf);
  return true;
}

}  // namespace net

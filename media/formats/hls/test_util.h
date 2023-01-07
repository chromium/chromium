// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TEST_UTIL_H_
#define MEDIA_FORMATS_HLS_TEST_UTIL_H_

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

inline types::VariableName CreateVarName(base::StringPiece name) {
  return types::VariableName::Parse(SourceString::CreateForTesting(name))
      .value();
}

inline types::ByteRange CreateByteRange(types::DecimalInteger length,
                                        types::DecimalInteger offset) {
  return types::ByteRange::Validate(length, offset).value();
}

// Helper for comparing base::TimeDelta values which may have some
// floating-point imprecision.
inline testing::AssertionResult RoughlyEqual(
    base::TimeDelta lhs,
    base::TimeDelta rhs,
    base::TimeDelta epsilon = base::Milliseconds(1)) {
  if (lhs + epsilon >= rhs && lhs - epsilon <= rhs) {
    return testing::AssertionSuccess();
  }

  return testing::AssertionFailure()
         << lhs << " != " << rhs << " +- " << epsilon;
}

inline testing::AssertionResult RoughlyEqual(
    absl::optional<base::TimeDelta> lhs,
    absl::optional<base::TimeDelta> rhs,
    base::TimeDelta epsilon = base::Milliseconds(1)) {
  if (!lhs.has_value() && !rhs.has_value()) {
    return testing::AssertionSuccess();
  }
  if (lhs.has_value() && !rhs.has_value()) {
    return testing::AssertionFailure() << lhs.value() << " != absl::nullopt";
  }
  if (!lhs.has_value() && rhs.has_value()) {
    return testing::AssertionFailure() << "absl::nullopt != " << rhs.value();
  }

  return RoughlyEqual(lhs.value(), rhs.value(), epsilon);
}

}  // namespace media::hls

#endif

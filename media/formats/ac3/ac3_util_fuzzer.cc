// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "media/formats/ac3/ac3_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace media {

void ParseTotalAc3SampleCountNeverCrashes(base::span<const uint8_t> buffer) {
  Ac3Util::ParseTotalAc3SampleCount(buffer.data(), buffer.size());
}

FUZZ_TEST(Ac3UtilTest, ParseTotalAc3SampleCountNeverCrashes)
    .WithDomains(
        fuzztest::NonEmpty(fuzztest::Arbitrary<std::vector<uint8_t>>()));

void ParseTotalEac3SampleCountNeverCrashes(base::span<const uint8_t> buffer) {
  Ac3Util::ParseTotalEac3SampleCount(buffer.data(), buffer.size());
}

FUZZ_TEST(Ac3UtilTest, ParseTotalEac3SampleCountNeverCrashes)
    .WithDomains(
        fuzztest::NonEmpty(fuzztest::Arbitrary<std::vector<uint8_t>>()));

}  // namespace media

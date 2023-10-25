// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/fuzztest/src/fuzztest/domain.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/libyuv/include/libyuv.h"

#include "testing/libfuzzer/fuzzers/libyuv_scale_fuzzer.h"

// FuzzTest norms are to name the function according to the invariant
// we're trying to test
static void ScaleDoesNotCrash(bool is420,
                              int src_width,
                              int src_height,
                              int dst_width,
                              int dst_height,
                              int filter_num,
                              std::string seed_str) {
  Scale(is420, src_width, src_height, dst_width, dst_height, filter_num,
        seed_str);
}

// This happens to be the first FuzzTest fuzzer we've tried to use in Chrome.
// In an ideal world, we can skip the whole WithDomains stuff, and simply
// declare FUZZ_TEST(ScaleFuzz, ScaleDoesNotCrash).
// That will work for some functions which can accept truly arbitrary input.
// Domains are necessary in this case because only certain ranges of input
// are acceptable to the function.

FUZZ_TEST(ScaleFuzz, ScaleDoesNotCrash)
    .WithDomains(
        fuzztest::Arbitrary<bool>(),
        fuzztest::InRange(1, 256),
        fuzztest::InRange(1, 256),
        fuzztest::InRange(1, 256),
        fuzztest::InRange(1, 256),
        fuzztest::InRange(0, static_cast<int>(libyuv::FilterMode::kFilterBox)),
        fuzztest::Arbitrary<std::string>());

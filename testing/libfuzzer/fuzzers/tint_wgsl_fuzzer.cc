// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/dawn/src/tint/lang/wgsl/reader/reader.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void CanParseWithoutCrashing(std::string_view wgsl) {
  tint::Source::File file("test.wgsl", wgsl);
  tint::wgsl::reader::Options parse_options;
  parse_options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
  auto program = tint::wgsl::reader::Parse(&file, parse_options);
}

FUZZ_TEST(ChromiumTintWgslTest, CanParseWithoutCrashing);

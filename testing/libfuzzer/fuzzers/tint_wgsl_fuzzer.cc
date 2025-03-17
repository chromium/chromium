// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/dawn/src/tint/lang/wgsl/reader/reader.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "tint_wgsl_fuzzer_grammar.h"

void CanParseProgramWithoutCrashing(std::string_view wgsl) {
  tint::Source::File file("test.wgsl", wgsl);
  tint::wgsl::reader::Options parse_options;
  parse_options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
  auto program = tint::wgsl::reader::Parse(&file, parse_options);
}

void CanConvertWgslToIRWithoutCrashing(std::string_view wgsl) {
  tint::Source::File file("test.wgsl", wgsl);
  tint::wgsl::reader::Options parse_options;
  parse_options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
  auto module = tint::wgsl::reader::WgslToIR(&file, parse_options);
}

// In the following fuzzing tests, we're using two different domains:
// 1. InWgslGrammar(), that will generate a syntactically valid WGSL program
// based on the grammar at //testing/libfuzzer/fuzzers/wgsl_fuzzer/wgsl.g4.
// 2. Arbitrary<std::string> seeded with valid WGSL program. It is frequent
// that parsing bugs are triggered with data structure close to - but not
// matching - the expected data structure. Adding this domain will help finding
// such bugs, but will also ensure we're covering every possible fuzzing test
// cases.
FUZZ_TEST(ChromiumTintWgslTest, CanParseProgramWithoutCrashing)
    .WithDomains(fuzztest::OneOf(fuzztest::InWgslGrammar(),
                                 fuzztest::Arbitrary<std::string>().WithSeeds(
                                     []() -> std::vector<std::string> {
                                       auto domain = fuzztest::InWgslGrammar();
                                       return {domain.GetRandomValue(
                                           std::mt19937_64())};
                                     })));

FUZZ_TEST(ChromiumTintWgslTest, CanConvertWgslToIRWithoutCrashing)
    .WithDomains(fuzztest::OneOf(fuzztest::InWgslGrammar(),
                                 fuzztest::Arbitrary<std::string>().WithSeeds(
                                     []() -> std::vector<std::string> {
                                       auto domain = fuzztest::InWgslGrammar();
                                       return {domain.GetRandomValue(
                                           std::mt19937_64())};
                                     })));

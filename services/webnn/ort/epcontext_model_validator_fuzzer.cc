// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/epcontext_model_validator.h"

#include "base/containers/span.h"
#include "services/webnn/ort/epcontext_model_validator_test_support.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace webnn::ort {
namespace {

// Purely random bytes almost never form a valid ONNX protobuf, so a single
// well-formed model seed gives the mutator a foothold into the graph/node walk.
Bytes SeedModel() {
  return BuildModel({BuildEpContextNode()});
}

// The validator parses fully attacker-controlled bytes from the sandboxed
// Compiler process; it must never crash, over-read, or invoke UB regardless of
// input.
void ValidateNeverCrashes(const Bytes& model_data) {
  ValidateEpContextModel(base::span(model_data));
}

FUZZ_TEST(EpcontextModelValidatorFuzzTest, ValidateNeverCrashes)
    .WithDomains(fuzztest::Arbitrary<Bytes>())
    .WithSeeds({{SeedModel()}});

}  // namespace
}  // namespace webnn::ort

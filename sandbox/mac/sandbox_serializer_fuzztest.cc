// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_serializer.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace sandbox {

// Tests for the specific crash in the `ApplySerializedPolicy` path.
// Fuzz for an input that, when processed, causes the crash
// in `std::string`'s destructor (crbug.com/40066531).
static void CanApplySerializedPolicyWithoutCrashing(std::string input) {
  std::string error;
  {
    SandboxSerializer serializer(SandboxSerializer::Target::kSource);
    serializer.SetProfile(input);
    std::string serialized;
    if (serializer.SerializePolicy(serialized, error)) {
      std::ignore = serializer.ApplySerializedPolicy(serialized);
    }
  }

  {
    SandboxSerializer serializer(SandboxSerializer::Target::kCompiled);
    serializer.SetProfile(input);
    std::string serialized;
    if (serializer.SerializePolicy(serialized, error)) {
      std::ignore = serializer.ApplySerializedPolicy(serialized);
    }
  }
}

FUZZ_TEST(SandboxSerializerFuzzTest, CanApplySerializedPolicyWithoutCrashing);

static void CanDeserializeWithoutCrashing(std::string input) {
  std::string error;
  {
    SandboxSerializer serializer(SandboxSerializer::Target::kSource);
    std::ignore = serializer.DeserializePolicy(input, error);
  }
  {
    SandboxSerializer serializer(SandboxSerializer::Target::kCompiled);
    std::ignore = serializer.DeserializePolicy(input, error);
  }
}

FUZZ_TEST(SandboxSerializerFuzzTest, CanDeserializeWithoutCrashing);

}  // namespace sandbox

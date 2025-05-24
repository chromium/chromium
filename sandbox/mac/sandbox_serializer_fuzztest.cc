// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_serializer.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace sandbox {

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

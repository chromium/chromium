// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/test_trap_registry.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace bpf_dsl {

TestTrapRegistry::TestTrapRegistry() = default;

TestTrapRegistry::~TestTrapRegistry() = default;

uint16_t TestTrapRegistry::Add(const Handler& handler) {
  EXPECT_TRUE(handler.safe);

  const uint16_t next_id = map_.size() + 1;
  auto result = map_.insert({handler, next_id});
  return result.first->second;  // Old value if pre-existing handler.
}

bool TestTrapRegistry::EnableUnsafeTraps() {
  ADD_FAILURE() << "Unimplemented";
  return false;
}

}  // namespace bpf_dsl
}  // namespace sandbox

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/test_trap_registry.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace bpf_dsl {

TestTrapRegistry::TestTrapRegistry() : map_() {}
TestTrapRegistry::~TestTrapRegistry() {}

uint16_t TestTrapRegistry::Add(TrapFnc fnc, const void* aux, bool safe) {
  EXPECT_TRUE(safe);

  const uint16_t next_id = map_.size() + 1;
  return map_.insert(std::make_pair(Key(fnc, aux), next_id)).first->second;
}

bool TestTrapRegistry::EnableUnsafeTraps() {
  ADD_FAILURE() << "Unimplemented";
  return false;
}

}  // namespace bpf_dsl
}  // namespace sandbox

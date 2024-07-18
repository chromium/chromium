// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/bpf_dsl/test_trap_registry.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

intptr_t TestTrapFuncOne(const arch_seccomp_data& data, void* aux) {
  return 1;
}

intptr_t TestTrapFuncTwo(const arch_seccomp_data& data, void* aux) {
  return 2;
}

// Test that TestTrapRegistry correctly assigns trap IDs to trap handlers.
TEST(TestTrapRegistry, TrapIDs) {
  struct {
    TrapRegistry::TrapFnc fnc;
    raw_ptr<const void> aux;
  } funcs[] = {
      {TestTrapFuncOne, nullptr},
      {TestTrapFuncTwo, nullptr},
      {TestTrapFuncOne, funcs},
      {TestTrapFuncTwo, funcs},
  };

  TestTrapRegistry traps;

  // Add traps twice to test that IDs are reused correctly.
  for (int i = 0; i < 2; ++i) {
    for (size_t j = 0; j < std::size(funcs); ++j) {
      // Trap IDs start at 1.
      EXPECT_EQ(j + 1, traps.Add({funcs[j].fnc, funcs[j].aux, true}));
    }
  }
}

}  // namespace
}  // namespace bpf_dsl
}  // namespace sandbox

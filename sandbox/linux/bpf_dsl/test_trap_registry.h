// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_
#define SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <utility>

#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"

namespace sandbox {
namespace bpf_dsl {

class TestTrapRegistry : public TrapRegistry {
 public:
  TestTrapRegistry();

  TestTrapRegistry(const TestTrapRegistry&) = delete;
  TestTrapRegistry& operator=(const TestTrapRegistry&) = delete;

  virtual ~TestTrapRegistry();

  uint16_t Add(TrapFnc fnc, const void* aux, bool safe) override;
  bool EnableUnsafeTraps() override;

 private:
  using Key = std::pair<TrapFnc, const void*>;

  std::map<Key, uint16_t> map_;
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_

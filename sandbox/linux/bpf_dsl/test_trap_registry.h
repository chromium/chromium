// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_
#define SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <utility>

#include "sandbox/linux/bpf_dsl/trap_registry.h"

namespace sandbox {
namespace bpf_dsl {

class TestTrapRegistry : public TrapRegistry {
 public:
  TestTrapRegistry();

  TestTrapRegistry(const TestTrapRegistry&) = delete;
  TestTrapRegistry& operator=(const TestTrapRegistry&) = delete;

  virtual ~TestTrapRegistry();

  uint16_t Add(const Handler& handler) override;
  bool EnableUnsafeTraps() override;

 private:
  std::map<Handler, uint16_t> map_;
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_TEST_TRAP_REGISTRY_H_

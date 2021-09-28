// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER_H_
#define SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER_H_

#include "base/macros.h"

namespace sandbox {

// A simple "runner" class to implement tests.
class SandboxTestRunner {
 public:
  SandboxTestRunner();

  SandboxTestRunner(const SandboxTestRunner&) = delete;
  SandboxTestRunner& operator=(const SandboxTestRunner&) = delete;

  virtual ~SandboxTestRunner();

  virtual void Run() = 0;

  // Override to decide whether or not to check for leaks with LSAN
  // (if built with LSAN and LSAN is enabled).
  virtual bool ShouldCheckForLeaks() const;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/sandbox_test_runner.h"

namespace sandbox {

SandboxTestRunner::SandboxTestRunner() {
}

SandboxTestRunner::~SandboxTestRunner() {
}

bool SandboxTestRunner::ShouldCheckForLeaks() const {
  return true;
}

}  // namespace sandbox

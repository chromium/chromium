// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/resource_limits.h"

#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include "base/check_op.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

// Fails on Android: crbug.com/459158
#if !defined(OS_ANDROID)
#define MAYBE_NoFork DISABLE_ON_ASAN(NoFork)
#else
#define MAYBE_NoFork DISABLED_NoFork
#endif  // OS_ANDROID

// Not being able to fork breaks LeakSanitizer, so disable on
// all ASAN builds.
SANDBOX_TEST(ResourceLimits, MAYBE_NoFork) {
  // Make sure that fork will fail with EAGAIN.
  SANDBOX_ASSERT(ResourceLimits::Lower(RLIMIT_NPROC, 0) == 0);
  errno = 0;
  pid_t pid = fork();
  // Reap any child if fork succeeded.
  TestUtils::HandlePostForkReturn(pid);
  SANDBOX_ASSERT_EQ(-1, pid);
  CHECK_EQ(EAGAIN, errno);
}

}  // namespace

}  // namespace sandbox

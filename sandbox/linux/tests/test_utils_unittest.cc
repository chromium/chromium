// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/test_utils.h"

#include <sys/types.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

// Check that HandlePostForkReturn works.
TEST(TestUtils, HandlePostForkReturn) {
  pid_t pid = fork();
  TestUtils::HandlePostForkReturn(pid);
}

}  // namespace

}  // namespace sandbox

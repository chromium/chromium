// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_TEST_UTILS_H_
#define SANDBOX_LINUX_TESTS_TEST_UTILS_H_

#include <sys/types.h>

namespace sandbox {

// This class provide small helpers to help writing tests.
class TestUtils {
 public:
  TestUtils() = delete;
  TestUtils(const TestUtils&) = delete;
  TestUtils& operator=(const TestUtils&) = delete;

  static bool CurrentProcessHasChildren();
  // |pid| is the return value of a fork()-like call. This
  // makes sure that if fork() succeeded the child exits
  // and the parent waits for it.
  static void HandlePostForkReturn(pid_t pid);
  static void* MapPagesOrDie(size_t num_pages);
  static void MprotectLastPageOrDie(char* addr, size_t num_pages);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_TEST_UTILS_H_

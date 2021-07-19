// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_TEST_UTILS_H_
#define SANDBOX_LINUX_TESTS_TEST_UTILS_H_

#include <sys/types.h>

#include "base/macros.h"

namespace sandbox {

// This class provide small helpers to help writing tests.
class TestUtils {
 public:
  static bool CurrentProcessHasChildren();
  // |pid| is the return value of a fork()-like call. This
  // makes sure that if fork() succeeded the child exits
  // and the parent waits for it.
  static void HandlePostForkReturn(pid_t pid);
  static void* MapPagesOrDie(size_t num_pages);
  static void MprotectLastPageOrDie(char* addr, size_t num_pages);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestUtils);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_TEST_UTILS_H_

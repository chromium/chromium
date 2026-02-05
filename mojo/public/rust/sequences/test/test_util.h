// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_SEQUENCES_TEST_TEST_UTIL_H_
#define MOJO_PUBLIC_RUST_SEQUENCES_TEST_TEST_UTIL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"

namespace rust_sequences_test {

// Class for testing ScopedRefPtr. It holds a reference to a boolean, which it
// flips when the class is destroyed.
class TestRefCounted : public base::RefCounted<TestRefCounted> {
 public:
  // Store a pointer to a bool that lives on the test stack
  explicit TestRefCounted(bool& destroyed_flag);

 private:
  friend class base::RefCounted<TestRefCounted>;

  ~TestRefCounted();

  // Used for FFI
  RAW_PTR_EXCLUSION bool& destroyed_flag_;
};

// Create a new ref counted object and return a pointer which is suitable for
// wrapping in a rust ScopedRefPtr.
TestRefCounted* CreateTestRefCounted(bool& destroyed_flag);

// Create a task environment for testing.
std::unique_ptr<base::test::SingleThreadTaskEnvironment>
CreateTaskEnvironment();

}  // namespace rust_sequences_test

#endif  // MOJO_PUBLIC_RUST_SEQUENCES_TEST_TEST_UTIL_H_

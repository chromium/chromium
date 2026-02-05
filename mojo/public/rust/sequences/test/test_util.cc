// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_util.h"

#include "base/run_loop.h"

namespace rust_sequences_test {

TestRefCounted::TestRefCounted(bool& destroyed_flag)
    : destroyed_flag_(destroyed_flag) {
  destroyed_flag_ = false;
}

TestRefCounted::~TestRefCounted() {
  destroyed_flag_ = true;
}

TestRefCounted* CreateTestRefCounted(bool& destroyed_flag) {
  return base::MakeRefCounted<TestRefCounted>(destroyed_flag).release();
}

std::unique_ptr<base::test::SingleThreadTaskEnvironment>
CreateTaskEnvironment() {
  return std::make_unique<base::test::SingleThreadTaskEnvironment>();
}

}  // namespace rust_sequences_test

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/rust_gtest_interop/test/test_factory.h"

namespace {
size_t g_num_subclass_created = 0;
}

namespace rust_gtest_interop {

TestSubclass::TestSubclass() {
  ++g_num_subclass_created;
}

// static
size_t TestSubclass::num_created() {
  return g_num_subclass_created;
}

}  // namespace rust_gtest_interop

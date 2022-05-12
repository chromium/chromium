// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/rust_gtest_interop/test/test_subclass.h"

namespace {
size_t g_num_subclass_created = 0;
}

namespace rust_gtest_interop {

TestSubclass::TestSubclass() {
  ++g_num_subclass_created;
}

RUST_GTEST_TEST_SUITE_FACTORY(TestSubclass);

}  // namespace rust_gtest_interop

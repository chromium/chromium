// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/rust_gtest_interop/test/test_subclass.h"

namespace rust_gtest_interop {

TestSubclass::TestSubclass() = default;

RUST_GTEST_TEST_SUITE_FACTORY(TestSubclass)

TestSubclassWithCustomTemplate::TestSubclassWithCustomTemplate() = default;

RUST_CUSTOM_TEMPLATE_TEST_SUITE_FACTORY(TestSubclassWithCustomTemplate)

}  // namespace rust_gtest_interop

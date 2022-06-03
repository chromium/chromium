// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "mojo/public/cpp/bindings/tests/extra_cpp_template_unittest.test-mojom-extra_cpp_template_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace extra_cpp_template_unittest {

TEST(ExtraCPPTemplate, BasicTest) {
  // A basic smoke test to ensure templates passed into the mojom GN() target
  // successfully generate relevant C++ file.

  mojom::ExtraCppInterfaceTestClass test_obj;
  EXPECT_TRUE(test_obj.ReturnTrue());
}

}  // namespace extra_cpp_template_unittest
}  // namespace test
}  // namespace mojo

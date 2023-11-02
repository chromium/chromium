// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/interface_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace interface_unittest {
namespace {

using InterfaceTest = ::testing::Test;

TEST(InterfaceTest, Uuid) {
  constexpr base::Token kExpectedId{0x51e24935100f474eULL,
                                    0x90f155354bf92a00ULL};
  EXPECT_EQ(kExpectedId, mojom::VeryCoolInterface::Uuid_);
}

}  // namespace
}  // namespace interface_unittest
}  // namespace test
}  // namespace mojo

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_connection_helper.h"

#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class QuicChromiumConnectionHelperTest : public ::testing::Test {
 protected:
  QuicChromiumConnectionHelperTest() : helper_(&clock_, &random_generator_) {}

  QuicChromiumConnectionHelper helper_;
  quic::MockClock clock_;
  quic::test::MockRandom random_generator_;
};

TEST_F(QuicChromiumConnectionHelperTest, GetClock) {
  EXPECT_EQ(&clock_, helper_.GetClock());
}

TEST_F(QuicChromiumConnectionHelperTest, GetRandomGenerator) {
  EXPECT_EQ(&random_generator_, helper_.GetRandomGenerator());
}

}  // namespace
}  // namespace test
}  // namespace net

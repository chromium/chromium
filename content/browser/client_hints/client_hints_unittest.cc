// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ClientHintsTest, RttRoundedOff) {
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(6787)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(12)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(1193)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(12)) % 50);
}

TEST(ClientHintsTest, DownlinkRoundedOff) {
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 102) * 1000) % 50);
  EXPECT_GE(1, static_cast<int>(RoundKbpsToMbpsForTesting("", 12) * 1000) % 50);
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 2102) * 1000) % 50);

  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 102) * 1000) % 50);
  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 12) * 1000) % 50);
  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 2102) * 1000) % 50);
  EXPECT_GE(
      1, static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 12102) * 1000) %
             50);
}

// Verify that the value of RTT after adding noise is within approximately 10%
// of the original value. Note that the difference between the final value of
// RTT and the original value may be slightly more than 10% due to rounding off.
// To handle that, the maximum absolute difference allowed is set to a value
// slightly larger than 10% of the original metric value.
TEST(ClientHintsTest, FinalRttWithin10PercentValue) {
  EXPECT_NEAR(98, RoundRttForTesting("", base::Milliseconds(98)), 100);
  EXPECT_NEAR(1023, RoundRttForTesting("", base::Milliseconds(1023)), 200);
  EXPECT_NEAR(1193, RoundRttForTesting("", base::Milliseconds(1193)), 200);
  EXPECT_NEAR(2750, RoundRttForTesting("", base::Milliseconds(2750)), 400);
}

// Verify that the value of downlink after adding noise is within approximately
// 10% of the original value. Note that the difference between the final value
// of downlink and the original value may be slightly more than 10% due to
// rounding off. To handle that, the maximum absolute difference allowed is set
// to a value slightly larger than 10% of the original metric value.
TEST(ClientHintsTest, FinalDownlinkWithin10PercentValue) {
  EXPECT_NEAR(0.098, RoundKbpsToMbpsForTesting("", 98), 0.1);
  EXPECT_NEAR(1.023, RoundKbpsToMbpsForTesting("", 1023), 0.2);
  EXPECT_NEAR(1.193, RoundKbpsToMbpsForTesting("", 1193), 0.2);
  EXPECT_NEAR(7.523, RoundKbpsToMbpsForTesting("", 7523), 0.9);
  EXPECT_NEAR(9.999, RoundKbpsToMbpsForTesting("", 9999), 1.2);
}

TEST(ClientHintsTest, RttMaxValue) {
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(1023)));
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(2789)));
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(6023)));
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(2789)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(6023)) % 50);
}

TEST(ClientHintsTest, DownlinkMaxValue) {
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 102));
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 2102));
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 100102));
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 102) * 1000) % 50);
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 2102) * 1000) % 50);
  EXPECT_GE(
      1, static_cast<int>(RoundKbpsToMbpsForTesting("", 100102) * 1000) % 50);
}

TEST(ClientHintsTest, RttRandomized) {
  const int initial_value =
      RoundRttForTesting("example.com", base::Milliseconds(1023));
  bool network_quality_randomized_by_host = false;
  // There is a 1/20 chance that the same random noise is selected for two
  // different hosts. Run this test across 20 hosts to reduce the chances of
  // test failing to (1/20)^20.
  for (size_t i = 0; i < 20; ++i) {
    int value =
        RoundRttForTesting(base::NumberToString(i), base::Milliseconds(1023));
    // If |value| is different than |initial_value|, it implies that RTT is
    // randomized by host. This verifies the behavior, and test can be ended.
    if (value != initial_value)
      network_quality_randomized_by_host = true;
  }
  EXPECT_TRUE(network_quality_randomized_by_host);

  // Calling RoundRttForTesting for same host should return the same result.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundRttForTesting("example.com", base::Milliseconds(1023));
    EXPECT_EQ(initial_value, value);
  }
}

TEST(ClientHintsTest, DownlinkRandomized) {
  const int initial_value = RoundKbpsToMbpsForTesting("example.com", 1023);
  bool network_quality_randomized_by_host = false;
  // There is a 1/20 chance that the same random noise is selected for two
  // different hosts. Run this test across 20 hosts to reduce the chances of
  // test failing to (1/20)^20.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundKbpsToMbpsForTesting(base::NumberToString(i), 1023);
    // If |value| is different than |initial_value|, it implies that downlink is
    // randomized by host. This verifies the behavior, and test can be ended.
    if (value != initial_value)
      network_quality_randomized_by_host = true;
  }
  EXPECT_TRUE(network_quality_randomized_by_host);

  // Calling RoundMbps for same host should return the same result.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundKbpsToMbpsForTesting("example.com", 1023);
    EXPECT_EQ(initial_value, value);
  }
}

}  // namespace content

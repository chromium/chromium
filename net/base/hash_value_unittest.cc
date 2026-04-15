// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hash_value.h"

#include "testing/gtest/include/gtest/gtest.h"

static constexpr auto kRawHash = std::to_array<const uint8_t>({
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
});
static constexpr std::string_view kEncodedHash =
    "sha256/AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=";

TEST(HashValueTest, FromAndToString) {
  const auto h = net::HashValue::FromString(kEncodedHash);
  ASSERT_TRUE(h);
  EXPECT_EQ(h->span(), kRawHash);

  const auto r = h->ToString();
  EXPECT_EQ(r, kEncodedHash);

  EXPECT_FALSE(net::HashValue::FromString("sha256/foo"));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/hash_value_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/hash_value.h"
#include "services/network/public/mojom/hash_value.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(HashValueMojomTraitsTest, SerializeAndDeserialize) {
  net::SHA256HashValue original = {
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
       0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
       0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F}};
  net::SHA256HashValue copied;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SHA256HashValue>(
      original, copied));
  EXPECT_EQ(original, copied);
}

}  // namespace
}  // namespace network

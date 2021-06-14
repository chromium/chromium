// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

TEST(DatagramDuplexStreamTest, Defaults) {
  auto* duplex = MakeGarbageCollected<DatagramDuplexStream>(nullptr);
  EXPECT_FALSE(duplex->incomingMaxAge().has_value());
  EXPECT_FALSE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->incomingHighWaterMark(), kDefaultIncomingHighWaterMark);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), kDefaultOutgoingHighWaterMark);
}

TEST(DatagramDuplexStreamTest, SetIncomingMaxAge) {
  auto* duplex = MakeGarbageCollected<DatagramDuplexStream>(nullptr);

  duplex->setIncomingMaxAge(1.0);
  ASSERT_TRUE(duplex->incomingMaxAge().has_value());
  EXPECT_EQ(duplex->incomingMaxAge().value(), 1.0);

  duplex->setIncomingMaxAge(absl::nullopt);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());

  duplex->setIncomingMaxAge(0.0);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());

  duplex->setIncomingMaxAge(-1.0);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());
}

TEST(DatagramDuplexStreamTest, SetOutgoingMaxAge) {
  auto* duplex = MakeGarbageCollected<DatagramDuplexStream>(nullptr);

  duplex->setOutgoingMaxAge(1.0);
  ASSERT_TRUE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->outgoingMaxAge().value(), 1.0);

  duplex->setOutgoingMaxAge(absl::nullopt);
  ASSERT_FALSE(duplex->outgoingMaxAge().has_value());

  duplex->setOutgoingMaxAge(0.0);
  ASSERT_FALSE(duplex->outgoingMaxAge().has_value());

  duplex->setOutgoingMaxAge(-1.0);
  ASSERT_FALSE(duplex->outgoingMaxAge().has_value());
}

TEST(DatagramDuplexStreamTest, SetIncomingHighWaterMark) {
  auto* duplex = MakeGarbageCollected<DatagramDuplexStream>(nullptr);

  duplex->setIncomingHighWaterMark(10);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 10);

  duplex->setIncomingHighWaterMark(0);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 0);

  duplex->setIncomingHighWaterMark(-1);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 0);
}

TEST(DatagramDuplexStreamTest, SetOutgoingHighWaterMark) {
  auto* duplex = MakeGarbageCollected<DatagramDuplexStream>(nullptr);

  duplex->setOutgoingHighWaterMark(10);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 10);

  duplex->setOutgoingHighWaterMark(0);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 0);

  duplex->setOutgoingHighWaterMark(-1);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 0);
}

}  // namespace

}  // namespace blink

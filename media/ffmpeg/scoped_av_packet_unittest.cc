// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/scoped_av_packet.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void VerifyEmptyPacket(const ScopedAVPacket& packet) {
  EXPECT_EQ(packet.get(), nullptr);
  EXPECT_FALSE(packet);
}

void VerifyNonEmptyPacket(const ScopedAVPacket& packet) {
  EXPECT_NE(packet.get(), nullptr);
  EXPECT_TRUE(packet);
  EXPECT_EQ(&(*packet), packet.get());
  EXPECT_EQ(packet.operator->(), packet.get());
}

}  // namespace

TEST(ScopedAVPacketTest, DefaultConstructor) {
  ScopedAVPacket packet;
  VerifyEmptyPacket(packet);
}

TEST(ScopedAVPacketTest, Allocate) {
  auto packet = ScopedAVPacket::Allocate();
  VerifyNonEmptyPacket(packet);
}

TEST(ScopedAVPacketTest, Move) {
  ScopedAVPacket empty_packet;
  ScopedAVPacket empty_packet_copy = std::move(empty_packet);
  VerifyEmptyPacket(empty_packet_copy);

  auto packet = ScopedAVPacket::Allocate();
  ScopedAVPacket packet_copy = std::move(packet);
  VerifyNonEmptyPacket(packet_copy);
}

}  // namespace media

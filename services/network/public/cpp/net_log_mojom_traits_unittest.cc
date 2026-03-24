// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_log_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(NetLogTraitsTest, Roundtrips_CaptureMode) {
  for (const net::NetLogCaptureMode capture_mode :
       {net::NetLogCaptureMode::kDefault,
        net::NetLogCaptureMode::kIncludeSensitive,
        net::NetLogCaptureMode::kEverything}) {
    net::NetLogCaptureMode roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::NetLogCaptureMode>(
        capture_mode, roundtrip));
    EXPECT_EQ(capture_mode, roundtrip);
  }
}

TEST(NetLogTraitsTest, Roundtrips_EventPhase) {
  for (const net::NetLogEventPhase event_phase :
       {net::NetLogEventPhase::NONE, net::NetLogEventPhase::BEGIN,
        net::NetLogEventPhase::END}) {
    net::NetLogEventPhase roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::NetLogEventPhase>(
        event_phase, roundtrip));
    EXPECT_EQ(event_phase, roundtrip);
  }
}

}  // namespace
}  // namespace network

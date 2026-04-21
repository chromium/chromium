// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_packet_socket.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

namespace remoting::protocol {

class StreamPacketSocketTest : public testing::Test {
 public:
  StreamPacketSocketTest() = default;
  ~StreamPacketSocketTest() override = default;
};

TEST_F(StreamPacketSocketTest, SetOptionUninitialized) {
  StreamPacketSocket socket;
  // Should return -1 instead of crashing.
  EXPECT_EQ(socket.SetOption(webrtc::Socket::OPT_NODELAY, 1), -1);
  EXPECT_EQ(socket.SetOption(webrtc::Socket::OPT_DSCP, 1), -1);
}

}  // namespace remoting::protocol

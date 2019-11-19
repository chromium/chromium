// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/stun_field_trial.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace blink {

TEST(StunProbeTrial, VerifyParameterParsing) {
  StunProberTrial::Param params;
  std::string param_line;

  param_line = "20/100/1/3/3/server:3478/server2:3478";
  EXPECT_TRUE(StunProberTrial::ParseParameters(param_line, &params));
  EXPECT_EQ(params.requests_per_ip, 20);
  EXPECT_EQ(params.interval_ms, 100);
  EXPECT_EQ(params.shared_socket_mode, 1);
  EXPECT_EQ(params.batch_size, 3);
  EXPECT_EQ(params.total_batches, 3);
  EXPECT_EQ(params.servers.size(), 2u);
  EXPECT_EQ(params.servers[0], rtc::SocketAddress("server", 3478));
  EXPECT_EQ(params.servers[1], rtc::SocketAddress("server2", 3478));
  params.servers.clear();

  param_line = "/////server:3478";
  EXPECT_TRUE(StunProberTrial::ParseParameters(param_line, &params));
  EXPECT_EQ(params.requests_per_ip, 10);
  EXPECT_EQ(params.servers.size(), 1u);
  EXPECT_EQ(params.servers[0], rtc::SocketAddress("server", 3478));
  params.servers.clear();

  // Make sure there is no crash. Parsing will fail as there is no server
  // specified.
  param_line = "/////";
  EXPECT_FALSE(StunProberTrial::ParseParameters(param_line, &params));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

namespace blink {

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new blink::MockPeerConnectionDependencyFactory());
  }

 protected:
  std::unique_ptr<blink::MockPeerConnectionDependencyFactory>
      dependency_factory_;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  MockRTCPeerConnectionHandlerClient client_jsep;
  std::unique_ptr<RTCPeerConnectionHandler> pc_handler(
      dependency_factory_->CreateRTCPeerConnectionHandler(
          &client_jsep, blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
          /*force_encoded_audio_insertable_streams=*/false,
          /*force_encoded_video_insertable_streams=*/false));
  EXPECT_TRUE(pc_handler);
}

}  // namespace blink

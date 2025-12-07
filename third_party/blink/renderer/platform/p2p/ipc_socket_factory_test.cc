// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"

#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/testing/fake_mojo_binding_context.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/transport/ecn_marking.h"
#include "third_party/webrtc_overrides/environment.h"

namespace blink {

class IpcPacketSocketFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    mojo_binding_context_ = MakeGarbageCollected<FakeMojoBindingContext>(
        task_environment_.GetMainThreadTaskRunner());
    socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
        CrossThreadBindRepeating(
            [](base::OnceCallback<void(
                   std::optional<base::UnguessableToken>)>) {}),
        &P2PSocketDispatcher::From(*mojo_binding_context_),
        TRAFFIC_ANNOTATION_FOR_TESTS, false);

    socket_ = socket_factory_->CreateUdpSocket(
        WebRtcEnvironment(), webrtc::SocketAddress("127.0.0.1", 0), 0, 0);
    ASSERT_NE(socket_, nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  Persistent<FakeMojoBindingContext> mojo_binding_context_;
  std::unique_ptr<webrtc::PacketSocketFactory> socket_factory_;
  std::unique_ptr<webrtc::AsyncPacketSocket> socket_;
};

// Verify that the socket correctly handles the OPT_RECV_ECCN option.
TEST_F(IpcPacketSocketFactoryTest, SetOptions) {
  int desired_recv_ecn = 1;
  int recv_ecn_option = 0;
  EXPECT_EQ(0,
            socket_->GetOption(webrtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(-1, recv_ecn_option);
  EXPECT_EQ(0,
            socket_->SetOption(webrtc::Socket::OPT_RECV_ECN, desired_recv_ecn));
  EXPECT_EQ(0,
            socket_->GetOption(webrtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(webrtc::EcnMarking::kEct1,
            static_cast<webrtc::EcnMarking>(recv_ecn_option));
}

}  // namespace blink

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
#include "third_party/webrtc/rtc_base/network/ecn_marking.h"

namespace blink {

class IpcPacketSocketFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    mojo_binding_context_ = MakeGarbageCollected<FakeMojoBindingContext>(
        task_environment_.GetMainThreadTaskRunner());
    socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
        WTF::CrossThreadBindRepeating(
            [](base::OnceCallback<void(
                   std::optional<base::UnguessableToken>)>) {}),
        &P2PSocketDispatcher::From(*mojo_binding_context_),
        TRAFFIC_ANNOTATION_FOR_TESTS, false);

    socket_.reset(socket_factory_->CreateUdpSocket(
        rtc::SocketAddress("127.0.0.1", 0), 0, 0));
    ASSERT_NE(socket_, nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  Persistent<FakeMojoBindingContext> mojo_binding_context_;
  std::unique_ptr<rtc::PacketSocketFactory> socket_factory_;
  std::unique_ptr<rtc::AsyncPacketSocket> socket_;
};

// Verify that the socket correctly handles the OPT_RECV_ECCN option.
TEST_F(IpcPacketSocketFactoryTest, SetOptions) {
  int desired_recv_ecn = 1;
  int recv_ecn_option = 0;
  EXPECT_EQ(0, socket_->GetOption(rtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(-1, recv_ecn_option);
  EXPECT_EQ(0, socket_->SetOption(rtc::Socket::OPT_RECV_ECN, desired_recv_ecn));
  EXPECT_EQ(0, socket_->GetOption(rtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(rtc::EcnMarking::kEct1,
            static_cast<rtc::EcnMarking>(recv_ecn_option));
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/node_channel.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mock_node_channel_delegate.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

using ports::NodeName;
using testing::_;

class NodeChannelTest : public testing::Test {
 public:
  void SetUp() override {
    if (IsMojoIpczEnabled()) {
      GTEST_SKIP() << "NodeChannel is never used when ipcz is enabled, so "
                   << "these tests are neither supported nor relevant.";
    }
  }

  MockNodeChannelDelegate local_delegate_;
  MockNodeChannelDelegate remote_delegate_;
};

scoped_refptr<NodeChannel> CreateNodeChannel(NodeChannel::Delegate* delegate,
                                             PlatformChannelEndpoint endpoint) {
  return NodeChannel::Create(delegate, ConnectionParams(std::move(endpoint)),
                             Channel::HandlePolicy::kAcceptHandles,
                             GetIOTaskRunner(), base::NullCallback());
}

TEST_F(NodeChannelTest, DestructionIsSafe) {
  // Regression test for https://crbug.com/1081874.
  base::test::TaskEnvironment task_environment;

  PlatformChannel channel;
  auto local_channel =
      CreateNodeChannel(&local_delegate_, channel.TakeLocalEndpoint());
  local_channel->Start();
  auto remote_channel =
      CreateNodeChannel(&remote_delegate_, channel.TakeRemoteEndpoint());
  remote_channel->Start();

  // Verify end-to-end operation
  const NodeName kRemoteNodeName{123, 456};
  const NodeName kToken{987, 654};
  base::RunLoop loop;
  EXPECT_CALL(local_delegate_,
              OnAcceptInvitee(ports::kInvalidNodeName, kRemoteNodeName, kToken))
      .WillRepeatedly([&] { loop.Quit(); });
  remote_channel->AcceptInvitee(kRemoteNodeName, kToken);
  loop.Run();

  // Now send another message to the local endpoint but tear it down
  // immediately. This will race with the message being received on the IO
  // thread, and although the corresponding delegate call may or may not
  // dispatch as a result, the race should still be memory-safe.
  remote_channel->AcceptInvitee(kRemoteNodeName, kToken);

  base::RunLoop error_loop;
  EXPECT_CALL(remote_delegate_, OnChannelError).WillOnce([&] {
    error_loop.Quit();
  });
  local_channel.reset();
  error_loop.Run();
}

TEST_F(NodeChannelTest, MessagesCannotBeSmallerThanOldestVersion) {
  base::test::TaskEnvironment task_environment;

  PlatformChannel channel;
  auto local_channel =
      CreateNodeChannel(&local_delegate_, channel.TakeLocalEndpoint());
  local_channel->Start();
  auto remote_channel =
      CreateNodeChannel(&remote_delegate_, channel.TakeRemoteEndpoint());
  remote_channel->Start();

  base::RunLoop loop;

  // It's a bad message and shouldn't be passed to the delegate.
  EXPECT_CALL(local_delegate_, OnRequestPortMerge(_, _, _)).Times(0);

  // This good message should go through after.
  const NodeName kRemoteNodeName{123, 456};
  const NodeName kToken{987, 654};
  EXPECT_CALL(local_delegate_,
              OnAcceptInvitee(ports::kInvalidNodeName, kRemoteNodeName, kToken))
      .WillRepeatedly([&] { loop.Quit(); });

  // 1 byte is not enough to contain the oldest version of the request port
  // merge payload, it should be discarded.
  int payload_size = 1;
  int capacity = /*sizeof(header)=*/8 + payload_size;
  auto message =
      Channel::Message::CreateMessage(capacity, capacity, /*num_handles=*/0);

  memset(message->mutable_payload(), 0, capacity);

  // Set the type of this message as REQUEST_PORT_MERGE (6)
  *reinterpret_cast<uint32_t*>(message->mutable_payload()) = 6;

  // This short message should be ignored.
  remote_channel->SendChannelMessage(std::move(message));
  remote_channel->AcceptInvitee(kRemoteNodeName, kToken);
  loop.Run();

  remote_channel->ShutDown();
  local_channel->ShutDown();
}

}  // namespace
}  // namespace core
}  // namespace mojo

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/node_channel.h"

#include "base/bind_helpers.h"
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

using NodeChannelTest = testing::Test;
using ports::NodeName;

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
  MockNodeChannelDelegate local_delegate;
  auto local_channel =
      CreateNodeChannel(&local_delegate, channel.TakeLocalEndpoint());
  local_channel->Start();
  MockNodeChannelDelegate remote_delegate;
  auto remote_channel =
      CreateNodeChannel(&remote_delegate, channel.TakeRemoteEndpoint());
  remote_channel->Start();

  // Verify end-to-end operation
  const NodeName kRemoteNodeName{123, 456};
  const NodeName kToken{987, 654};
  base::RunLoop loop;
  EXPECT_CALL(local_delegate,
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
  EXPECT_CALL(remote_delegate, OnChannelError).WillOnce([&] {
    error_loop.Quit();
  });
  local_channel.reset();
  error_loop.Run();
}

}  // namespace
}  // namespace core
}  // namespace mojo

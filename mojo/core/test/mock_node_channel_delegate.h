// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_MOCK_NODE_CHANNEL_DELEGATE_H_
#define MOJO_CORE_TEST_MOCK_NODE_CHANNEL_DELEGATE_H_

#include "build/build_config.h"
#include "mojo/core/node_channel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace mojo {
namespace core {

// A NodeChannel Delegate implementation which can be used by NodeChannel unit
// tests and fuzzers.
class MockNodeChannelDelegate
    : public testing::NiceMock<NodeChannel::Delegate> {
 public:
  using NodeName = ports::NodeName;
  using PortName = ports::PortName;

  MockNodeChannelDelegate();
  MockNodeChannelDelegate(const MockNodeChannelDelegate&) = delete;
  MockNodeChannelDelegate& operator=(const MockNodeChannelDelegate&) = delete;
  ~MockNodeChannelDelegate() override;

  // testing::NiceMock<NodeChannel::Delegate> implementation:
  MOCK_METHOD(void,
              OnAcceptInvitee,
              (const NodeName& from_node,
               const NodeName& inviter_name,
               const NodeName& token),
              (override));
  MOCK_METHOD(void,
              OnAcceptInvitation,
              (const NodeName& from_node,
               const NodeName& token,
               const NodeName& invitee_name),
              (override));
  MOCK_METHOD(void,
              OnAddBrokerClient,
              (const NodeName& from_node,
               const NodeName& client_name,
               base::ProcessHandle process_handle),
              (override));
  MOCK_METHOD(void,
              OnBrokerClientAdded,
              (const NodeName& from_node,
               const NodeName& client_name,
               PlatformHandle broker_channel),
              (override));
  MOCK_METHOD(void,
              OnAcceptBrokerClient,
              (const NodeName& from_node,
               const NodeName& broker_name,
               PlatformHandle broker_channel,
               const uint64_t capabilities),
              (override));
  MOCK_METHOD(void,
              OnEventMessage,
              (const NodeName& from_node, Channel::MessagePtr message),
              (override));
  MOCK_METHOD(void,
              OnRequestPortMerge,
              (const NodeName& from_node,
               const PortName& connector_port_name,
               const std::string& token),
              (override));
  MOCK_METHOD(void,
              OnRequestIntroduction,
              (const NodeName& from_node, const NodeName& name),
              (override));
  MOCK_METHOD(void,
              OnIntroduce,
              (const NodeName& from_node,
               const NodeName& name,
               PlatformHandle channel_handle,
               const uint64_t remote_capabilites),
              (override));
  MOCK_METHOD(void,
              OnBroadcast,
              (const NodeName& from_node, Channel::MessagePtr message),
              (override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              OnRelayEventMessage,
              (const NodeName& from_node,
               base::ProcessHandle from_process,
               const NodeName& destination,
               Channel::MessagePtr message),
              (override));
  MOCK_METHOD(void,
              OnEventMessageFromRelay,
              (const NodeName& from_node,
               const NodeName& source_node,
               Channel::MessagePtr message),
              (override));
#endif
  MOCK_METHOD(void,
              OnAcceptPeer,
              (const NodeName& from_node,
               const NodeName& token,
               const NodeName& peer_name,
               const PortName& port_name),
              (override));
  MOCK_METHOD(void,
              OnChannelError,
              (const NodeName& node, NodeChannel* channel),
              (override));
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_TEST_MOCK_NODE_CHANNEL_DELEGATE_H_

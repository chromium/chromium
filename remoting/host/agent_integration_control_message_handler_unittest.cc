// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/agent_integration_control_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/agent_integration_control.pb.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

protocol::AgentIntegrationControl ParseMessage(const std::string& data) {
  protocol::AgentIntegrationControl message;
  message.ParseFromString(data);
  return message;
}

}  // namespace

class AgentIntegrationControlMessageHandlerTest : public testing::Test {
 public:
  AgentIntegrationControlMessageHandlerTest();
  ~AgentIntegrationControlMessageHandlerTest() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  protocol::FakeMessagePipe fake_pipe_{/*asynchronous=*/false};
  raw_ptr<MockAgentIntegrationConfigurator> configurator_ = nullptr;
};

AgentIntegrationControlMessageHandlerTest::
    AgentIntegrationControlMessageHandlerTest() {
  auto configurator = std::make_unique<MockAgentIntegrationConfigurator>();
  configurator_ = configurator.get();
  // Lifetime of the handler is controlled by `fake_pipe_`.
  new AgentIntegrationControlMessageHandler(
      std::move(configurator),
      AgentIntegrationControlMessageHandler::kDataChannelName,
      fake_pipe_.Wrap());
  fake_pipe_.OpenPipe();
}

AgentIntegrationControlMessageHandlerTest::
    ~AgentIntegrationControlMessageHandlerTest() {
  configurator_ = nullptr;
  if (fake_pipe_.pipe_opened()) {
    fake_pipe_.ClosePipe();
  }
}

TEST_F(AgentIntegrationControlMessageHandlerTest, QueryState_Enabled) {
  EXPECT_CALL(*configurator_, IsAgentIntegrationEnabled(_))
      .WillOnce(RunOnceCallback<0>(true));

  protocol::AgentIntegrationControl request;
  request.mutable_query_state_request();
  fake_pipe_.ReceiveProtobufMessage(request);

  ASSERT_EQ(fake_pipe_.sent_messages().size(), 1u);
  protocol::AgentIntegrationControl response =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(response.has_query_state_response());
  EXPECT_TRUE(response.query_state_response().enabled());
}

TEST_F(AgentIntegrationControlMessageHandlerTest, QueryState_Disabled) {
  EXPECT_CALL(*configurator_, IsAgentIntegrationEnabled(_))
      .WillOnce(RunOnceCallback<0>(false));

  protocol::AgentIntegrationControl request;
  request.mutable_query_state_request();
  fake_pipe_.ReceiveProtobufMessage(request);

  ASSERT_EQ(fake_pipe_.sent_messages().size(), 1u);
  protocol::AgentIntegrationControl response =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(response.has_query_state_response());
  EXPECT_FALSE(response.query_state_response().enabled());
}

TEST_F(AgentIntegrationControlMessageHandlerTest, SetState_EnableSuccess) {
  EXPECT_CALL(*configurator_, SetAgentIntegrationEnabled(true, _))
      .WillOnce(RunOnceCallback<1>(true));

  protocol::AgentIntegrationControl request;
  request.mutable_set_state_request()->set_enabled(true);
  fake_pipe_.ReceiveProtobufMessage(request);

  ASSERT_EQ(fake_pipe_.sent_messages().size(), 1u);
  protocol::AgentIntegrationControl response =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(response.has_set_state_response());
  EXPECT_TRUE(response.set_state_response().success());
}

TEST_F(AgentIntegrationControlMessageHandlerTest, SetState_DisableSuccess) {
  EXPECT_CALL(*configurator_, SetAgentIntegrationEnabled(false, _))
      .WillOnce(RunOnceCallback<1>(true));

  protocol::AgentIntegrationControl request;
  request.mutable_set_state_request()->set_enabled(false);
  fake_pipe_.ReceiveProtobufMessage(request);

  ASSERT_EQ(fake_pipe_.sent_messages().size(), 1u);
  protocol::AgentIntegrationControl response =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(response.has_set_state_response());
  EXPECT_TRUE(response.set_state_response().success());
}

TEST_F(AgentIntegrationControlMessageHandlerTest, SetState_EnableFailure) {
  EXPECT_CALL(*configurator_, SetAgentIntegrationEnabled(true, _))
      .WillOnce(RunOnceCallback<1>(false));

  protocol::AgentIntegrationControl request;
  request.mutable_set_state_request()->set_enabled(true);
  fake_pipe_.ReceiveProtobufMessage(request);

  ASSERT_EQ(fake_pipe_.sent_messages().size(), 1u);
  protocol::AgentIntegrationControl response =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(response.has_set_state_response());
  EXPECT_FALSE(response.set_state_response().success());
}

TEST_F(AgentIntegrationControlMessageHandlerTest,
       SetState_MissingEnabledField) {
  EXPECT_CALL(*configurator_, SetAgentIntegrationEnabled(_, _)).Times(0);

  protocol::AgentIntegrationControl request;
  request.mutable_set_state_request();
  fake_pipe_.ReceiveProtobufMessage(request);

  EXPECT_EQ(fake_pipe_.sent_messages().size(), 0u);
}

}  // namespace remoting

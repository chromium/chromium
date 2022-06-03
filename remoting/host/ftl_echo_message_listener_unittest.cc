// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_echo_message_listener.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NotNull;
using testing::Return;
using testing::Unused;

namespace remoting {

namespace {

constexpr char kOwnerEmail[] = "machine_owner@gmail.com";
constexpr char kTestJid[] = "machine_owner@gmail.com/chromoting_ftl_abc123";
constexpr char kUnknownEmail[] = "not_the_machine_owner@gmail.com";
constexpr char kRegistrationId[] = "lkfhilawhfilauefiuw";
constexpr char kSystemServiceName[] = "chromoting-backend-service";
constexpr char kEchoMessagePayload[] = "Echo!";
constexpr char kSuperLongMessagePayload[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kTruncatedMessagePayload[] = "aaaaaaaaaaaaaaaa";

ACTION_P(AddListener, list) {
  list->insert(arg0);
}

ACTION_P(RemoveListener, list) {
  EXPECT_TRUE(list->find(arg0) != list->end());
  list->erase(arg0);
}

ftl::ChromotingMessage CreateEchoMessageWithPayload(
    const std::string& message_payload) {
  ftl::ChromotingMessage message;
  message.mutable_echo()->set_message(message_payload);
  return message;
}

}  // namespace

class FtlEchoMessageListenerTest : public testing::Test {
 protected:
  FtlEchoMessageListenerTest() : signal_strategy_(SignalingAddress(kTestJid)) {}

  void SetUp() override {
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));

    system_sender_id_.set_type(ftl::IdType_Type_SYSTEM);
    system_sender_id_.set_id(kSystemServiceName);

    machine_owner_sender_id_.set_type(ftl::IdType_Type_EMAIL);
    machine_owner_sender_id_.set_id(kOwnerEmail);

    unknown_sender_id_.set_type(ftl::IdType_Type_EMAIL);
    unknown_sender_id_.set_id(kUnknownEmail);

    ftl_echo_message_listener_ = std::make_unique<FtlEchoMessageListener>(
        kOwnerEmail, &signal_strategy_);
  }

  void TearDown() override {
    ftl_echo_message_listener_.reset();
    EXPECT_TRUE(signal_strategy_listeners_.empty());
  }

 protected:
  ftl::Id system_sender_id_;
  ftl::Id machine_owner_sender_id_;
  ftl::Id unknown_sender_id_;

  base::test::TaskEnvironment task_environment_;

  MockSignalStrategy signal_strategy_;
  std::set<SignalStrategy::Listener*> signal_strategy_listeners_;
  std::unique_ptr<FtlEchoMessageListener> ftl_echo_message_listener_;
};

TEST_F(FtlEchoMessageListenerTest, EchoRequestFromOwnerHandled) {
  base::RunLoop run_loop;
  EXPECT_CALL(signal_strategy_, SendMessage(_, _))
      .WillOnce([&](const SignalingAddress& destination_address,
                    const ftl::ChromotingMessage& message) -> bool {
        std::string username;
        std::string registration_id;
        EXPECT_TRUE(
            destination_address.GetFtlInfo(&username, &registration_id));
        EXPECT_EQ(kOwnerEmail, username);
        EXPECT_EQ(kRegistrationId, registration_id);
        EXPECT_TRUE(message.has_echo());
        EXPECT_EQ(kEchoMessagePayload, message.echo().message());

        run_loop.Quit();
        return true;
      });

  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      machine_owner_sender_id_, kRegistrationId,
      CreateEchoMessageWithPayload(kEchoMessagePayload));
  ASSERT_TRUE(is_handled);

  run_loop.Run();
}

TEST_F(FtlEchoMessageListenerTest, EchoRequestFromServiceRejected) {
  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      system_sender_id_, {}, CreateEchoMessageWithPayload(kEchoMessagePayload));
  ASSERT_FALSE(is_handled);
}

TEST_F(FtlEchoMessageListenerTest, EchoRequestFromNonOwnerRejected) {
  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      unknown_sender_id_, {},
      CreateEchoMessageWithPayload(kEchoMessagePayload));
  ASSERT_FALSE(is_handled);
}

TEST_F(FtlEchoMessageListenerTest, SuperLongMessageIsTruncated) {
  base::RunLoop run_loop;
  EXPECT_CALL(signal_strategy_, SendMessage(_, _))
      .WillOnce([&](Unused, const ftl::ChromotingMessage& message) -> bool {
        EXPECT_EQ(kTruncatedMessagePayload, message.echo().message());

        run_loop.Quit();
        return true;
      });

  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      machine_owner_sender_id_, kRegistrationId,
      CreateEchoMessageWithPayload(kSuperLongMessagePayload));
  ASSERT_TRUE(is_handled);

  run_loop.Run();
}

TEST_F(FtlEchoMessageListenerTest, EmptyMessageIsRejected) {
  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      machine_owner_sender_id_, kRegistrationId, ftl::ChromotingMessage());
  ASSERT_FALSE(is_handled);
}

TEST_F(FtlEchoMessageListenerTest, EmptyMessagePayloadIsHandled) {
  base::RunLoop run_loop;
  EXPECT_CALL(signal_strategy_, SendMessage(_, _))
      .WillOnce([&](Unused, const ftl::ChromotingMessage& message) -> bool {
        EXPECT_TRUE(message.has_echo());
        EXPECT_TRUE(message.echo().message().empty());

        run_loop.Quit();
        return true;
      });

  bool is_handled = ftl_echo_message_listener_->OnSignalStrategyIncomingMessage(
      machine_owner_sender_id_, kRegistrationId,
      CreateEchoMessageWithPayload(""));
  ASSERT_TRUE(is_handled);

  run_loop.Run();
}

}  // namespace remoting

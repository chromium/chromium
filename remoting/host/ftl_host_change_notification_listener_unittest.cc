// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_host_change_notification_listener.h"

#include <set>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NotNull;
using testing::Return;

namespace remoting {

namespace {

const char kTestJid[] = "user@gmail.com/chromoting_ftl_abc123";
const char kSystemSenderId[] = "chromoting-backend-service";
const char kPeerSenderId[] = "fake_peer@gmail.com";

ACTION_P(AddListener, list) {
  list->insert(arg0);
}

ACTION_P(RemoveListener, list) {
  EXPECT_TRUE(list->find(arg0) != list->end());
  list->erase(arg0);
}

SignalingMessage CreateMessageWithDirectoryState(
    ftl::HostStatusChangeMessage_DirectoryState state) {
  ftl::ChromotingMessage message;
  message.mutable_status()->set_directory_state(state);
  return SignalingMessage{message};
}

}  // namespace

class FtlHostChangeNotificationListenerTest : public testing::Test {
 protected:
  FtlHostChangeNotificationListenerTest()
      : signal_strategy_(SignalingAddress(kTestJid)),
        system_sender_address_(
            SignalingAddress::CreateFtlSystemAddress(kSystemSenderId)),
        peer_sender_address_(SignalingAddress::CreateFtlSignalingAddress(
            kPeerSenderId,
            "fake_registration_id")) {}
  class MockListener : public FtlHostChangeNotificationListener::Listener {
   public:
    MOCK_METHOD0(OnHostDeleted, void());
  };

  void SetUp() override {
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));

    ftl_host_change_notification_listener_ =
        std::make_unique<FtlHostChangeNotificationListener>(&mock_listener_,
                                                            &signal_strategy_);
  }

  void TearDown() override {
    ftl_host_change_notification_listener_.reset();
    EXPECT_TRUE(signal_strategy_listeners_.empty());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  MockListener mock_listener_;
  MockSignalStrategy signal_strategy_;
  SignalingAddress system_sender_address_;
  SignalingAddress peer_sender_address_;
  std::set<raw_ptr<SignalStrategy::Listener, SetExperimental>>
      signal_strategy_listeners_;
  std::unique_ptr<FtlHostChangeNotificationListener>
      ftl_host_change_notification_listener_;
};

TEST_F(FtlHostChangeNotificationListenerTest, ReceiveValidNotification) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_listener_, OnHostDeleted()).WillOnce([&]() {
    run_loop.Quit();
  });
  bool is_handled =
      ftl_host_change_notification_listener_->OnSignalStrategyIncomingMessage(
          system_sender_address_,
          CreateMessageWithDirectoryState(
              ftl::HostStatusChangeMessage_DirectoryState_DELETED));
  ASSERT_TRUE(is_handled);
  run_loop.Run();
}

TEST_F(FtlHostChangeNotificationListenerTest,
       ReceiveNotificationThenDeleteObject_CallbackNotCalled) {
  EXPECT_CALL(mock_listener_, OnHostDeleted()).Times(0);
  bool is_handled =
      ftl_host_change_notification_listener_->OnSignalStrategyIncomingMessage(
          system_sender_address_,
          CreateMessageWithDirectoryState(
              ftl::HostStatusChangeMessage_DirectoryState_DELETED));
  ASSERT_TRUE(is_handled);
  ftl_host_change_notification_listener_.reset();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(FtlHostChangeNotificationListenerTest,
       ReceiveNonSystemNotification_Ignored) {
  EXPECT_CALL(mock_listener_, OnHostDeleted()).Times(0);
  bool is_handled =
      ftl_host_change_notification_listener_->OnSignalStrategyIncomingMessage(
          peer_sender_address_,
          CreateMessageWithDirectoryState(
              ftl::HostStatusChangeMessage_DirectoryState_DELETED));
  ASSERT_FALSE(is_handled);
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(FtlHostChangeNotificationListenerTest,
       ReceiveUnknownChromotingMessage_Ignored) {
  EXPECT_CALL(mock_listener_, OnHostDeleted()).Times(0);
  bool is_handled =
      ftl_host_change_notification_listener_->OnSignalStrategyIncomingMessage(
          system_sender_address_, SignalingMessage{ftl::ChromotingMessage()});
  ASSERT_FALSE(is_handled);
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(FtlHostChangeNotificationListenerTest,
       ReceiveUnknownDirectoryState_Ignored) {
  EXPECT_CALL(mock_listener_, OnHostDeleted()).Times(0);
  bool is_handled =
      ftl_host_change_notification_listener_->OnSignalStrategyIncomingMessage(
          system_sender_address_,
          CreateMessageWithDirectoryState(
              ftl::HostStatusChangeMessage_DirectoryState_NOT_SET));
  ASSERT_FALSE(is_handled);
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace remoting

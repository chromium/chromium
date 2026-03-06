// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/iq_sender.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

namespace remoting {

namespace {

const char kStanzaId[] = "123";
const char kLocalUser[] = "local_user@domain.com";
const char kRemoteUser[] = "remote_user@domain.com";
const char kUpperCaseRemoteUser[] = "REMOTE_USER@domain.com";

MATCHER_P(ReplyEq, expected, "") {
  return arg.reply_type == expected.reply_type &&
         arg.error_type == expected.error_type && arg.text == expected.text;
}

}  // namespace

class IqSenderTest : public testing::Test {
 public:
  IqSenderTest() : signal_strategy_(SignalingAddress(kLocalUser)) {
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()));
    sender_ = std::make_unique<IqSender>(&signal_strategy_);
    EXPECT_CALL(
        signal_strategy_,
        RemoveListener(static_cast<SignalStrategy::Listener*>(sender_.get())));
  }

 protected:
  void SendTestMessage() {
    JingleMessage message;
    message.to = SignalingAddress(kRemoteUser);
    message.sid = "test_sid";
    message.message_id = kStanzaId;
    message.SetPayload(SessionTerminate());

    EXPECT_CALL(signal_strategy_, SendMessage(_))
        .WillOnce([&](JingleMessage&& message_arg) {
          EXPECT_EQ(message_arg.to, message.to);
          EXPECT_EQ(message_arg.sid, message.sid);
          EXPECT_EQ(message_arg.message_id, message.message_id);
          EXPECT_EQ(message_arg.action(), message.action());
          EXPECT_TRUE(std::get_if<SessionTerminate>(&message_arg.payload()));
          return true;
        });
    request_ = sender_->SendIq(std::move(message), callback_.Get());
  }

  bool DeliverResponse(const std::string& from,
                       JingleMessageReply* reply_out = nullptr) {
    JingleMessageReply reply;
    reply.reply_type = JingleMessageReply::REPLY_RESULT;
    reply.message_id = kStanzaId;
    reply.from = SignalingAddress(from);

    bool result = sender_->OnSignalingReply(SignalingAddress(from), reply);

    if (reply_out) {
      *reply_out = std::move(reply);
    }
    return result;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockSignalStrategy signal_strategy_;
  std::unique_ptr<IqSender> sender_;
  base::MockCallback<IqSender::ReplyCallback> callback_;
  std::unique_ptr<IqRequest> request_;
};

TEST_F(IqSenderTest, SendIq) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  JingleMessageReply expected_reply;
  EXPECT_TRUE(DeliverResponse(kRemoteUser, &expected_reply));

  EXPECT_CALL(callback_, Run(request_.get(), ReplyEq(expected_reply)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, Timeout) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  request_->SetTimeout(base::Milliseconds(2));

  JingleMessageReply expected_reply;
  expected_reply.reply_type = JingleMessageReply::REPLY_ERROR;
  expected_reply.error_type = JingleMessageReply::UNEXPECTED_REQUEST;
  expected_reply.text = "timeout";

  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Run(request_.get(), ReplyEq(expected_reply)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  run_loop.Run();
}

TEST_F(IqSenderTest, NotNormalizedJid) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  // Use an upper-case value to verify it is normalized.
  JingleMessageReply expected_reply;
  EXPECT_TRUE(DeliverResponse(kUpperCaseRemoteUser, &expected_reply));

  EXPECT_CALL(callback_, Run(request_.get(), ReplyEq(expected_reply)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, InvalidFrom) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  EXPECT_FALSE(DeliverResponse("different_user@domain.com", nullptr));

  EXPECT_CALL(callback_, Run(_, _)).Times(0);
  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting

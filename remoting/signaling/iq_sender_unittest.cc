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
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/xmpp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

using ::jingle_xmpp::QName;
using ::jingle_xmpp::XmlElement;

namespace remoting {

namespace {

const char kStanzaId[] = "123";
const char kTo[] = "user@domain.com";

MATCHER_P(ReplyEq, expected, "") {
  return arg.reply_type == expected.reply_type &&
         arg.error_type == expected.error_type && arg.text == expected.text;
}

}  // namespace

class IqSenderTest : public testing::Test {
 public:
  IqSenderTest() : signal_strategy_(SignalingAddress("local_jid@domain.com")) {
    EXPECT_CALL(signal_strategy_, AddListener(NotNull()));
    sender_ = std::make_unique<IqSender>(&signal_strategy_);
    EXPECT_CALL(
        signal_strategy_,
        RemoveListener(static_cast<SignalStrategy::Listener*>(sender_.get())));
  }

 protected:
  void SendTestMessage() {
    JingleMessage message;
    message.to = SignalingAddress(kTo);
    message.sid = "test_sid";
    message.message_id = kStanzaId;
    message.SetPayload(SessionTerminate());

    EXPECT_CALL(signal_strategy_, SendMessage(SignalingAddress(kTo), _))
        .WillOnce([&](const SignalingAddress&, SignalingMessage&& message_arg) {
          auto* sent_jingle_message = std::get_if<JingleMessage>(&message_arg);
          EXPECT_TRUE(sent_jingle_message);
          if (sent_jingle_message) {
            std::unique_ptr<XmlElement> sent_stanza =
                JingleMessageToXml(*sent_jingle_message);
            std::unique_ptr<XmlElement> expected_stanza =
                JingleMessageToXml(message);
            EXPECT_EQ(expected_stanza->Str(), sent_stanza->Str());
          }
          return true;
        });
    request_ = sender_->SendIq(message, callback_.Get());
  }

  bool FormatAndDeliverResponse(const std::string& from,
                                std::unique_ptr<XmlElement>* response_out) {
    std::unique_ptr<XmlElement> response(new XmlElement(kQNameIq));
    response->AddAttr(QName(std::string(), "type"), "result");
    response->AddAttr(QName(std::string(), "id"), kStanzaId);
    response->AddAttr(QName(std::string(), "from"), from);

    XmlElement* response_body =
        new XmlElement(QName("test:namespace", "response-body"));
    response->AddElement(response_body);

    JingleMessageReply reply;
    bool parse_result = JingleMessageReplyFromXml(response.get(), &reply);
    DCHECK(parse_result);
    reply.message_id = kStanzaId;
    reply.from = SignalingAddress(from);

    bool result = sender_->OnSignalStrategyIncomingMessage(
        SignalingAddress(from), SignalingMessage(reply));

    if (response_out) {
      *response_out = std::move(response);
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

  std::unique_ptr<XmlElement> response_xml;
  EXPECT_TRUE(FormatAndDeliverResponse(kTo, &response_xml));

  JingleMessageReply expected_reply;
  ASSERT_TRUE(JingleMessageReplyFromXml(response_xml.get(), &expected_reply));

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

  // Set upper-case from value, which is equivalent to kTo in the original
  // message.
  std::unique_ptr<XmlElement> response_xml;
  EXPECT_TRUE(FormatAndDeliverResponse("USER@domain.com", &response_xml));

  JingleMessageReply expected_reply;
  ASSERT_TRUE(JingleMessageReplyFromXml(response_xml.get(), &expected_reply));

  EXPECT_CALL(callback_, Run(request_.get(), ReplyEq(expected_reply)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, InvalidFrom) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  EXPECT_FALSE(FormatAndDeliverResponse("different_user@domain.com", nullptr));

  EXPECT_CALL(callback_, Run(_, _)).Times(0);
  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting

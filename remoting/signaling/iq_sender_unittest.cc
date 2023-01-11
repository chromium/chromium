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
const char kNamespace[] = "chromium:testns";
const char kNamespacePrefix[] = "tes";
const char kBodyTag[] = "test";
const char kType[] = "get";
const char kTo[] = "user@domain.com";

MATCHER_P(XmlEq, expected, "") {
  return arg->Str() == expected->Str();
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
    std::unique_ptr<XmlElement> iq_body(
        new XmlElement(QName(kNamespace, kBodyTag)));
    XmlElement* sent_stanza;
    EXPECT_CALL(signal_strategy_, GetNextId()).WillOnce(Return(kStanzaId));
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(_))
        .WillOnce(DoAll(SaveArg<0>(&sent_stanza), Return(true)));
    request_ = sender_->SendIq(kType, kTo, std::move(iq_body), callback_.Get());

    std::string expected_xml_string = base::StringPrintf(
        "<cli:iq type=\"%s\" to=\"%s\" id=\"%s\" "
        "xmlns:cli=\"jabber:client\">"
        "<%s:%s xmlns:%s=\"%s\"/>"
        "</cli:iq>",
        kType, kTo, kStanzaId, kNamespacePrefix, kBodyTag, kNamespacePrefix,
        kNamespace);
    EXPECT_EQ(expected_xml_string, sent_stanza->Str());
    delete sent_stanza;
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

    bool result = sender_->OnSignalStrategyIncomingStanza(response.get());

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

  std::unique_ptr<XmlElement> response;
  EXPECT_TRUE(FormatAndDeliverResponse(kTo, &response));

  EXPECT_CALL(callback_, Run(request_.get(), XmlEq(response.get())));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, Timeout) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  request_->SetTimeout(base::Milliseconds(2));

  base::RunLoop run_loop;
  EXPECT_CALL(callback_, Run(request_.get(), nullptr))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  run_loop.Run();
}

TEST_F(IqSenderTest, NotNormalizedJid) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  // Set upper-case from value, which is equivalent to kTo in the original
  // message.
  std::unique_ptr<XmlElement> response;
  EXPECT_TRUE(FormatAndDeliverResponse("USER@domain.com", &response));

  EXPECT_CALL(callback_, Run(request_.get(), XmlEq(response.get())));
  base::RunLoop().RunUntilIdle();
}

TEST_F(IqSenderTest, InvalidFrom) {
  ASSERT_NO_FATAL_FAILURE({ SendTestMessage(); });

  EXPECT_FALSE(FormatAndDeliverResponse("different_user@domain.com", nullptr));

  EXPECT_CALL(callback_, Run(_, _)).Times(0);
  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting

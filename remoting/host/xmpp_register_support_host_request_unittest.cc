// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/xmpp_register_support_host_request.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/host/host_details.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

using testing::_;
using testing::DeleteArg;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace remoting {

using protocol::ErrorCode;

namespace {
const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kTestJid[] = "User@gmail.com/chromotingABC123";
const char kTestJidNormalized[] = "user@gmail.com/chromotingABC123";
const char kSupportId[] = "AB4RF3";
const char kSupportIdLifetime[] = "300";
const char kStanzaId[] = "123";

ACTION_P(AddListener, list) {
  list->AddObserver(arg0);
}
ACTION_P(RemoveListener, list) {
  list->RemoveObserver(arg0);
}

}  // namespace

class XmppRegisterSupportHostRequestTest : public testing::Test {
 public:
 protected:
  XmppRegisterSupportHostRequestTest()
      : signal_strategy_(SignalingAddress(kTestJid)) {}

  void SetUp() override {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    ASSERT_TRUE(key_pair_.get());

    EXPECT_CALL(signal_strategy_, AddListener(NotNull()))
        .WillRepeatedly(AddListener(&signal_strategy_listeners_));
    EXPECT_CALL(signal_strategy_, RemoveListener(NotNull()))
        .WillRepeatedly(RemoveListener(&signal_strategy_listeners_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockSignalStrategy signal_strategy_;
  base::ObserverList<SignalStrategy::Listener, true> signal_strategy_listeners_;
  scoped_refptr<RsaKeyPair> key_pair_;
  base::MockCallback<RegisterSupportHostRequest::RegisterCallback> callback_;
};

TEST_F(XmppRegisterSupportHostRequestTest, Timeout) {
  auto request = std::make_unique<XmppRegisterSupportHostRequest>(kTestBotJid);
  request->StartRequest(&signal_strategy_, key_pair_, callback_.Get());
  EXPECT_CALL(signal_strategy_, GetNextId()).WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(DeleteArg<0>(), Return(true)));

  request->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);

  // Generate response and verify that callback is called.
  EXPECT_CALL(callback_, Run("", base::TimeDelta::FromSeconds(0),
                             ErrorCode::SIGNALING_TIMEOUT));

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(15));
}

TEST_F(XmppRegisterSupportHostRequestTest, Send) {
  // |iq_request| is freed by XmppRegisterSupportHostRequest.
  int64_t start_time = static_cast<int64_t>(base::Time::Now().ToDoubleT());

  auto request = std::make_unique<XmppRegisterSupportHostRequest>(kTestBotJid);
  request->StartRequest(&signal_strategy_, key_pair_, callback_.Get());

  XmlElement* sent_iq = nullptr;
  EXPECT_CALL(signal_strategy_, GetNextId()).WillOnce(Return(kStanzaId));
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&sent_iq), Return(true)));

  request->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  task_environment_.RunUntilIdle();

  // Verify format of the query.
  std::unique_ptr<XmlElement> stanza(sent_iq);
  ASSERT_TRUE(stanza != nullptr);

  EXPECT_EQ(stanza->Attr(jingle_xmpp::QName(std::string(), "to")),
            std::string(kTestBotJid));
  EXPECT_EQ(stanza->Attr(jingle_xmpp::QName(std::string(), "type")), "set");

  EXPECT_EQ(QName(kChromotingXmlNamespace, "register-support-host"),
            stanza->FirstElement()->Name());

  QName signature_tag(kChromotingXmlNamespace, "signature");
  XmlElement* signature = stanza->FirstElement()->FirstNamed(signature_tag);
  ASSERT_TRUE(signature != nullptr);
  EXPECT_TRUE(stanza->NextNamed(signature_tag) == nullptr);

  std::string time_str =
      signature->Attr(QName(kChromotingXmlNamespace, "time"));
  int64_t time;
  EXPECT_TRUE(base::StringToInt64(time_str, &time));
  int64_t now = static_cast<int64_t>(base::Time::Now().ToDoubleT());
  EXPECT_LE(start_time, time);
  EXPECT_GE(now, time);

  XmlElement* host_version = stanza->FirstElement()->FirstNamed(
      QName(kChromotingXmlNamespace, "host-version"));
  EXPECT_EQ(STRINGIZE(VERSION), host_version->BodyText());

  XmlElement* host_os_name = stanza->FirstElement()->FirstNamed(
      QName(kChromotingXmlNamespace, "host-os-name"));
  EXPECT_EQ(GetHostOperatingSystemName(), host_os_name->BodyText());

  XmlElement* host_os_version = stanza->FirstElement()->FirstNamed(
      QName(kChromotingXmlNamespace, "host-os-version"));
  EXPECT_EQ(GetHostOperatingSystemVersion(), host_os_version->BodyText());

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  ASSERT_TRUE(key_pair.get());

  std::string expected_signature =
      key_pair->SignMessage(std::string(kTestJidNormalized) + ' ' + time_str);
  EXPECT_EQ(expected_signature, signature->BodyText());

  // Generate response and verify that callback is called.
  EXPECT_CALL(callback_, Run(kSupportId, base::TimeDelta::FromSeconds(300),
                             ErrorCode::OK));

  std::unique_ptr<XmlElement> response(new XmlElement(jingle_xmpp::QN_IQ));
  response->AddAttr(QName(std::string(), "from"), kTestBotJid);
  response->AddAttr(QName(std::string(), "type"), "result");
  response->AddAttr(QName(std::string(), "id"), kStanzaId);

  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, "register-support-host-result"));
  response->AddElement(result);

  XmlElement* support_id =
      new XmlElement(QName(kChromotingXmlNamespace, "support-id"));
  support_id->AddText(kSupportId);
  result->AddElement(support_id);

  XmlElement* support_id_lifetime =
      new XmlElement(QName(kChromotingXmlNamespace, "support-id-lifetime"));
  support_id_lifetime->AddText(kSupportIdLifetime);
  result->AddElement(support_id_lifetime);

  int consumed = 0;
  for (auto& listener : signal_strategy_listeners_) {
    if (listener.OnSignalStrategyIncomingStanza(response.get()))
      consumed++;
  }
  EXPECT_EQ(1, consumed);

  task_environment_.RunUntilIdle();
}

}  // namespace remoting

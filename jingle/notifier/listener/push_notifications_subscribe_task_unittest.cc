// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_subscribe_task.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmpp/jid.h"

namespace jingle_xmpp {
class XmlElement;
}

namespace notifier {

class PushNotificationsSubscribeTaskTest : public testing::Test {
 public:
  PushNotificationsSubscribeTaskTest()
      : jid_("to@jid.com/test123"), task_id_("taskid") {
    EXPECT_NE(jid_.Str(), jid_.BareJid().Str());
  }

  PushNotificationsSubscribeTaskTest(
      const PushNotificationsSubscribeTaskTest&) = delete;
  PushNotificationsSubscribeTaskTest& operator=(
      const PushNotificationsSubscribeTaskTest&) = delete;

 protected:
  const jingle_xmpp::Jid jid_;
  const std::string task_id_;
};

TEST_F(PushNotificationsSubscribeTaskTest, MakeSubscriptionMessage) {
  SubscriptionList subscriptions;

  Subscription subscription;
  subscription.channel = "test_channel1";
  subscription.from = "from.test.com";
  subscriptions.push_back(subscription);
  subscription.channel = "test_channel2";
  subscription.from = "from.test2.com";
  subscriptions.push_back(subscription);
  std::unique_ptr<jingle_xmpp::XmlElement> message(
      PushNotificationsSubscribeTask::MakeSubscriptionMessage(subscriptions,
                                                              jid_, task_id_));
  std::string expected_xml_string =
      base::StringPrintf(
          "<cli:iq type=\"set\" to=\"%s\" id=\"%s\" "
                  "xmlns:cli=\"jabber:client\">"
            "<subscribe xmlns=\"google:push\">"
              "<item channel=\"test_channel1\" from=\"from.test.com\"/>"
              "<item channel=\"test_channel2\" from=\"from.test2.com\"/>"
            "</subscribe>"
          "</cli:iq>",
          jid_.BareJid().Str().c_str(), task_id_.c_str());

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

}  // namespace notifier


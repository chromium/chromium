// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_send_update_task.h"

#include <memory>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmpp/jid.h"

namespace jingle_xmpp {
class XmlElement;
}

namespace notifier {

class PushNotificationsSendUpdateTaskTest : public testing::Test {
 public:
  PushNotificationsSendUpdateTaskTest() : to_jid_bare_("to@jid.com") {
    EXPECT_EQ(to_jid_bare_.Str(), to_jid_bare_.BareJid().Str());
  }

  PushNotificationsSendUpdateTaskTest(
      const PushNotificationsSendUpdateTaskTest&) = delete;
  PushNotificationsSendUpdateTaskTest& operator=(
      const PushNotificationsSendUpdateTaskTest&) = delete;

 protected:
  const jingle_xmpp::Jid to_jid_bare_;
};

TEST_F(PushNotificationsSendUpdateTaskTest, MakeUpdateMessage) {
  Notification notification;
  notification.channel = "test_channel";
  notification.data = "test_data";

  std::string base64_data;
  base::Base64Encode(notification.data, &base64_data);

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      PushNotificationsSendUpdateTask::MakeUpdateMessage(notification,
                                                         to_jid_bare_));

  std::string expected_xml_string =
      base::StringPrintf(
          "<cli:message to=\"%s\" type=\"headline\" "
              "xmlns:cli=\"jabber:client\">"
            "<push xmlns=\"google:push\" channel=\"%s\">"
              "<data xmlns=\"google:push\">%s</data>"
            "</push>"
          "</cli:message>",
          to_jid_bare_.Str().c_str(), notification.channel.c_str(),
          base64_data.c_str());
  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

}  // namespace notifier

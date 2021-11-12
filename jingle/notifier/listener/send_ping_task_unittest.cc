// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/send_ping_task.h"

#include <memory>

#include "base/base64.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmpp/jid.h"

namespace jingle_xmpp {
class XmlElement;
}

namespace notifier {

class SendPingTaskTest : public testing::Test {
 public:
  SendPingTaskTest() {}

  SendPingTaskTest(const SendPingTaskTest&) = delete;
  SendPingTaskTest& operator=(const SendPingTaskTest&) = delete;
};

TEST_F(SendPingTaskTest, MakePingStanza) {
  std::string task_id = "42";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      SendPingTask::MakePingStanza(task_id));

  std::string expected_xml_string("<cli:iq type=\"get\" id=\"");
  expected_xml_string += task_id;
  expected_xml_string +=
      "\" xmlns:cli=\"jabber:client\">"
      "<ping:ping xmlns:ping=\"urn:xmpp:ping\"/></cli:iq>";

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

}  // namespace notifier

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/xmpp_log_to_server.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/signaling/mock_signal_strategy.h"
#include "remoting/signaling/server_log_entry_unittest.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;
using testing::_;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::Return;

namespace remoting {

namespace {

const char kTestBotJid[] = "remotingunittest@bot.talk.google.com";
const char kClientJid[] = "host@domain.com/1234";

MATCHER_P2(IsLogEntry, key, value, "") {
  XmlElement* entry = GetSingleLogEntryFromStanza(arg);
  if (!entry) {
    return false;
  }

  return entry->Attr(QName(std::string(), key)) == value;
}

}  // namespace

class XmppLogToServerTest : public testing::Test {
 public:
  XmppLogToServerTest() : signal_strategy_(SignalingAddress(kClientJid)) {}
  void SetUp() override {
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, RemoveListener(_));
    xmpp_log_to_server_ = std::make_unique<XmppLogToServer>(
        ServerLogEntry::ME2ME, &signal_strategy_, kTestBotJid);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  MockSignalStrategy signal_strategy_;
  std::unique_ptr<XmppLogToServer> xmpp_log_to_server_;
};

TEST_F(XmppLogToServerTest, LogWhenConnected) {
  {
    InSequence s;
    EXPECT_CALL(signal_strategy_, AddListener(_));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsLogEntry("a", "1")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, GetNextId());
    EXPECT_CALL(signal_strategy_, SendStanzaPtr(IsLogEntry("b", "2")))
        .WillOnce(DoAll(DeleteArg<0>(), Return(true)));
    EXPECT_CALL(signal_strategy_, RemoveListener(_)).RetiresOnSaturation();
  }

  ServerLogEntry entry1;
  ServerLogEntry entry2;
  entry1.Set("a", "1");
  entry2.Set("b", "2");
  xmpp_log_to_server_->Log(entry1);
  xmpp_log_to_server_->OnSignalStrategyStateChange(SignalStrategy::CONNECTED);
  xmpp_log_to_server_->Log(entry2);
  run_loop_.RunUntilIdle();
}

TEST_F(XmppLogToServerTest, DontLogWhenDisconnected) {
  EXPECT_CALL(signal_strategy_, SendStanzaPtr(_)).Times(0);

  ServerLogEntry entry;
  entry.Set("foo", "bar");
  xmpp_log_to_server_->Log(entry);
  run_loop_.RunUntilIdle();
}

}  // namespace remoting

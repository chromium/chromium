// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_proto_converter.h"

#include <string>

#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char kFromLocalId[] = "from_user@gmail.com";
const char kFromRegistrationId[] = "from_registration_id";
const char kToLocalId[] = "to_user@gmail.com";
const char kToRegistrationId[] = "to_registration_id";
const char kMessageId[] = "test_message_id";
const char kSid[] = "test_sid";
}  // namespace

class JingleMessageProtoConverterTest : public testing::Test {
 public:
  void SetUp() override {
    from_address_ = SignalingAddress::CreateFtlSignalingAddress(
        kFromLocalId, kFromRegistrationId);
    to_address_ = SignalingAddress::CreateFtlSignalingAddress(
        kToLocalId, kToRegistrationId);
  }

 protected:
  SignalingAddress from_address_;
  SignalingAddress to_address_;
};

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInitiate) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  message.SetPayload(SessionInitiate());

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_EQ(stanza.id(), kMessageId);
  EXPECT_EQ(stanza.sender().local_part(), kFromLocalId);
  EXPECT_EQ(stanza.sender().resource_part(), kFromRegistrationId);
  EXPECT_EQ(stanza.receiver().local_part(), kToLocalId);
  EXPECT_EQ(stanza.receiver().resource_part(), kToRegistrationId);

  EXPECT_TRUE(stanza.has_jingle());
  EXPECT_EQ(stanza.jingle().session_id(), kSid);
  EXPECT_TRUE(stanza.jingle().has_session_initiate());

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  EXPECT_EQ(converted_message.message_id, kMessageId);
  EXPECT_EQ(converted_message.from, from_address_);
  EXPECT_EQ(converted_message.to, to_address_);
  EXPECT_EQ(converted_message.sid, kSid);
  EXPECT_TRUE(
      std::holds_alternative<SessionInitiate>(converted_message.payload()));
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionTerminate) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  SessionTerminate terminate;
  terminate.reason = SessionTerminate::Reason::kDecline;
  terminate.error_code = "PEER_IS_OFFLINE";
  terminate.error_details = "The peer is offline.";
  message.SetPayload(std::move(terminate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_terminate());
  EXPECT_EQ(stanza.jingle().session_terminate().reason(),
            ftl::SessionTerminate::DECLINE);
  EXPECT_EQ(stanza.jingle().session_terminate().error_code(),
            "PEER_IS_OFFLINE");
  EXPECT_EQ(stanza.jingle().session_terminate().error_details(),
            "The peer is offline.");

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  auto* converted_terminate =
      std::get_if<SessionTerminate>(&converted_message.payload());
  ASSERT_TRUE(converted_terminate);
  EXPECT_EQ(converted_terminate->reason, SessionTerminate::Reason::kDecline);
  EXPECT_EQ(converted_terminate->error_code, "PEER_IS_OFFLINE");
  EXPECT_EQ(converted_terminate->error_details, "The peer is offline.");
}

TEST_F(JingleMessageProtoConverterTest, ConvertReplyResult) {
  JingleMessageReply reply;
  reply.from = from_address_;
  reply.to = to_address_;
  reply.message_id = kMessageId;
  reply.reply_type = JingleMessageReply::REPLY_RESULT;

  ftl::IqStanza stanza = reply.ToFtlIqStanza();

  EXPECT_EQ(stanza.id(), kMessageId);
  EXPECT_TRUE(stanza.has_reply());
  EXPECT_EQ(stanza.sender().local_part(), kFromLocalId);
  EXPECT_EQ(stanza.receiver().local_part(), kToLocalId);

  JingleMessageReply converted_reply;
  ASSERT_TRUE(JingleMessageReplyFromProto(stanza, &converted_reply));
  EXPECT_EQ(converted_reply.message_id, kMessageId);
  EXPECT_EQ(converted_reply.from, from_address_);
  EXPECT_EQ(converted_reply.to, to_address_);
  EXPECT_EQ(converted_reply.reply_type, JingleMessageReply::REPLY_RESULT);
}

TEST_F(JingleMessageProtoConverterTest, ConvertReplyError) {
  JingleMessageReply reply;
  reply.from = from_address_;
  reply.to = to_address_;
  reply.message_id = kMessageId;
  reply.reply_type = JingleMessageReply::REPLY_ERROR;
  reply.error_type = JingleMessageReply::INVALID_SID;
  reply.text = "Invalid session ID";

  ftl::IqStanza stanza = reply.ToFtlIqStanza();

  EXPECT_TRUE(stanza.has_error());
  EXPECT_EQ(stanza.error().condition(), ftl::ErrorStanza::INVALID_SID);
  EXPECT_EQ(stanza.error().text(), "Invalid session ID");

  JingleMessageReply converted_reply;
  ASSERT_TRUE(JingleMessageReplyFromProto(stanza, &converted_reply));
  EXPECT_EQ(converted_reply.reply_type, JingleMessageReply::REPLY_ERROR);
  EXPECT_EQ(converted_reply.error_type, JingleMessageReply::INVALID_SID);
  EXPECT_EQ(converted_reply.text, "Invalid session ID");
}

}  // namespace remoting

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_struct_converter.h"

#include <string>
#include <vector>

#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using ::testing::HasSubstr;

namespace {

const char kFromJid[] = "from@domain.com/resource";
const char kToJid[] = "to@domain.com/resource";

struct JidTestCase {
  std::string signaling_id;
  std::string local_part;
  std::string domain_part;
  std::string resource_part;
};

class JidConversionTest : public testing::TestWithParam<JidTestCase> {};

TEST_P(JidConversionTest, SignalingAddressToJabberIdStruct) {
  const auto& test_case = GetParam();
  SignalingAddress addr(test_case.signaling_id);
  auto jabber_id = SignalingAddressToJabberIdStruct(addr);
  EXPECT_EQ(jabber_id.local_part, test_case.local_part);
  EXPECT_EQ(jabber_id.domain_part, test_case.domain_part);
  EXPECT_EQ(jabber_id.resource_part, test_case.resource_part);
}

TEST_P(JidConversionTest, JabberIdStructToSignalingAddress) {
  const auto& test_case = GetParam();
  internal::JabberIdStruct jabber_id;
  jabber_id.local_part = test_case.local_part;
  jabber_id.domain_part = test_case.domain_part;
  jabber_id.resource_part = test_case.resource_part;
  auto addr = JabberIdStructToSignalingAddress(jabber_id);
  EXPECT_EQ(addr.id(), test_case.signaling_id);
}

INSTANTIATE_TEST_SUITE_P(
    JingleMessageStructConverterTest,
    JidConversionTest,
    testing::Values(JidTestCase{"user@domain.com/resource", "user",
                                "domain.com", "resource"},
                    JidTestCase{"user@domain.com", "user", "domain.com", ""},
                    JidTestCase{"user", "user", "", ""},
                    JidTestCase{"user/resource", "user", "", "resource"}));

TEST(JingleMessageStructConverterTest, SignalingAddressToJabberIdStruct_Empty) {
  SignalingAddress addr;
  auto jabber_id = SignalingAddressToJabberIdStruct(addr);
  EXPECT_TRUE(jabber_id.local_part.empty());
  EXPECT_TRUE(jabber_id.domain_part.empty());
  EXPECT_TRUE(jabber_id.resource_part.empty());
}

TEST(JingleMessageStructConverterTest, JabberIdStructToSignalingAddress_Empty) {
  internal::JabberIdStruct jabber_id;
  auto addr = JabberIdStructToSignalingAddress(jabber_id);
  EXPECT_TRUE(addr.empty());
}

TEST(JingleMessageStructConverterTest, JingleMessageConversion) {
  JingleMessage message;
  message.message_id = "test_msg_id";
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  message.sid = "test_sid";
  message.SetPayload(SessionInfo());

  auto stanza = JingleMessageToStruct(message);
  EXPECT_EQ(stanza.id, "test_msg_id");
  EXPECT_EQ(stanza.sender.local_part, "from");
  EXPECT_EQ(stanza.receiver.local_part, "to");
  EXPECT_THAT(stanza.xml, testing::HasSubstr("sid=\"test_sid\""));

  JingleMessage message2;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &message2, &error));
  EXPECT_EQ(message2.message_id, "test_msg_id");
  EXPECT_EQ(message2.from.id(), kFromJid);
  EXPECT_EQ(message2.to.id(), kToJid);
  EXPECT_EQ(message2.sid, "test_sid");
  EXPECT_TRUE(std::holds_alternative<SessionInfo>(message2.payload()));
}

TEST(JingleMessageStructConverterTest, JingleMessageReplyConversion) {
  JingleMessageReply reply;
  reply.message_id = "reply_id";
  reply.from = SignalingAddress(kFromJid);
  reply.to = SignalingAddress(kToJid);
  reply.reply_type = JingleMessageReply::REPLY_RESULT;

  auto stanza = JingleMessageReplyToStruct(reply);
  EXPECT_EQ(stanza.id, "reply_id");
  EXPECT_EQ(stanza.sender.local_part, "from");
  EXPECT_EQ(stanza.receiver.local_part, "to");
  EXPECT_THAT(stanza.xml, HasSubstr("type=\"result\""));

  JingleMessageReply reply2;
  ASSERT_TRUE(JingleMessageReplyFromStruct(stanza, &reply2));
  EXPECT_EQ(reply2.message_id, "reply_id");
  EXPECT_EQ(reply2.from.id(), kFromJid);
  EXPECT_EQ(reply2.to.id(), kToJid);
  EXPECT_EQ(reply2.reply_type, JingleMessageReply::REPLY_RESULT);
}

TEST(JingleMessageStructConverterTest,
     JingleMessageConversion_WithNestedFields) {
  JingleMessage message;
  message.message_id = "test_msg_id";
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  message.sid = "test_sid";
  message.initiator = "initiator@domain.com";

  SessionTerminate terminate;
  terminate.reason = SessionTerminate::Reason::kGeneralError;
  terminate.error_code = "ERR_CODE";
  terminate.error_details = "ERR_DETAILS";
  message.SetPayload(std::move(terminate));

  Attachment attachment;
  HostConfigAttachment host_config;
  host_config.settings["key"] = "value";
  attachment.host_config = std::move(host_config);
  message.attachments.push_back(std::move(attachment));

  auto stanza = JingleMessageToStruct(message);

  const auto* jingle_struct =
      std::get_if<internal::JingleMessageStruct>(&stanza.payload);
  ASSERT_TRUE(jingle_struct);
  EXPECT_EQ(jingle_struct->session_id, "test_sid");
  ASSERT_EQ(jingle_struct->attachments.size(), 1u);
  ASSERT_TRUE(jingle_struct->attachments[0].host_config.has_value());
  EXPECT_EQ(jingle_struct->attachments[0].host_config->settings.at("key"),
            "value");

  const auto* action =
      std::get_if<internal::SessionTerminateStruct>(&jingle_struct->action);
  ASSERT_TRUE(action);
  EXPECT_EQ(action->reason,
            internal::SessionTerminateStruct::Reason::kGeneralError);
  EXPECT_EQ(action->error_code, "ERR_CODE");

  JingleMessage message2;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &message2, &error));
  EXPECT_EQ(message2.sid, "test_sid");
  ASSERT_EQ(message2.attachments.size(), 1u);
  ASSERT_TRUE(message2.attachments[0].host_config.has_value());
  EXPECT_EQ(message2.attachments[0].host_config->settings.at("key"), "value");

  const auto* terminate2 = std::get_if<SessionTerminate>(&message2.payload());
  ASSERT_TRUE(terminate2);
  EXPECT_EQ(terminate2->reason, SessionTerminate::Reason::kGeneralError);
  EXPECT_EQ(terminate2->error_code, "ERR_CODE");
}

TEST(JingleMessageStructConverterTest, JingleMessageReplyConversion_Error) {
  JingleMessageReply reply;
  reply.from = SignalingAddress(kFromJid);
  reply.to = SignalingAddress(kToJid);
  reply.message_id = "reply_id";
  reply.reply_type = JingleMessageReply::REPLY_ERROR;
  reply.error_type = JingleMessageReply::BAD_REQUEST;
  reply.text = "Bad Request Details";

  auto stanza = JingleMessageReplyToStruct(reply);
  const auto* error_struct =
      std::get_if<internal::ErrorStanzaStruct>(&stanza.payload);
  ASSERT_TRUE(error_struct);
  EXPECT_EQ(error_struct->condition,
            internal::ErrorStanzaStruct::Condition::kBadRequest);
  EXPECT_EQ(error_struct->text, "Bad Request Details");

  JingleMessageReply reply2;
  ASSERT_TRUE(JingleMessageReplyFromStruct(stanza, &reply2));
  EXPECT_EQ(reply2.reply_type, JingleMessageReply::REPLY_ERROR);
  EXPECT_EQ(reply2.error_type, JingleMessageReply::BAD_REQUEST);
  EXPECT_EQ(reply2.text, "Bad Request Details");
}

TEST(JingleMessageStructConverterTest, SessionInitiateConversion) {
  JingleMessage message;
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  message.message_id = "init_id";
  message.initiator = "initiator@domain.com";

  SessionInitiate initiate;
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519};
  auth.session_authz_host_token = "host_token";
  initiate.authentication = std::move(auth);
  message.SetPayload(std::move(initiate));

  auto stanza = JingleMessageToStruct(message);
  EXPECT_EQ(stanza.id, "init_id");

  const auto* jingle_struct =
      std::get_if<internal::JingleMessageStruct>(&stanza.payload);
  ASSERT_TRUE(jingle_struct);
  const auto* action =
      std::get_if<internal::SessionInitiateStruct>(&jingle_struct->action);
  ASSERT_TRUE(action);
  EXPECT_EQ(action->initiator.local_part, "initiator");
  ASSERT_TRUE(action->authentication.has_value());
  EXPECT_EQ(action->authentication->session_authz_host_token, "host_token");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &converted, &error));
  EXPECT_EQ(converted.initiator, "initiator@domain.com");
  const auto* converted_initiate =
      std::get_if<SessionInitiate>(&converted.payload());
  ASSERT_TRUE(converted_initiate);
  ASSERT_TRUE(converted_initiate->authentication.has_value());
  EXPECT_EQ(converted_initiate->authentication->session_authz_host_token,
            "host_token");
}

TEST(JingleMessageStructConverterTest, SessionAcceptConversion) {
  JingleMessage message;
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  message.message_id = "accept_id";
  message.sid = "sid";

  SessionAccept accept;
  JingleAuthentication auth;
  auth.method = AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
  auth.session_authz_session_token = "session_token";
  accept.authentication = std::move(auth);
  message.SetPayload(std::move(accept));

  auto stanza = JingleMessageToStruct(message);
  EXPECT_EQ(stanza.id, "accept_id");

  const auto* jingle_struct =
      std::get_if<internal::JingleMessageStruct>(&stanza.payload);
  ASSERT_TRUE(jingle_struct);
  const auto* action =
      std::get_if<internal::SessionAcceptStruct>(&jingle_struct->action);
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->authentication.has_value());
  EXPECT_EQ(action->authentication->session_authz_session_token,
            "session_token");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &converted, &error));
  const auto* converted_accept =
      std::get_if<SessionAccept>(&converted.payload());
  ASSERT_TRUE(converted_accept);
  ASSERT_TRUE(converted_accept->authentication.has_value());
  EXPECT_EQ(converted_accept->authentication->session_authz_session_token,
            "session_token");
}

TEST(JingleMessageStructConverterTest, JingleTransportInfoConversion) {
  JingleMessage message;
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  message.sid = "sid";

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kOffer;
  sdp.sdp = "v=0\r\no=-...";
  sdp.signature = {0x01, 0x02, 0x03};
  transport.session_description = std::move(sdp);

  IceTransportInfo::NamedCandidate candidate;
  candidate.name = "audio";
  candidate.sdp_m_line_index = 0;
  // Use a valid candidate that ParseCandidateString can handle.
  candidate.candidate =
      webrtc::Candidate(1, "udp", webrtc::SocketAddress("1.2.3.4", 5678), 100,
                        "u", "p", webrtc::IceCandidateType::kHost, 0, "f");

  transport.candidates.push_back(std::move(candidate));

  message.SetPayload(std::move(transport));

  auto stanza = JingleMessageToStruct(message);
  const auto* jingle_struct =
      std::get_if<internal::JingleMessageStruct>(&stanza.payload);
  ASSERT_TRUE(jingle_struct);
  const auto* action =
      std::get_if<internal::TransportInfoStruct>(&jingle_struct->action);
  ASSERT_TRUE(action);
  ASSERT_TRUE(action->session_description.has_value());
  EXPECT_EQ(action->session_description->type,
            internal::SessionDescriptionStruct::SdpType::kOffer);
  ASSERT_EQ(action->candidates.size(), 1u);
  EXPECT_EQ(action->candidates[0].sdp_mid, "audio");
  EXPECT_THAT(action->candidates[0].candidate, HasSubstr("1.2.3.4"));

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &converted, &error));
  const auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted.payload());
  ASSERT_TRUE(converted_transport);
  ASSERT_TRUE(converted_transport->session_description.has_value());
  EXPECT_EQ(converted_transport->session_description->type,
            SessionDescription::Type::kOffer);
  EXPECT_EQ(converted_transport->session_description->signature,
            std::vector<uint8_t>({0x01, 0x02, 0x03}));
  ASSERT_EQ(converted_transport->candidates.size(), 1u);
  EXPECT_EQ(converted_transport->candidates[0].name, "audio");
  EXPECT_EQ(converted_transport->candidates[0]
                .candidate.address()
                .ipaddr()
                .ToString(),
            "1.2.3.4");
}

TEST(JingleMessageStructConverterTest, HostAttributesAttachmentConversion) {
  JingleMessage message;
  message.from = SignalingAddress(kFromJid);
  message.to = SignalingAddress(kToJid);
  Attachment attachment;
  HostAttributesAttachment host_attr;
  host_attr.attribute = {"attr1", "attr2"};
  attachment.host_attributes = std::move(host_attr);
  message.attachments.push_back(std::move(attachment));
  message.SetPayload(SessionInfo());

  auto stanza = JingleMessageToStruct(message);
  const auto* jingle_struct =
      std::get_if<internal::JingleMessageStruct>(&stanza.payload);
  ASSERT_TRUE(jingle_struct);
  ASSERT_EQ(jingle_struct->attachments.size(), 1u);
  ASSERT_TRUE(jingle_struct->attachments[0].host_attributes.has_value());
  EXPECT_THAT(jingle_struct->attachments[0].host_attributes->attribute,
              testing::ElementsAre("attr1", "attr2"));

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &converted, &error));
  ASSERT_EQ(converted.attachments.size(), 1u);
  ASSERT_TRUE(converted.attachments[0].host_attributes.has_value());
  EXPECT_THAT(converted.attachments[0].host_attributes->attribute,
              testing::ElementsAre("attr1", "attr2"));
}

TEST(JingleMessageStructConverterTest, FallbackToXmlParsing) {
  internal::IqStanzaStruct stanza;
  stanza.id = "xml_id";
  // Raw XML for a basic SessionInfo message.
  stanza.xml =
      "<iq xmlns=\"jabber:client\" id=\"xml_id\" type=\"set\" "
      "from=\"from@domain.com\" to=\"to@domain.com\">"
      "<jingle xmlns=\"urn:xmpp:jingle:1\" action=\"session-info\" "
      "sid=\"xml_sid\"/></iq>";
  // payload is std::monostate by default.

  JingleMessage message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromStruct(stanza, &message, &error)) << error;
  EXPECT_EQ(message.message_id, "xml_id");
  EXPECT_EQ(message.sid, "xml_sid");
  EXPECT_TRUE(std::holds_alternative<SessionInfo>(message.payload()));
}

}  // namespace
}  // namespace remoting

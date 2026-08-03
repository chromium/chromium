// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_proto_converter.h"

#include <string>

#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/jsep.h"

namespace remoting {

namespace {
const char kFromLocalId[] = "from_user@gmail.com";
const char kFromRegistrationId[] = "from_registration_id";
const char kToLocalId[] = "to_user@gmail.com";
const char kToRegistrationId[] = "to_registration_id";
const char kMessageId[] = "test_message_id";
const char kSid[] = "test_sid";

JingleAuthentication CreateTestAuthentication() {
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519};
  auth.method = AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
  auth.spake_message = {1, 2, 3, 4};
  auth.verification_hash = {5, 6, 7, 8};
  auth.session_authz_host_token = "host_token";
  auth.session_authz_session_token = "session_token";
  return auth;
}

void VerifyAuthentication(const JingleAuthentication& actual,
                          const JingleAuthentication& expected) {
  EXPECT_EQ(actual.supported_methods, expected.supported_methods);
  EXPECT_EQ(actual.method, expected.method);
  EXPECT_EQ(actual.spake_message, expected.spake_message);
  EXPECT_EQ(actual.verification_hash, expected.verification_hash);
  EXPECT_EQ(actual.session_authz_host_token, expected.session_authz_host_token);
  EXPECT_EQ(actual.session_authz_session_token,
            expected.session_authz_session_token);
}
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

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInitiateWithAuth) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  message.initiator = kFromLocalId;
  SessionInitiate initiate;
  JingleAuthentication auth = CreateTestAuthentication();
  initiate.authentication = auth;
  message.SetPayload(std::move(initiate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_initiate());
  EXPECT_EQ(stanza.jingle().session_initiate().initiator().local_part(),
            kFromLocalId);
  EXPECT_TRUE(stanza.jingle().session_initiate().has_authentication());

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  EXPECT_EQ(converted_message.initiator, kFromLocalId);
  auto* converted_initiate =
      std::get_if<SessionInitiate>(&converted_message.payload());
  ASSERT_TRUE(converted_initiate);
  ASSERT_TRUE(converted_initiate->authentication.has_value());
  VerifyAuthentication(*converted_initiate->authentication, auth);

  ASSERT_TRUE(converted_message.description);
  VerifyAuthentication(converted_message.description->authentication(), auth);
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionAcceptWithAuth) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  SessionAccept accept;
  JingleAuthentication auth = CreateTestAuthentication();
  accept.authentication = auth;
  message.SetPayload(std::move(accept));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_accept());
  EXPECT_TRUE(stanza.jingle().session_accept().has_authentication());

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  auto* converted_accept =
      std::get_if<SessionAccept>(&converted_message.payload());
  ASSERT_TRUE(converted_accept);
  ASSERT_TRUE(converted_accept->authentication.has_value());
  VerifyAuthentication(*converted_accept->authentication, auth);

  ASSERT_TRUE(converted_message.description);
  VerifyAuthentication(converted_message.description->authentication(), auth);
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInfoWithAuth) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  SessionInfo session_info;
  JingleAuthentication auth = CreateTestAuthentication();
  session_info.authentication = auth;
  message.SetPayload(std::move(session_info));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_info());
  EXPECT_TRUE(stanza.jingle().session_info().has_authentication());

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  auto* converted_session_info =
      std::get_if<SessionInfo>(&converted_message.payload());
  ASSERT_TRUE(converted_session_info);
  ASSERT_TRUE(converted_session_info->authentication.has_value());
  VerifyAuthentication(*converted_session_info->authentication, auth);
}

TEST_F(JingleMessageProtoConverterTest, ConvertTransportInfo) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kOffer;
  sdp.sdp = "test_sdp";
  sdp.signature = {9, 10, 11};
  transport.session_description = sdp;

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
      webrtc::IceCandidate::Create(
          "audio", 0,
          "candidate:842163049 1 udp 16777215 127.0.0.1 12345 typ host",
          &error);
  ASSERT_TRUE(webrtc_candidate) << error.description;
  IceTransportInfo::NamedCandidate candidate("audio",
                                             webrtc_candidate->candidate(), 0);
  transport.candidates.push_back(candidate);

  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_transport_info());
  EXPECT_TRUE(stanza.jingle().transport_info().has_session_description());
  EXPECT_EQ(stanza.jingle().transport_info().session_description().sdp(),
            "test_sdp");
  EXPECT_EQ(stanza.jingle().transport_info().candidates_size(), 1);

  JingleMessage converted_message;
  std::string conversion_error;
  ASSERT_TRUE(
      JingleMessageFromProto(stanza, &converted_message, &conversion_error))
      << conversion_error;
  auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted_message.payload());
  ASSERT_TRUE(converted_transport);
  ASSERT_TRUE(converted_transport->session_description.has_value());
  EXPECT_EQ(converted_transport->session_description->sdp, "test_sdp");
  EXPECT_EQ(converted_transport->session_description->type,
            SessionDescription::Type::kOffer);
  EXPECT_EQ(converted_transport->session_description->signature, sdp.signature);

  ASSERT_EQ(converted_transport->candidates.size(), 1u);
  EXPECT_EQ(converted_transport->candidates[0].name, "audio");
  EXPECT_EQ(*converted_transport->candidates[0].sdp_m_line_index, 0);
  EXPECT_EQ(converted_transport->candidates[0].candidate.protocol(), "udp");
  EXPECT_EQ(converted_transport->candidates[0]
                .candidate.address()
                .ipaddr()
                .ToString(),
            "127.0.0.1");
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertTransportInfoWithMalformedCandidates) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kOffer;
  sdp.sdp = "test_sdp";
  transport.session_description = sdp;

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
      webrtc::IceCandidate::Create(
          "audio", 0,
          "candidate:842163049 1 udp 16777215 127.0.0.1 12345 typ host",
          &error);
  ASSERT_TRUE(webrtc_candidate) << error.description;
  IceTransportInfo::NamedCandidate candidate("audio",
                                             webrtc_candidate->candidate(), 0);
  transport.candidates.push_back(candidate);

  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  // Now manually add malformed candidates to the proto stanza.
  auto* jingle_stanza = stanza.mutable_jingle();
  auto* transport_info = jingle_stanza->mutable_transport_info();

  // 1. Incomplete candidate (missing sdp_m_line_index)
  auto* incomplete_candidate = transport_info->add_candidates();
  incomplete_candidate->set_sdp_mid("video");
  incomplete_candidate->set_candidate(
      "candidate:842163049 1 udp 16777215 127.0.0.1 12346 typ host");

  // 2. Malformed candidate (invalid candidate string)
  auto* malformed_candidate = transport_info->add_candidates();
  malformed_candidate->set_sdp_mid("video");
  malformed_candidate->set_sdp_m_line_index(1);
  malformed_candidate->set_candidate("invalid candidate string");

  JingleMessage converted_message;
  std::string conversion_error;
  ASSERT_TRUE(
      JingleMessageFromProto(stanza, &converted_message, &conversion_error))
      << conversion_error;

  auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted_message.payload());
  ASSERT_TRUE(converted_transport);

  // Should only have the one valid candidate.
  ASSERT_EQ(converted_transport->candidates.size(), 1u);
  EXPECT_EQ(converted_transport->candidates[0].name, "audio");
  EXPECT_EQ(*converted_transport->candidates[0].sdp_m_line_index, 0);
}

}  // namespace remoting

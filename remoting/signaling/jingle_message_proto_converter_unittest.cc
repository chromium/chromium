// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_proto_converter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/api/jsep.h"

namespace remoting {

namespace {
const char kFromLocalId[] = "from_user@gmail.com";
const char kFromRegistrationId[] = "from_registration_id";
const char kToLocalId[] = "to_user@gmail.com";
const char kToRegistrationId[] = "to_registration_id";
const char kMessageId[] = "test_message_id";
const char kSid[] = "test_sid";

constexpr char kTestSenderLocal[] = "user";
constexpr char kTestSenderDomain[] = "test_domain.com";
constexpr char kTestSenderEmail[] = "user@test_domain.com";
constexpr char kTestSenderRegistration[] =
    "00000000-1111-2222-3333-444444444444";

constexpr char kTestReceiverLocal[] = "host";
constexpr char kTestReceiverDomain[] = "robot_domain.com";
constexpr char kTestReceiverEmail[] = "host@robot_domain.com";
constexpr char kTestReceiverRegistration[] =
    "00000000-1111-2222-3333-555555555555";
constexpr char kSessionAuthzHostToken[] = "aG9zdF90b2tlbl9zYW1wbGU=";
constexpr char kSessionAuthzSessionToken[] = "c2Vzc2lvbl90b2tlbl9zYW1wbGU=";

constexpr char kRealisticBundleSdp[] =
    "v=0\r\n"
    "o=- 4123456789012345 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1 2\r\n"
    "a=msid-semantic: WMS\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:UFrG\r\n"
    "a=ice-pwd:PassWord1234567890ABCDEF\r\n"
    "a=fingerprint:sha-256 "
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
    "a=setup:actpass\r\n"
    "a=mid:0\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:UFrG\r\n"
    "a=ice-pwd:PassWord1234567890ABCDEF\r\n"
    "a=fingerprint:sha-256 "
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
    "a=setup:actpass\r\n"
    "a=mid:1\r\n"
    "a=rtpmap:96 VP9/90000\r\n"
    "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:UFrG\r\n"
    "a=ice-pwd:PassWord1234567890ABCDEF\r\n"
    "a=fingerprint:sha-256 "
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
    "a=mid:2\r\n"
    "a=sctp-port:5000\r\n"
    "a=max-message-size:262144\r\n";

JingleAuthentication CreateTestAuthentication() {
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519};
  auth.method = AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
  auth.spake_message = std::vector<uint8_t>(32, 0x01);
  auth.verification_hash = std::vector<uint8_t>(32, 0xAA);
  auth.certificate = std::vector<uint8_t>(32, 0xBB);
  auth.session_authz_host_token = kSessionAuthzHostToken;
  auth.session_authz_session_token = kSessionAuthzSessionToken;
  JingleAuthentication::PairingInfo pairing_info;
  pairing_info.client_id = "paired_client_uuid_abc123";
  auth.pairing_info = std::move(pairing_info);
  return auth;
}

void VerifyAuthentication(const JingleAuthentication& actual,
                          const JingleAuthentication& expected) {
  EXPECT_EQ(actual.supported_methods, expected.supported_methods);
  EXPECT_EQ(actual.method, expected.method);
  EXPECT_EQ(actual.spake_message, expected.spake_message);
  EXPECT_EQ(actual.verification_hash, expected.verification_hash);
  EXPECT_EQ(actual.certificate, expected.certificate);
  EXPECT_EQ(actual.session_authz_host_token, expected.session_authz_host_token);
  EXPECT_EQ(actual.session_authz_session_token,
            expected.session_authz_session_token);
  if (expected.pairing_info) {
    ASSERT_TRUE(actual.pairing_info.has_value());
    EXPECT_EQ(actual.pairing_info->client_id, expected.pairing_info->client_id);
  } else {
    EXPECT_FALSE(actual.pairing_info.has_value());
  }
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
  EXPECT_EQ(stanza.sender().local_part(), "from_user");
  EXPECT_EQ(stanza.sender().domain_part(), "gmail.com");
  EXPECT_EQ(stanza.sender().resource_part(), kFromRegistrationId);
  EXPECT_EQ(stanza.receiver().local_part(), "to_user");
  EXPECT_EQ(stanza.receiver().domain_part(), "gmail.com");
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

TEST_F(JingleMessageProtoConverterTest,
       ConvertSessionInitiate_CorpSessionAuthz) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_initiate_001";
  message.sid = "crd_sess_987654321";
  message.initiator = kFromLocalId;

  SessionInitiate initiate;
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519,
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519};
  auth.session_authz_host_token =
      "aG9zdF9hdXRoel90b2tlbl9leGFtcGxlXzEyMzQ1Njc4OTA=";
  initiate.authentication = auth;
  message.description = std::make_unique<ContentDescription>(auth);

  Attachment attachment;
  HostConfigAttachment host_config;
  host_config.settings["Av1-Encoder-Speed"] = "11";
  host_config.settings["VideoCodecPreset"] = "high_quality";
  host_config.settings["EnableMultimon"] = "true";
  attachment.host_config = std::move(host_config);
  message.attachments.push_back(std::move(attachment));
  message.SetPayload(std::move(initiate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_initiate());
  EXPECT_THAT(
      stanza.jingle().session_initiate().authentication().supported_methods(),
      testing::ElementsAre(
          ftl::AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519,
          ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519));
  EXPECT_EQ(stanza.jingle()
                .session_initiate()
                .authentication()
                .session_authz_host_token(),
            "host_authz_token_example_1234567890");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_initiate = std::get_if<SessionInitiate>(&converted.payload());
  ASSERT_TRUE(converted_initiate);
  ASSERT_TRUE(converted_initiate->authentication.has_value());
  EXPECT_EQ(converted_initiate->authentication->session_authz_host_token,
            auth.session_authz_host_token);
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInitiate_StandardPin) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_initiate_002a";
  message.sid = "crd_sess_987654321";
  message.initiator = kFromLocalId;

  SessionInitiate initiate;
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519,
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519};
  initiate.authentication = auth;
  message.description = std::make_unique<ContentDescription>(auth);
  message.SetPayload(std::move(initiate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_initiate());
  EXPECT_THAT(
      stanza.jingle().session_initiate().authentication().supported_methods(),
      testing::ElementsAre(
          ftl::AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519,
          ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519));

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_initiate = std::get_if<SessionInitiate>(&converted.payload());
  ASSERT_TRUE(converted_initiate);
  ASSERT_TRUE(converted_initiate->authentication.has_value());
  EXPECT_THAT(
      converted_initiate->authentication->supported_methods,
      testing::ElementsAre(
          AuthenticationMethod::PAIRED_SPAKE2_CURVE25519,
          AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519));
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInitiate_PairedSpake) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_initiate_002b";
  message.sid = "crd_sess_987654321";
  message.initiator = kFromLocalId;

  SessionInitiate initiate;
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519,
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519};
  JingleAuthentication::PairingInfo pairing_info;
  pairing_info.client_id = "paired_client_uuid_abc123";
  auth.pairing_info = std::move(pairing_info);
  initiate.authentication = auth;
  message.description = std::make_unique<ContentDescription>(auth);
  message.SetPayload(std::move(initiate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_initiate());
  EXPECT_THAT(
      stanza.jingle().session_initiate().authentication().supported_methods(),
      testing::ElementsAre(ftl::AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519,
                           ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519));
  ASSERT_TRUE(
      stanza.jingle().session_initiate().authentication().has_pairing_info());
  EXPECT_EQ(stanza.jingle()
                .session_initiate()
                .authentication()
                .pairing_info()
                .client_id(),
            "paired_client_uuid_abc123");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_initiate = std::get_if<SessionInitiate>(&converted.payload());
  ASSERT_TRUE(converted_initiate);
  ASSERT_TRUE(converted_initiate->authentication.has_value());
  EXPECT_THAT(converted_initiate->authentication->supported_methods,
              testing::ElementsAre(
                  AuthenticationMethod::PAIRED_SPAKE2_CURVE25519,
                  AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519));
  ASSERT_TRUE(converted_initiate->authentication->pairing_info.has_value());
  EXPECT_EQ(converted_initiate->authentication->pairing_info->client_id,
            "paired_client_uuid_abc123");
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionAccept_CorpSessionAuthz) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_accept_003a";
  message.sid = "crd_sess_987654321";

  SessionAccept accept;
  JingleAuthentication auth;
  auth.method = AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
  auth.session_authz_session_token =
      "c2Vzc2lvbl9hdXRoel90b2tlbl9leGFtcGxlXzA5ODc2NTQzMjE=";
  auth.spake_message = std::vector<uint8_t>(32, 0x02);
  accept.authentication = auth;
  message.description = std::make_unique<ContentDescription>(auth);

  Attachment attachment;
  HostAttributesAttachment host_attributes;
  host_attributes.attribute = {"Debug-Build", "HWEncoder",
                               "SupportsIceDatagramTransport"};
  attachment.host_attributes = std::move(host_attributes);
  message.attachments.push_back(std::move(attachment));
  message.SetPayload(std::move(accept));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_accept());
  EXPECT_EQ(stanza.jingle().session_accept().authentication().method(),
            ftl::AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519);
  EXPECT_EQ(stanza.jingle().session_accept().authentication().spake_message(),
            std::string(32, '\x02'));
  EXPECT_EQ(stanza.jingle()
                .session_accept()
                .authentication()
                .session_authz_session_token(),
            "session_authz_token_example_0987654321");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_accept = std::get_if<SessionAccept>(&converted.payload());
  ASSERT_TRUE(converted_accept);
  ASSERT_TRUE(converted_accept->authentication.has_value());
  EXPECT_EQ(converted_accept->authentication->spake_message,
            auth.spake_message);
  EXPECT_EQ(converted_accept->authentication->session_authz_session_token,
            auth.session_authz_session_token);
  ASSERT_EQ(converted.attachments.size(), 1u);
  ASSERT_TRUE(converted.attachments[0].host_attributes.has_value());
  EXPECT_THAT(converted.attachments[0].host_attributes->attribute,
              testing::ElementsAre("Debug-Build", "HWEncoder",
                                   "SupportsIceDatagramTransport"));
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertSessionAccept_Spake2HostCertificate) {
  constexpr char kTestCertificateBase64[] =
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyN3rL6u+eG8H9o3F7w4f1mK2s8u1"
      "v9X0y5z6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8"
      "c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4"
      "c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0"
      "c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9IDAQAB";
  std::string decoded_cert;
  ASSERT_TRUE(base::Base64Decode(kTestCertificateBase64, &decoded_cert));

  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_accept_003b";
  message.sid = "crd_sess_987654321";

  SessionAccept accept;
  JingleAuthentication auth;
  auth.method = AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
  auth.certificate.assign(decoded_cert.begin(), decoded_cert.end());
  auth.spake_message = std::vector<uint8_t>(32, 0x01);
  accept.authentication = auth;
  message.description = std::make_unique<ContentDescription>(auth);
  message.SetPayload(std::move(accept));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_accept());
  EXPECT_EQ(stanza.jingle().session_accept().authentication().method(),
            ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519);
  EXPECT_EQ(stanza.jingle().session_accept().authentication().certificate(),
            decoded_cert);
  EXPECT_EQ(stanza.jingle().session_accept().authentication().spake_message(),
            std::string(32, '\x01'));

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_accept = std::get_if<SessionAccept>(&converted.payload());
  ASSERT_TRUE(converted_accept);
  ASSERT_TRUE(converted_accept->authentication.has_value());
  EXPECT_EQ(converted_accept->authentication->method,
            AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519);
  EXPECT_EQ(converted_accept->authentication->certificate, auth.certificate);
  EXPECT_EQ(converted_accept->authentication->spake_message,
            auth.spake_message);
}

TEST_F(JingleMessageProtoConverterTest, ConvertSessionTerminate_CleanTeardown) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_term_clean";
  message.sid = "crd_sess_987654321";
  SessionTerminate terminate;
  terminate.reason = SessionTerminate::Reason::kSuccess;
  message.SetPayload(std::move(terminate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_terminate());
  EXPECT_EQ(stanza.jingle().session_terminate().reason(),
            ftl::SessionTerminate::SUCCESS);
  EXPECT_FALSE(stanza.jingle().session_terminate().has_error_code());

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
  auto* converted_terminate =
      std::get_if<SessionTerminate>(&converted_message.payload());
  ASSERT_TRUE(converted_terminate);
  EXPECT_EQ(converted_terminate->reason, SessionTerminate::Reason::kSuccess);
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertSessionTerminate_MaxSessionLengthExpired) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_term_err";
  message.sid = "crd_sess_987654321";
  message.reason = SessionTerminate::Reason::kExpired;
  message.error_code = ErrorCode::MAX_SESSION_LENGTH;
  message.error_details =
      "The maximum allowed session duration (20 hours) has elapsed.";
  message.error_location = "remoting/host/client_session.cc:512";
  message.SetPayload(SessionTerminate());

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  EXPECT_TRUE(stanza.jingle().has_session_terminate());
  EXPECT_EQ(stanza.jingle().session_terminate().reason(),
            ftl::SessionTerminate::EXPIRED);
  EXPECT_EQ(stanza.jingle().session_terminate().error_code(),
            "MAX_SESSION_LENGTH");
  EXPECT_EQ(stanza.jingle().session_terminate().error_details(),
            "The maximum allowed session duration (20 hours) has elapsed.");
  EXPECT_EQ(stanza.jingle().session_terminate().error_location(),
            "remoting/host/client_session.cc:512");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  EXPECT_EQ(converted.reason, SessionTerminate::Reason::kExpired);
  EXPECT_EQ(converted.error_code, ErrorCode::MAX_SESSION_LENGTH);
  EXPECT_EQ(converted.error_details,
            "The maximum allowed session duration (20 hours) has elapsed.");
  EXPECT_EQ(converted.error_location, "remoting/host/client_session.cc:512");
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertSessionTerminateWithDirectFields) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_term_direct";
  message.sid = kSid;
  message.reason = SessionTerminate::Reason::kGeneralError;
  message.error_code = ErrorCode::HOST_OVERLOAD;
  message.error_details = "Host is overloaded";
  message.error_location = "host.cc:123";
  message.SetPayload(SessionTerminate());

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_terminate());
  EXPECT_EQ(stanza.jingle().session_terminate().reason(),
            ftl::SessionTerminate::GENERAL_ERROR);
  EXPECT_EQ(stanza.jingle().session_terminate().error_code(), "HOST_OVERLOAD");
  EXPECT_EQ(stanza.jingle().session_terminate().error_details(),
            "Host is overloaded");
  EXPECT_EQ(stanza.jingle().session_terminate().error_location(),
            "host.cc:123");

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;

  EXPECT_EQ(converted_message.reason, SessionTerminate::Reason::kGeneralError);
  EXPECT_EQ(converted_message.error_code, ErrorCode::HOST_OVERLOAD);
  EXPECT_EQ(converted_message.error_details, "Host is overloaded");
  EXPECT_EQ(converted_message.error_location, "host.cc:123");

  auto* converted_terminate =
      std::get_if<SessionTerminate>(&converted_message.payload());
  ASSERT_TRUE(converted_terminate);
  EXPECT_EQ(converted_terminate->reason,
            SessionTerminate::Reason::kGeneralError);
  EXPECT_EQ(converted_terminate->error_code, "HOST_OVERLOAD");
  EXPECT_EQ(converted_terminate->error_details, "Host is overloaded");
  EXPECT_EQ(converted_terminate->error_location, "host.cc:123");
}

TEST_F(JingleMessageProtoConverterTest, ConvertReplyResult) {
  JingleMessageReply reply;
  reply.from = from_address_;
  reply.to = to_address_;
  reply.message_id = "msg_reply_008_res";
  reply.reply_type = JingleMessageReply::REPLY_RESULT;

  ftl::IqStanza stanza = reply.ToFtlIqStanza();

  EXPECT_EQ(stanza.id(), "msg_reply_008_res");
  EXPECT_TRUE(stanza.has_reply());
  EXPECT_EQ(stanza.sender().local_part(), "from_user");
  EXPECT_EQ(stanza.sender().domain_part(), "gmail.com");
  EXPECT_EQ(stanza.receiver().local_part(), "to_user");
  EXPECT_EQ(stanza.receiver().domain_part(), "gmail.com");

  JingleMessageReply converted_reply;
  ASSERT_TRUE(JingleMessageReplyFromProto(stanza, &converted_reply));
  EXPECT_EQ(converted_reply.message_id, "msg_reply_008_res");
  EXPECT_EQ(converted_reply.from, from_address_);
  EXPECT_EQ(converted_reply.to, to_address_);
  EXPECT_EQ(converted_reply.reply_type, JingleMessageReply::REPLY_RESULT);
}

struct IqErrorTestCase {
  const char* stanza_id;
  JingleMessageReply::ErrorType error_type;
  ftl::ErrorStanza::Condition proto_condition;
  const char* error_text;
};

class JingleMessageProtoConverterIqErrorTest
    : public JingleMessageProtoConverterTest,
      public testing::WithParamInterface<IqErrorTestCase> {};

TEST_P(JingleMessageProtoConverterIqErrorTest, ConvertReplyErrorAllConditions) {
  const IqErrorTestCase& tc = GetParam();
  JingleMessageReply reply;
  reply.from = from_address_;
  reply.to = to_address_;
  reply.message_id = tc.stanza_id;
  reply.reply_type = JingleMessageReply::REPLY_ERROR;
  reply.error_type = tc.error_type;
  reply.text = tc.error_text;

  ftl::IqStanza stanza = reply.ToFtlIqStanza();

  EXPECT_EQ(stanza.id(), tc.stanza_id);
  EXPECT_TRUE(stanza.has_error());
  EXPECT_EQ(stanza.error().condition(), tc.proto_condition);
  EXPECT_EQ(stanza.error().text(), tc.error_text);

  JingleMessageReply converted_reply;
  ASSERT_TRUE(JingleMessageReplyFromProto(stanza, &converted_reply));
  EXPECT_EQ(converted_reply.message_id, tc.stanza_id);
  EXPECT_EQ(converted_reply.reply_type, JingleMessageReply::REPLY_ERROR);
  EXPECT_EQ(converted_reply.error_type, tc.error_type);
  EXPECT_EQ(converted_reply.text, tc.error_text);
}

INSTANTIATE_TEST_SUITE_P(
    AllConditions,
    JingleMessageProtoConverterIqErrorTest,
    testing::Values(
        IqErrorTestCase{"msg_reply_008_bad_req",
                        JingleMessageReply::BAD_REQUEST,
                        ftl::ErrorStanza::BAD_REQUEST, "Bad request payload"},
        IqErrorTestCase{
            "msg_reply_008_not_impl", JingleMessageReply::NOT_IMPLEMENTED,
            ftl::ErrorStanza::NOT_IMPLEMENTED, "Feature not implemented"},
        IqErrorTestCase{"msg_reply_008_invalid_sid",
                        JingleMessageReply::INVALID_SID,
                        ftl::ErrorStanza::INVALID_SID, "Invalid session ID"},
        IqErrorTestCase{"msg_reply_008_unexp_req",
                        JingleMessageReply::UNEXPECTED_REQUEST,
                        ftl::ErrorStanza::UNEXPECTED_REQUEST,
                        "Unexpected request in current state"},
        IqErrorTestCase{
            "msg_reply_008_unsupp_info", JingleMessageReply::UNSUPPORTED_INFO,
            ftl::ErrorStanza::UNSUPPORTED_INFO, "Unsupported info element"},
        IqErrorTestCase{"msg_reply_008_unspec", JingleMessageReply::UNSPECIFIED,
                        ftl::ErrorStanza::CONDITION_UNSPECIFIED,
                        "Unspecified error"}));

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
            "from_user");
  EXPECT_EQ(stanza.jingle().session_initiate().initiator().domain_part(),
            "gmail.com");
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

TEST_F(JingleMessageProtoConverterTest,
       ConvertSessionAcceptWithDescriptionAuth) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  message.SetPayload(SessionAccept());
  JingleAuthentication auth = CreateTestAuthentication();
  message.description = std::make_unique<ContentDescription>(auth);

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  EXPECT_TRUE(stanza.jingle().has_session_accept());
  EXPECT_TRUE(stanza.jingle().session_accept().has_authentication());
  EXPECT_EQ(stanza.jingle().session_accept().authentication().method(),
            ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519);

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;
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

TEST_F(JingleMessageProtoConverterTest, ConvertSessionInfo_VerificationHash) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_info_004";
  message.sid = "crd_sess_987654321";

  SessionInfo session_info;
  JingleAuthentication auth;
  std::string hash_str = "01234567890123456789012345678901";
  auth.verification_hash =
      std::vector<uint8_t>(hash_str.begin(), hash_str.end());
  session_info.authentication = auth;
  message.SetPayload(std::move(session_info));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_session_info());
  ASSERT_TRUE(stanza.jingle().session_info().has_authentication());
  EXPECT_EQ(stanza.jingle().session_info().authentication().verification_hash(),
            "01234567890123456789012345678901");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_info = std::get_if<SessionInfo>(&converted.payload());
  ASSERT_TRUE(converted_info);
  ASSERT_TRUE(converted_info->authentication.has_value());
  EXPECT_EQ(converted_info->authentication->verification_hash,
            auth.verification_hash);
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertTransportInfo_RealisticBundleSdpAndIceBatch) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_transport_005";
  message.sid = "crd_sess_987654321";

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kOffer;
  sdp.sdp = kRealisticBundleSdp;
  sdp.signature = {0x01, 0x02, 0x03, 0x04};
  transport.session_description = sdp;

  struct CandidateDef {
    std::string mid;
    int mline;
    std::string candidate_str;
  };
  CandidateDef candidates[] = {
      {"0", 0,
       "candidate:1001 1 udp 2122260223 192.168.1.150 54321 typ host "
       "generation 0"},
      {"0", 0,
       "candidate:1002 1 udp 2122260222 2607:f8b0:4005:805::200e 54322 typ "
       "host generation 0"},
      {"1", 1,
       "candidate:1003 1 tcp 1518280447 192.168.1.150 9 typ host tcptype "
       "active generation 0"},
      {"1", 1,
       "candidate:2001 1 udp 1686052607 74.125.250.1 54321 typ srflx raddr "
       "192.168.1.150 rport 54321 generation 0"},
      {"0", 0,
       "candidate:3001 1 udp 41885695 74.125.250.200 19302 typ relay raddr "
       "74.125.250.1 rport 54321 generation 0"},
  };

  for (const auto& c : candidates) {
    webrtc::SdpParseError parse_error;
    std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
        webrtc::IceCandidate::Create(c.mid, c.mline, c.candidate_str,
                                     &parse_error);
    ASSERT_TRUE(webrtc_candidate) << parse_error.description;
    IceTransportInfo::NamedCandidate named_cand(
        c.mid, webrtc_candidate->candidate(), c.mline);
    transport.candidates.push_back(std::move(named_cand));
  }

  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_transport_info());
  EXPECT_EQ(stanza.jingle().transport_info().candidates_size(), 5);

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted.payload());
  ASSERT_TRUE(converted_transport->session_description.has_value());
  EXPECT_EQ(converted_transport->session_description->sdp, kRealisticBundleSdp);
  EXPECT_EQ(converted_transport->session_description->type,
            SessionDescription::Type::kOffer);
  EXPECT_EQ(converted_transport->session_description->signature, sdp.signature);
  ASSERT_EQ(converted_transport->candidates.size(), 5u);
  EXPECT_EQ(converted_transport->candidates[0].name, "0");
  EXPECT_EQ(*converted_transport->candidates[0].sdp_m_line_index, 0);
  EXPECT_EQ(converted_transport->candidates[0].candidate.protocol(), "udp");
  EXPECT_EQ(converted_transport->candidates[2].name, "1");
  EXPECT_EQ(*converted_transport->candidates[2].sdp_m_line_index, 1);
  EXPECT_EQ(converted_transport->candidates[2].candidate.protocol(), "tcp");
  EXPECT_EQ(converted_transport->candidates[3].name, "1");
  EXPECT_EQ(*converted_transport->candidates[3].sdp_m_line_index, 1);
  EXPECT_EQ(converted_transport->candidates[3].candidate.type(),
            webrtc::IceCandidateType::kSrflx);
  EXPECT_EQ(converted_transport->candidates[4].name, "0");
  EXPECT_EQ(*converted_transport->candidates[4].sdp_m_line_index, 0);
  EXPECT_EQ(converted_transport->candidates[4].candidate.type(),
            webrtc::IceCandidateType::kRelay);
}

TEST_F(JingleMessageProtoConverterTest,
       ConvertTransportInfo_InitialAnswerWithSignature) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_transport_006a";
  message.sid = "crd_sess_987654321";

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kAnswer;
  sdp.sdp =
      "v=0\r\no=- 5123456789012345 2 IN IP4 127.0.0.1\r\ns=-\r\n"
      "t=0 0\r\na=group:BUNDLE 0 1 2\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\nc=IN IP4 0.0.0.0\r\n"
      "a=ice-ufrag:ClientUfrag123\r\n"
      "a=ice-pwd:ClientPassword1234567890\r\n"
      "a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:"
      "88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99\r\n"
      "a=setup:active\r\na=mid:0\r\na=sendrecv\r\na=rtcp-mux\r\n"
      "a=rtpmap:111 opus/48000/2\r\n";
  std::string sig_str = "signature_hmac_example_1234567890";
  sdp.signature = std::vector<uint8_t>(sig_str.begin(), sig_str.end());
  transport.session_description = sdp;
  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_transport_info());
  ASSERT_TRUE(stanza.jingle().transport_info().has_session_description());
  EXPECT_EQ(stanza.jingle().transport_info().session_description().type(),
            ftl::SessionDescription::ANSWER);
  EXPECT_EQ(stanza.jingle().transport_info().session_description().signature(),
            "signature_hmac_example_1234567890");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted.payload());
  ASSERT_TRUE(converted_transport);
  ASSERT_TRUE(converted_transport->session_description.has_value());
  EXPECT_EQ(converted_transport->session_description->type,
            SessionDescription::Type::kAnswer);
  EXPECT_EQ(converted_transport->session_description->sdp, sdp.sdp);
  EXPECT_EQ(converted_transport->session_description->signature, sdp.signature);
}

TEST_F(JingleMessageProtoConverterTest, ConvertTransportInfo_IceRestartOffer) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = "msg_restart_006b";
  message.sid = "crd_sess_987654321";

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = SessionDescription::Type::kOffer;
  sdp.sdp =
      "v=0\r\no=- 4123456789012345 3 IN IP4 127.0.0.1\r\ns=-\r\n"
      "t=0 0\r\na=group:BUNDLE 0 1 2\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\nc=IN IP4 0.0.0.0\r\n"
      "a=ice-ufrag:NewClientUfrag\r\n"
      "a=ice-pwd:NewClientPassword1234567890\r\n"
      "a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:"
      "EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
      "a=setup:actpass\r\na=mid:0\r\na=sendrecv\r\na=rtcp-mux\r\n"
      "a=rtpmap:111 opus/48000/2\r\n";
  transport.session_description = sdp;

  webrtc::SdpParseError parse_error;
  std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
      webrtc::IceCandidate::Create(
          "0", 0,
          "candidate:5001 1 udp 2122260223 10.0.0.5 54321 typ host "
          "generation 1",
          &parse_error);
  ASSERT_TRUE(webrtc_candidate) << parse_error.description;
  IceTransportInfo::NamedCandidate named_cand("0",
                                              webrtc_candidate->candidate(), 0);
  transport.candidates.push_back(std::move(named_cand));
  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_TRUE(stanza.jingle().has_transport_info());
  ASSERT_TRUE(stanza.jingle().transport_info().has_session_description());
  EXPECT_EQ(stanza.jingle().transport_info().session_description().type(),
            ftl::SessionDescription::OFFER);
  ASSERT_EQ(stanza.jingle().transport_info().candidates_size(), 1);
  EXPECT_EQ(stanza.jingle().transport_info().candidates(0).sdp_mid(), "0");

  JingleMessage converted;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted, &error)) << error;
  auto* converted_transport =
      std::get_if<JingleTransportInfo>(&converted.payload());
  ASSERT_TRUE(converted_transport);
  ASSERT_TRUE(converted_transport->session_description.has_value());
  EXPECT_EQ(converted_transport->session_description->type,
            SessionDescription::Type::kOffer);
  EXPECT_EQ(converted_transport->session_description->sdp, sdp.sdp);
  ASSERT_EQ(converted_transport->candidates.size(), 1u);
  EXPECT_EQ(converted_transport->candidates[0].name, "0");
  EXPECT_EQ(*converted_transport->candidates[0].sdp_m_line_index, 0);
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

TEST_F(JingleMessageProtoConverterTest, ConvertIncomingSplitSender) {
  ftl::IqStanza stanza;
  stanza.set_id(kMessageId);

  auto* sender = stanza.mutable_sender();
  sender->set_local_part(kTestSenderLocal);
  sender->set_domain_part(kTestSenderDomain);
  sender->set_resource_part(std::string(kFtlResourcePrefix) +
                            kTestSenderRegistration);

  auto* receiver = stanza.mutable_receiver();
  receiver->set_local_part(kTestReceiverLocal);
  receiver->set_domain_part(kTestReceiverDomain);
  receiver->set_resource_part(std::string(kFtlResourcePrefix) +
                              kTestReceiverRegistration);

  stanza.mutable_jingle()->set_session_id(kSid);
  stanza.mutable_jingle()->mutable_session_initiate();

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;

  std::string expected_from = SignalingAddress::CreateFtlSignalingAddress(
                                  kTestSenderEmail, kTestSenderRegistration)
                                  .id();
  std::string expected_to = SignalingAddress::CreateFtlSignalingAddress(
                                kTestReceiverEmail, kTestReceiverRegistration)
                                .id();
  EXPECT_EQ(converted_message.from.id(), expected_from);
  EXPECT_EQ(converted_message.to.id(), expected_to);
}

TEST_F(JingleMessageProtoConverterTest, ConvertIncomingReplySplitSender) {
  ftl::IqStanza stanza;
  stanza.set_id(kMessageId);

  auto* sender = stanza.mutable_sender();
  sender->set_local_part(kTestSenderLocal);
  sender->set_domain_part(kTestSenderDomain);
  sender->set_resource_part(std::string(kFtlResourcePrefix) +
                            kTestSenderRegistration);

  auto* receiver = stanza.mutable_receiver();
  receiver->set_local_part(kTestReceiverLocal);
  receiver->set_domain_part(kTestReceiverDomain);
  receiver->set_resource_part(std::string(kFtlResourcePrefix) +
                              kTestReceiverRegistration);

  stanza.mutable_reply();

  JingleMessageReply converted_reply;
  ASSERT_TRUE(JingleMessageReplyFromProto(stanza, &converted_reply));

  std::string expected_from = SignalingAddress::CreateFtlSignalingAddress(
                                  kTestSenderEmail, kTestSenderRegistration)
                                  .id();
  std::string expected_to = SignalingAddress::CreateFtlSignalingAddress(
                                kTestReceiverEmail, kTestReceiverRegistration)
                                .id();
  EXPECT_EQ(converted_reply.from.id(), expected_from);
  EXPECT_EQ(converted_reply.to.id(), expected_to);
}

TEST_F(JingleMessageProtoConverterTest, ConvertAttachmentsRoundTrip) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  message.SetPayload(SessionInitiate());

  Attachment host_attr_attachment;
  HostAttributesAttachment host_attributes;
  host_attributes.attribute.push_back("Debug-Build");
  host_attributes.attribute.push_back("HWEncoder");
  host_attr_attachment.host_attributes = std::move(host_attributes);
  message.attachments.push_back(std::move(host_attr_attachment));

  Attachment host_config_attachment;
  HostConfigAttachment host_config;
  host_config.settings["Av1-Encoder-Speed"] = "11";
  host_config_attachment.host_config = std::move(host_config);
  message.attachments.push_back(std::move(host_config_attachment));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  ASSERT_EQ(stanza.jingle().attachments_size(), 2);

  JingleMessage converted_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromProto(stanza, &converted_message, &error))
      << error;

  ASSERT_EQ(converted_message.attachments.size(), 2u);
  ASSERT_TRUE(converted_message.attachments[0].host_attributes.has_value());
  EXPECT_EQ(converted_message.attachments[0].host_attributes->attribute.size(),
            2u);
  EXPECT_EQ(converted_message.attachments[0].host_attributes->attribute[0],
            "Debug-Build");
  EXPECT_EQ(converted_message.attachments[0].host_attributes->attribute[1],
            "HWEncoder");

  ASSERT_TRUE(converted_message.attachments[1].host_config.has_value());
  EXPECT_EQ(converted_message.attachments[1].host_config->settings.size(), 1u);
  EXPECT_EQ(converted_message.attachments[1]
                .host_config->settings["Av1-Encoder-Speed"],
            "11");
}

TEST_F(JingleMessageProtoConverterTest, ConvertUnknownTerminateReason) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  SessionTerminate terminate;
  terminate.reason = static_cast<SessionTerminate::Reason>(999);
  message.SetPayload(std::move(terminate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  EXPECT_EQ(stanza.jingle().session_terminate().reason(),
            ftl::SessionTerminate::UNKNOWN_REASON);
}

TEST_F(JingleMessageProtoConverterTest, ConvertUnknownAuthMethod) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;
  SessionInitiate initiate;
  JingleAuthentication auth;
  auth.supported_methods = {static_cast<AuthenticationMethod>(999)};
  auth.method = static_cast<AuthenticationMethod>(999);
  initiate.authentication = auth;
  message.SetPayload(std::move(initiate));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  EXPECT_TRUE(stanza.jingle().has_session_initiate());
  EXPECT_TRUE(stanza.jingle().session_initiate().has_authentication());
  EXPECT_EQ(
      stanza.jingle().session_initiate().authentication().supported_methods(0),
      ftl::AUTHENTICATION_METHOD_UNSPECIFIED);
  EXPECT_EQ(stanza.jingle().session_initiate().authentication().method(),
            ftl::AUTHENTICATION_METHOD_UNSPECIFIED);
}

TEST_F(JingleMessageProtoConverterTest, ConvertUnknownSdpType) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = kSid;

  JingleTransportInfo transport;
  SessionDescription sdp;
  sdp.type = static_cast<SessionDescription::Type>(999);
  sdp.sdp = "test_sdp";
  transport.session_description = sdp;
  message.SetPayload(std::move(transport));

  ftl::IqStanza stanza = message.ToFtlIqStanza();
  EXPECT_TRUE(stanza.jingle().has_transport_info());
  EXPECT_TRUE(stanza.jingle().transport_info().has_session_description());
  EXPECT_EQ(stanza.jingle().transport_info().session_description().type(),
            ftl::SessionDescription::SDP_TYPE_UNSPECIFIED);
}

TEST_F(JingleMessageProtoConverterTest, ConvertMissingHeaders) {
  JingleMessage message;
  message.from = from_address_;
  message.to = to_address_;
  message.message_id = kMessageId;
  message.sid = "";
  message.SetPayload(SessionInitiate());

  ftl::IqStanza stanza = message.ToFtlIqStanza();

  JingleMessage converted_message;
  std::string error;
  EXPECT_FALSE(JingleMessageFromProto(stanza, &converted_message, &error));
  EXPECT_EQ(error, "sid attribute is missing");

  stanza.mutable_jingle()->set_session_id(kSid);
  stanza.clear_sender();
  EXPECT_FALSE(JingleMessageFromProto(stanza, &converted_message, &error));
  EXPECT_TRUE(error.starts_with("Missing signaling address"));
}

}  // namespace remoting

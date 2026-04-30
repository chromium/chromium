// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_xml_converter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/webrtc/api/candidate.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlAttr;
using jingle_xmpp::XmlElement;

namespace remoting {

namespace {

const char kXmlNsNs[] = "http://www.w3.org/2000/xmlns/";
const char kXmlNs[] = "xmlns";

bool VerifyXml(const XmlElement* exp,
               const XmlElement* val,
               std::string* error) {
  if (exp->Name() != val->Name()) {
    *error = base::StringPrintf("<%s> is expected, but <%s> found",
                                exp->Name().Merged().c_str(),
                                val->Name().Merged().c_str());
    return false;
  }
  if (exp->BodyText() != val->BodyText()) {
    const std::string& local_part = exp->Name().LocalPart();
    *error = base::StringPrintf(
        "<%s>%s</%s> is expected, but found <%s>%s</%s>", local_part.c_str(),
        exp->BodyText().c_str(), local_part.c_str(), local_part.c_str(),
        val->BodyText().c_str(), local_part.c_str());
    return false;
  }

  for (const XmlAttr* exp_attr = exp->FirstAttr(); exp_attr != nullptr;
       exp_attr = exp_attr->NextAttr()) {
    if (exp_attr->Name().Namespace() == kXmlNsNs ||
        exp_attr->Name() == QName(kXmlNs)) {
      continue;
    }
    if (val->Attr(exp_attr->Name()) != exp_attr->Value()) {
      *error = base::StringPrintf(
          "In <%s> attribute %s is expected to be set to %s",
          exp->Name().LocalPart().c_str(), exp_attr->Name().LocalPart().c_str(),
          exp_attr->Value().c_str());
      return false;
    }
  }

  for (const XmlAttr* val_attr = val->FirstAttr(); val_attr;
       val_attr = val_attr->NextAttr()) {
    if (val_attr->Name().Namespace() == kXmlNsNs ||
        val_attr->Name() == QName(kXmlNs)) {
      continue;
    }
    if (exp->Attr(val_attr->Name()) != val_attr->Value()) {
      *error = base::StringPrintf("In <%s> unexpected attribute %s",
                                  exp->Name().LocalPart().c_str(),
                                  val_attr->Name().LocalPart().c_str());
      return false;
    }
  }

  const XmlElement* exp_child = exp->FirstElement();
  const XmlElement* val_child = val->FirstElement();
  while (exp_child && val_child) {
    if (!VerifyXml(exp_child, val_child, error)) {
      return false;
    }
    exp_child = exp_child->NextElement();
    val_child = val_child->NextElement();
  }
  if (exp_child) {
    *error = base::StringPrintf("<%s> is expected, but not found",
                                exp_child->Name().Merged().c_str());
    return false;
  }

  if (val_child) {
    *error = base::StringPrintf("Unexpected <%s> found",
                                val_child->Name().Merged().c_str());
    return false;
  }

  return true;
}

}  // namespace

TEST(JingleMessageXmlConverterTest, JingleMessageReply_Result) {
  JingleMessageReply reply;
  reply.reply_type = JingleMessageReply::REPLY_RESULT;
  reply.message_id = "test_id";
  reply.to = SignalingAddress("to_jid");
  reply.from = SignalingAddress("from_jid");

  std::unique_ptr<XmlElement> xml = JingleMessageReplyToXml(reply);
  ASSERT_TRUE(xml);

  const char* expected_xml =
      "<iq type='result' id='test_id' to='to_jid' from='from_jid' "
      "xmlns='jabber:client'>"
      "<jingle xmlns='urn:xmpp:jingle:1'/>"
      "</iq>";
  std::unique_ptr<XmlElement> expected(XmlElement::ForStr(expected_xml));
  std::string error;
  EXPECT_TRUE(VerifyXml(expected.get(), xml.get(), &error)) << error;

  JingleMessageReply parsed_reply;
  EXPECT_TRUE(JingleMessageReplyFromXml(xml.get(), &parsed_reply));
  EXPECT_EQ(parsed_reply.reply_type, JingleMessageReply::REPLY_RESULT);
  EXPECT_FALSE(parsed_reply.error_type.has_value());
}

TEST(JingleMessageXmlConverterTest, JingleMessageReply_Error) {
  struct ErrorTestCase {
    JingleMessageReply::ErrorType type;
    const char* expected_error_xml;
  };
  ErrorTestCase test_cases[] = {
      {JingleMessageReply::BAD_REQUEST,
       "<error type='modify'><bad-request/></error>"},
      {JingleMessageReply::NOT_IMPLEMENTED,
       "<error type='cancel'><feature-bad-request/></error>"},
      {JingleMessageReply::INVALID_SID,
       "<error type='modify'><item-not-found/><text xml:lang='en' "
       "xmlns='jabber:client'>Invalid SID</text></error>"},
      {JingleMessageReply::UNEXPECTED_REQUEST,
       "<error type='modify'><unexpected-request/></error>"},
      {JingleMessageReply::UNSUPPORTED_INFO,
       "<error type='modify'><feature-not-implemented/></error>"},
      {JingleMessageReply::UNSPECIFIED,
       "<error type='cancel'><unspecified-error/></error>"},
  };

  for (const auto& test_case : test_cases) {
    JingleMessageReply reply(test_case.type);
    reply.message_id = "test_id";
    reply.to = SignalingAddress("to_jid");
    reply.from = SignalingAddress("from_jid");

    std::unique_ptr<XmlElement> xml = JingleMessageReplyToXml(reply);
    ASSERT_TRUE(xml);

    std::string expected_xml_str = base::ReplaceStringPlaceholders(
        "<iq type='error' id='test_id' to='to_jid' from='from_jid' "
        "xmlns='jabber:client'>$1</iq>",
        {test_case.expected_error_xml}, nullptr);
    std::unique_ptr<XmlElement> expected(XmlElement::ForStr(expected_xml_str));
    std::string error;
    EXPECT_TRUE(VerifyXml(expected.get(), xml.get(), &error))
        << "Error type: " << test_case.type << " - " << error;

    JingleMessageReply parsed_reply;
    EXPECT_TRUE(JingleMessageReplyFromXml(xml.get(), &parsed_reply));
    EXPECT_EQ(parsed_reply.reply_type, JingleMessageReply::REPLY_ERROR);
    EXPECT_EQ(parsed_reply.error_type, test_case.type);
  }
}

TEST(JingleMessageXmlConverterTest, Attachment_RoundTrip) {
  Attachment attachment;
  HostAttributesAttachment host_attributes;
  host_attributes.attribute = {"attr1", "attr2"};
  attachment.host_attributes = std::move(host_attributes);

  HostConfigAttachment host_config;
  host_config.settings["key1"] = "val1";
  host_config.settings["key2"] = "val2";
  attachment.host_config = std::move(host_config);

  std::unique_ptr<XmlElement> xml = AttachmentToXml(attachment);
  ASSERT_TRUE(xml);

  Attachment parsed_attachment;
  EXPECT_TRUE(AttachmentFromXml(xml.get(), &parsed_attachment));

  ASSERT_TRUE(parsed_attachment.host_attributes);
  EXPECT_THAT(parsed_attachment.host_attributes->attribute,
              testing::ElementsAre("attr1", "attr2"));

  ASSERT_TRUE(parsed_attachment.host_config);
  EXPECT_EQ(parsed_attachment.host_config->settings["key1"], "val1");
  EXPECT_EQ(parsed_attachment.host_config->settings["key2"], "val2");
}

TEST(JingleMessageXmlConverterTest, JingleAuthentication_RoundTrip) {
  JingleAuthentication auth;
  auth.supported_methods = {
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
      AuthenticationMethod::PAIRED_SPAKE2_CURVE25519};
  auth.method = AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
  auth.id = "auth_id";
  auth.spake_message = {1, 2, 3};
  auth.verification_hash = {4, 5, 6};
  auth.certificate = {7, 8, 9};
  auth.session_authz_host_token = "host_token";
  auth.session_authz_session_token = "session_token";
  auth.pairing_info.emplace();
  auth.pairing_info->client_id = "client_id";
  auth.pairing_error = "pairing_error";
  auth.test_id = "test_id";
  auth.test_key = {10, 11, 12};

  std::unique_ptr<XmlElement> xml = JingleAuthenticationToXml(auth);
  ASSERT_TRUE(xml);

  JingleAuthentication parsed_auth;
  EXPECT_TRUE(JingleAuthenticationFromXml(xml.get(), &parsed_auth));

  EXPECT_THAT(parsed_auth.supported_methods,
              testing::ElementsAre(
                  AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
                  AuthenticationMethod::PAIRED_SPAKE2_CURVE25519));
  EXPECT_EQ(parsed_auth.method,
            AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519);
  EXPECT_EQ(parsed_auth.id, "auth_id");
  EXPECT_EQ(parsed_auth.spake_message, auth.spake_message);
  EXPECT_EQ(parsed_auth.verification_hash, auth.verification_hash);
  EXPECT_EQ(parsed_auth.certificate, auth.certificate);
  EXPECT_EQ(parsed_auth.session_authz_host_token, "host_token");
  EXPECT_EQ(parsed_auth.session_authz_session_token, "session_token");
  ASSERT_TRUE(parsed_auth.pairing_info);
  EXPECT_EQ(parsed_auth.pairing_info->client_id, "client_id");
  EXPECT_EQ(parsed_auth.pairing_error, "pairing_error");
  EXPECT_EQ(parsed_auth.test_id, "test_id");
  EXPECT_EQ(parsed_auth.test_key, auth.test_key);
}

TEST(JingleMessageXmlConverterTest, SessionInfo_Authentication_RoundTrip) {
  SessionInfo info;
  info.authentication.emplace();
  info.authentication->id = "auth_id";

  auto jingle_tag =
      std::make_unique<XmlElement>(QName("urn:xmpp:jingle:1", "jingle"), true);
  SessionInfoToXml(info, jingle_tag.get());

  SessionInfo parsed_info;
  EXPECT_TRUE(SessionInfoFromXml(jingle_tag.get(), &parsed_info));

  ASSERT_TRUE(parsed_info.authentication);
  EXPECT_EQ(parsed_info.authentication->id, "auth_id");
  EXPECT_FALSE(parsed_info.generic_info.has_value());
}

TEST(JingleMessageXmlConverterTest, SessionInfo_Generic_RoundTrip) {
  SessionInfo info;
  info.generic_info.emplace();
  info.generic_info->name = "test-node";
  info.generic_info->namespace_uri = "test-ns";
  info.generic_info->body = "test-body";

  auto jingle_tag =
      std::make_unique<XmlElement>(QName("urn:xmpp:jingle:1", "jingle"), true);
  SessionInfoToXml(info, jingle_tag.get());

  SessionInfo parsed_info;
  EXPECT_TRUE(SessionInfoFromXml(jingle_tag.get(), &parsed_info));

  ASSERT_TRUE(parsed_info.generic_info);
  EXPECT_EQ(parsed_info.generic_info->name, "test-node");
  EXPECT_EQ(parsed_info.generic_info->namespace_uri, "test-ns");
  EXPECT_EQ(parsed_info.generic_info->body, "test-body");
  EXPECT_FALSE(parsed_info.authentication.has_value());
}

TEST(JingleMessageXmlConverterTest, ContentDescription_RoundTrip) {
  JingleAuthentication auth;
  auth.id = "auth_id";

  ContentDescription description(auth);

  std::unique_ptr<XmlElement> xml = ContentDescriptionToXml(description);
  ASSERT_TRUE(xml);

  std::unique_ptr<ContentDescription> parsed =
      ContentDescriptionFromXml(xml.get());
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->authentication().id, "auth_id");
}

TEST(JingleMessageXmlConverterTest, JingleTransportInfo_RoundTrip) {
  JingleTransportInfo transport;
  transport.xml_namespace = "google:remoting:webrtc";

  IceTransportInfo::NamedCandidate candidate;
  candidate.name = "test-candidate";
  candidate.candidate = webrtc::Candidate(
      1, "udp", webrtc::SocketAddress("1.2.3.4", 1234), 1, "ufrag", "password",
      webrtc::IceCandidateType::kHost, 0, "foundation");
  candidate.sdp_m_line_index = 0;
  transport.candidates.push_back(candidate);

  std::unique_ptr<XmlElement> xml = JingleTransportInfoToXml(transport);
  ASSERT_TRUE(xml);

  JingleTransportInfo parsed_transport;
  EXPECT_TRUE(JingleTransportInfoFromXml(xml.get(), &parsed_transport));

  EXPECT_EQ(parsed_transport.xml_namespace, "google:remoting:webrtc");
  ASSERT_EQ(parsed_transport.candidates.size(), 1U);
  EXPECT_EQ(parsed_transport.candidates.front().name, "test-candidate");
  EXPECT_EQ(parsed_transport.candidates.front()
                .candidate.address()
                .ipaddr()
                .ToString(),
            "1.2.3.4");
  EXPECT_EQ(parsed_transport.candidates.front().candidate.address().port(),
            1234);
}

}  // namespace remoting

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_data_structures.h"

#include <stddef.h>

#include <array>

#include "base/strings/string_util.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/xmpp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlAttr;
using jingle_xmpp::XmlElement;

namespace remoting::protocol {

using ActionType = JingleMessage::ActionType;

namespace {

const char kXmlNsNs[] = "http://www.w3.org/2000/xmlns/";
const char kXmlNs[] = "xmlns";

// Compares two XML blobs and returns true if they are
// equivalent. Otherwise |error| is set to error message that
// specifies the first test.
bool VerifyXml(const XmlElement* exp,
               const XmlElement* val,
               std::string* error) {
  if (exp->Name() != val->Name()) {
    *error = "<" + exp->Name().Merged() + ">" + " is expected, but " + "<" +
             val->Name().Merged() + ">" + " found";
    return false;
  }
  if (exp->BodyText() != val->BodyText()) {
    *error = "<" + exp->Name().LocalPart() + ">" + exp->BodyText() + "</" +
             exp->Name().LocalPart() + ">" + " is expected, but found " + "<" +
             exp->Name().LocalPart() + ">" + val->BodyText() + "</" +
             exp->Name().LocalPart() + ">";
    return false;
  }

  for (const XmlAttr* exp_attr = exp->FirstAttr(); exp_attr != nullptr;
       exp_attr = exp_attr->NextAttr()) {
    if (exp_attr->Name().Namespace() == kXmlNsNs ||
        exp_attr->Name() == QName(kXmlNs)) {
      continue;  // Skip NS attributes.
    }
    if (val->Attr(exp_attr->Name()) != exp_attr->Value()) {
      *error = "In <" + exp->Name().LocalPart() + "> attribute " +
               exp_attr->Name().LocalPart() + " is expected to be set to " +
               exp_attr->Value();
      return false;
    }
  }

  for (const XmlAttr* val_attr = val->FirstAttr(); val_attr;
       val_attr = val_attr->NextAttr()) {
    if (val_attr->Name().Namespace() == kXmlNsNs ||
        val_attr->Name() == QName(kXmlNs)) {
      continue;  // Skip NS attributes.
    }
    if (exp->Attr(val_attr->Name()) != val_attr->Value()) {
      *error = "In <" + exp->Name().LocalPart() + "> unexpected attribute " +
               val_attr->Name().LocalPart();
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
    *error = "<" + exp_child->Name().Merged() + "> is expected, but not found";
    return false;
  }

  if (val_child) {
    *error = "Unexpected <" + val_child->Name().Merged() + "> found";
    return false;
  }

  return true;
}

// Parses |message_text| to JingleMessage then attempts to format it to XML and
// verifies that the same XML content is generated.
void ParseFormatAndCompare(const char* message_text, JingleMessage* parsed) {
  std::unique_ptr<XmlElement> source_message(XmlElement::ForStr(message_text));
  ASSERT_TRUE(source_message.get());

  EXPECT_TRUE(IsJingleMessage(source_message.get()));

  std::string error;
  EXPECT_TRUE(JingleMessageFromXml(source_message.get(), parsed, &error))
      << error;

  std::unique_ptr<XmlElement> formatted_message(JingleMessageToXml(*parsed));
  ASSERT_TRUE(formatted_message.get());
  EXPECT_TRUE(VerifyXml(source_message.get(), formatted_message.get(), &error))
      << error;
}

}  // namespace

// Session-initiate message for WebRTC-based protocol.
TEST(JingleMessageTest, SessionInitiateWebrtc) {
  // clang-format off
  const char* kTestSessionInitiateMessage =
      "<iq to='user@gmail.com/chromoting016DBB07' type='set' "
          "from='user@gmail.com/chromiumsy5C6A652D' "
          "xmlns='jabber:client'>"
        "<jingle xmlns='urn:xmpp:jingle:1' "
                "action='session-initiate' sid='2227053353' "
                "initiator='user@gmail.com/chromiumsy5C6A652D'>"
          "<content name='chromoting' creator='initiator'>"
            "<description xmlns='google:remoting'>"
              "<authentication>"
                "<spake-message>RmFrZSBhdXRoIHRva2VuCg==</spake-message>"
              "</authentication>"
            "</description>"
            "<transport xmlns='google:remoting:webrtc' />"
          "</content>"
        "</jingle>"
      "</iq>";
  // clang-format on

  JingleMessage message;
  ParseFormatAndCompare(kTestSessionInitiateMessage, &message);

  EXPECT_EQ(message.action(), ActionType::kSessionInitiate);
}

// Session-accept message for WebRTC-based protocol.
TEST(JingleMessageTest, SessionAcceptWebrtc) {
  // clang-format off
  const char* kTestSessionAcceptMessage =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-accept' sid='2227053353' "
                "xmlns='urn:xmpp:jingle:1'>"
          "<content creator='initiator' name='chromoting'>"
            "<description xmlns='google:remoting'>"
              "<authentication>"
                "<certificate>"
                  "MIICpjCCAY6gW0Cert0TANBgkqhkiG9w0BAQUFA="
                "</certificate>"
              "</authentication>"
            "</description>"
            "<transport xmlns='google:remoting:webrtc' />"
          "</content>"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  JingleMessage message;
  ParseFormatAndCompare(kTestSessionAcceptMessage, &message);

  EXPECT_EQ(message.action(), ActionType::kSessionAccept);
}

TEST(JingleMessageTest, SessionTerminate) {
  // clang-format off
  const char* kTestSessionTerminateMessage =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-terminate' sid='2227053353' "
                "xmlns='urn:xmpp:jingle:1'>"
          "<reason><success/></reason>"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  JingleMessage message;
  ParseFormatAndCompare(kTestSessionTerminateMessage, &message);
  EXPECT_EQ(message.action(), ActionType::kSessionTerminate);
}

TEST(JingleMessageTest, SessionInfo) {
  // clang-format off
  const char* kTestSessionInfoMessage =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-info' sid='2227053353' "
                "xmlns='urn:xmpp:jingle:1'>"
          "<test-info>TestMessage</test-info>"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  JingleMessage message;
  ParseFormatAndCompare(kTestSessionInfoMessage, &message);

  EXPECT_EQ(message.action(), ActionType::kSessionInfo);
  auto* session_info = std::get_if<SessionInfo>(&message.payload());
  ASSERT_TRUE(session_info);
  ASSERT_TRUE(session_info->generic_info);
  EXPECT_EQ(session_info->generic_info->name, "test-info");
  EXPECT_EQ(session_info->generic_info->namespace_uri, "urn:xmpp:jingle:1");
  EXPECT_EQ(session_info->generic_info->body, "TestMessage");
}

TEST(JingleMessageTest, MessageId) {
  // clang-format off
  const char* kTestMessageWithMessageId =
      "<iq to='user@gmail.com/chromoting016DBB07' id='test_id' type='set' "
          "from='user@gmail.com/chromiumsy5C6A652D' "
          "xmlns='jabber:client'>"
        "<jingle xmlns='urn:xmpp:jingle:1' "
                "action='session-info' sid='2227053353'>"
          "<test-info>TestMessage</test-info>"
        "</jingle>"
      "</iq>";
  // clang-format on

  JingleMessage message;
  ParseFormatAndCompare(kTestMessageWithMessageId, &message);

  EXPECT_EQ(message.message_id, "test_id");
}

TEST(JingleMessageReplyTest, ToXml) {
  // clang-format off
  const char* kTestIncomingMessage1 =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' id='4' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-terminate' "
                "sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
          "<reason><success/></reason>"
        "</jingle>"
      "</cli:iq>";
  const char* kTestIncomingMessage2 =
      "<cli:iq from='remoting@bot.talk.google.com' id='4' "
              "to='user@gmail.com/chromoting_ftl_5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-terminate' "
                "sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
          "<reason><success/></reason>"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  struct TestCase {
    const std::optional<JingleMessageReply::ErrorType> error;
    std::string error_text;
    std::string expected_text;
    std::string incoming_message;
  };
  // clang-format off
  auto tests = std::to_array<TestCase>({
      {JingleMessageReply::BAD_REQUEST, "",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'><bad-request/></error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::BAD_REQUEST, "ErrorText",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'>"
           "<bad-request/>"
           "<text xml:lang='en'>ErrorText</text>"
         "</error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::NOT_IMPLEMENTED, "",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='cancel'><feature-bad-request/></error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::INVALID_SID, "",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'>"
           "<item-not-found/>"
           "<text xml:lang='en'>Invalid SID</text>"
         "</error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::INVALID_SID, "ErrorText",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'>"
           "<item-not-found/>"
           "<text xml:lang='en'>ErrorText</text>"
         "</error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::UNEXPECTED_REQUEST, "",
       "<iq xmlns='jabber:client' "
           "to='user@gmail.com/chromoting016DBB07' id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'><unexpected-request/></error>"
       "</iq>",
       kTestIncomingMessage1},
      {JingleMessageReply::INVALID_SID, "ErrorText",
       "<iq xmlns='jabber:client' to='remoting@bot.talk.google.com' "
           "id='4' type='error'>"
         "<jingle action='session-terminate' sid='2227053353' "
                 "xmlns='urn:xmpp:jingle:1'>"
           "<reason><success/></reason>"
         "</jingle>"
         "<error type='modify'>"
           "<item-not-found/>"
           "<text xml:lang='en'>ErrorText</text>"
         "</error>"
       "</iq>",
       kTestIncomingMessage2},
      {std::nullopt, "",
       "<iq xmlns='jabber:client' to='remoting@bot.talk.google.com' "
           "id='4' type='result'>"
         "<jingle xmlns='urn:xmpp:jingle:1'/>"
       "</iq>",
       kTestIncomingMessage2},
  });
  // clang-format on

  for (size_t i = 0; i < std::size(tests); ++i) {
    std::unique_ptr<XmlElement> incoming_message(
        XmlElement::ForStr(tests[i].incoming_message.c_str()));
    ASSERT_TRUE(incoming_message.get());

    SCOPED_TRACE(testing::Message() << "Running test case: " << i);
    JingleMessageReply reply_msg;
    if (tests[i].error) {
      if (tests[i].error_text.empty()) {
        reply_msg = JingleMessageReply(*tests[i].error);
      } else {
        reply_msg = JingleMessageReply(*tests[i].error, tests[i].error_text);
      }
    } else {
      reply_msg = JingleMessageReply();
    }
    std::unique_ptr<XmlElement> reply(
        JingleMessageReplyToXml(reply_msg, incoming_message.get()));

    std::unique_ptr<XmlElement> expected(
        XmlElement::ForStr(tests[i].expected_text.c_str()));
    ASSERT_TRUE(expected.get());

    std::string error;
    EXPECT_TRUE(VerifyXml(expected.get(), reply.get(), &error)) << error;
  }
}

TEST(JingleMessageTest, RemotingErrorCode) {
  // clang-format off
  const char* kTestSessionTerminateMessageBegin =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='session-terminate' sid='2227053353' "
                "xmlns='urn:xmpp:jingle:1'>"
          "<reason><decline/></reason>"
          "<gr:error-code xmlns:gr='google:remoting'>";
  const char* kTestSessionTerminateMessageEnd =
          "</gr:error-code>"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  for (int i = static_cast<int>(ErrorCode::OK);
       i <= static_cast<int>(ErrorCode::ERROR_CODE_MAX); i++) {
    ErrorCode error = static_cast<ErrorCode>(i);
    std::string message_str = kTestSessionTerminateMessageBegin;
    message_str.append(ErrorCodeToString(error));
    message_str.append(kTestSessionTerminateMessageEnd);
    JingleMessage message;
    if (error == ErrorCode::UNKNOWN_ERROR) {
      // We do not include UNKNOWN_ERROR in xml output, so VerifyXml will fail.
      std::unique_ptr<XmlElement> source_message(
          XmlElement::ForStr(message_str.c_str()));
      std::string parse_error;
      EXPECT_TRUE(
          JingleMessageFromXml(source_message.get(), &message, &parse_error));
    } else {
      ParseFormatAndCompare(message_str.c_str(), &message);
    }

    EXPECT_EQ(message.action(), ActionType::kSessionTerminate);
    EXPECT_EQ(message.reason, SessionTerminate::Reason::kDecline);
    EXPECT_EQ(message.error_code, error);
  }
}

TEST(JingleMessageTest, Attachment) {
  // clang-format off
  const char* kAttachmentXml =
      "<gr:attachments xmlns:gr='google:remoting'>"
        "<gr:host-attributes>"
          "Debug-Build,ChromeBrand,Win10+"
        "</gr:host-attributes>"
        "<gr:host-configuration>"
          "Detect-Updated-Region:true,SomeOtherSetting:Value"
        "</gr:host-configuration>"
        "<gr:unrecognized-tag>Some value</gr:unrecognized-tag>"
      "</gr:attachments>";
  // clang-format on

  std::unique_ptr<XmlElement> source_xml(XmlElement::ForStr(kAttachmentXml));
  ASSERT_TRUE(source_xml);

  Attachment attachment;
  EXPECT_TRUE(AttachmentFromXml(source_xml.get(), &attachment));

  ASSERT_TRUE(attachment.host_attributes);
  EXPECT_THAT(attachment.host_attributes->attribute,
              testing::ElementsAre("Debug-Build", "ChromeBrand", "Win10+"));

  ASSERT_TRUE(attachment.host_config);
  EXPECT_EQ(attachment.host_config->settings.size(), 2U);
  EXPECT_EQ(attachment.host_config->settings["Detect-Updated-Region"], "true");
  EXPECT_EQ(attachment.host_config->settings["SomeOtherSetting"], "Value");

  // Format back to XML and verify. Note that unrecognized-tag is lost.
  std::unique_ptr<XmlElement> formatted_xml(AttachmentToXml(attachment));
  ASSERT_TRUE(formatted_xml);

  EXPECT_TRUE(formatted_xml->FirstNamed(
      jingle_xmpp::QName("google:remoting", "host-attributes")));
  EXPECT_TRUE(formatted_xml->FirstNamed(
      jingle_xmpp::QName("google:remoting", "host-configuration")));
  EXPECT_FALSE(formatted_xml->FirstNamed(
      jingle_xmpp::QName("google:remoting", "unrecognized-tag")));
}

TEST(JingleMessageTest, AttachmentsMessage) {
  // Ordering of the "attachments" tag and other tags are irrelevant. But the
  // JingleMessage implementation always puts it before other tags, so we do the
  // same thing in test cases.
  // clang-format off
  static constexpr char kMessageWithPluginTag[] =
      "<cli:iq from='user@gmail.com/chromoting016DBB07' "
              "to='user@gmail.com/chromiumsy5C6A652D' type='set' "
              "xmlns:cli='jabber:client'>"
        "<jingle action='$1' sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
          "<gr:attachments xmlns:gr='google:remoting'>"
            "<gr:host-attributes>Debug-Build</gr:host-attributes>"
          "</gr:attachments>"
          "$2"
        "</jingle>"
      "</cli:iq>";
  // clang-format on

  for (int i = static_cast<int>(ActionType::kSessionInitiate);
       i <= static_cast<int>(ActionType::kTransportInfo); i++) {
    ActionType action_type = static_cast<ActionType>(i);
    std::vector<std::string> substitutes = {
        JingleMessage::GetActionName(action_type)};

    if (action_type == ActionType::kSessionInitiate ||
        action_type == ActionType::kSessionAccept) {
      // clang-format off
      substitutes.push_back(
          "<content creator='initiator' name='chromoting'>"
            "<description xmlns='google:remoting'>"
              "<authentication>"
                "<spake-message>RmFrZSBhdXRoIHRva2VuCg==</spake-message>"
              "</authentication>"
            "</description>"
            "<transport xmlns='google:remoting:webrtc' />"
          "</content>");
      // clang-format on
    } else if (action_type == ActionType::kSessionInfo) {
      substitutes.push_back("<test-info>test-message</test-info>");
    } else if (action_type == ActionType::kSessionTerminate) {
      substitutes.push_back("<reason><success/></reason>");
    } else if (action_type == ActionType::kTransportInfo) {
      // clang-format off
      substitutes.push_back(
          "<content name='chromoting' creator='initiator'>"
            "<transport xmlns='google:remoting:webrtc'>"
              "<candidate sdpMid='event' sdpMLineIndex='0'>"
                "candidate:1 1 udp 2122194688 172.23.164.186 59089 typ host generation 0"
              "</candidate>"
            "</transport>"
          "</content>");
      // clang-format on
    } else {
      substitutes.push_back("");
    }

    std::string message_str = base::ReplaceStringPlaceholders(
        kMessageWithPluginTag, substitutes, nullptr);

    JingleMessage message;
    ParseFormatAndCompare(message_str.c_str(), &message);

    EXPECT_EQ(message.action(), action_type);
    ASSERT_EQ(message.attachments.size(), 1U);
    ASSERT_TRUE(message.attachments[0].host_attributes);
    EXPECT_THAT(message.attachments[0].host_attributes->attribute,
                testing::ElementsAre("Debug-Build"));
  }
}

TEST(JingleMessageTest, ActionHelpers) {
  EXPECT_EQ(JingleMessage::GetActionName(ActionType::kSessionInitiate),
            "session-initiate");
  EXPECT_EQ(JingleMessage::GetActionName(ActionType::kSessionAccept),
            "session-accept");
  EXPECT_EQ(JingleMessage::GetActionName(ActionType::kSessionTerminate),
            "session-terminate");
  EXPECT_EQ(JingleMessage::GetActionName(ActionType::kSessionInfo),
            "session-info");
  EXPECT_EQ(JingleMessage::GetActionName(ActionType::kTransportInfo),
            "transport-info");

  EXPECT_EQ(JingleMessage::ActionFromPayload(std::monostate()),
            ActionType::kUnknownAction);
  EXPECT_EQ(JingleMessage::ActionFromPayload(SessionInitiate()),
            ActionType::kSessionInitiate);
  EXPECT_EQ(JingleMessage::ActionFromPayload(SessionAccept()),
            ActionType::kSessionAccept);
  EXPECT_EQ(JingleMessage::ActionFromPayload(SessionInfo()),
            ActionType::kSessionInfo);
  EXPECT_EQ(JingleMessage::ActionFromPayload(JingleTransportInfo()),
            ActionType::kTransportInfo);
  EXPECT_EQ(JingleMessage::ActionFromPayload(SessionTerminate()),
            ActionType::kSessionTerminate);
}

TEST(JingleMessageTest, CopyAndAssign) {
  JingleMessage message;
  message.sid = "test_sid";
  message.description =
      std::make_unique<ContentDescription>(JingleAuthentication());

  JingleMessage copy(message);
  EXPECT_EQ(copy.sid, "test_sid");
  ASSERT_TRUE(copy.description);
  EXPECT_NE(copy.description.get(), message.description.get());

  JingleMessage assign;
  assign = message;
  EXPECT_EQ(assign.sid, "test_sid");
  ASSERT_TRUE(assign.description);
  EXPECT_NE(assign.description.get(), message.description.get());

  message.description.reset();
  assign = message;
  EXPECT_FALSE(assign.description);
}

TEST(JingleAuthenticationTest, IsEmpty) {
  JingleAuthentication auth;
  EXPECT_TRUE(auth.is_empty());

  auth.supported_methods = {
      AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519};
  EXPECT_FALSE(auth.is_empty());
  auth.supported_methods.clear();

  auth.method = AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
  EXPECT_FALSE(auth.is_empty());
  auth.method.reset();

  auth.spake_message = {1};
  EXPECT_FALSE(auth.is_empty());
  auth.spake_message.clear();

  auth.verification_hash = {1};
  EXPECT_FALSE(auth.is_empty());
  auth.verification_hash.clear();

  auth.certificate = {1};
  EXPECT_FALSE(auth.is_empty());
  auth.certificate.clear();

  auth.pairing_info.emplace();
  EXPECT_FALSE(auth.is_empty());
  auth.pairing_info.reset();

  auth.id = "test";
  EXPECT_FALSE(auth.is_empty());
  auth.id.clear();

  auth.session_authz_host_token = "test";
  EXPECT_FALSE(auth.is_empty());
  auth.session_authz_host_token.clear();

  auth.session_authz_session_token = "test";
  EXPECT_FALSE(auth.is_empty());
  auth.session_authz_session_token.clear();

  auth.pairing_error = "test";
  EXPECT_FALSE(auth.is_empty());
  auth.pairing_error.clear();

  auth.test_id = "test";
  EXPECT_FALSE(auth.is_empty());
  auth.test_id.clear();

  auth.test_key = {1};
  EXPECT_FALSE(auth.is_empty());
}

TEST(IceTransportInfoTest, NamedCandidate_Constructor) {
  webrtc::Candidate candidate;
  IceTransportInfo::NamedCandidate named("test_name", candidate, 5);
  EXPECT_EQ(named.name, "test_name");
  EXPECT_EQ(named.sdp_m_line_index, 5);
}

}  // namespace remoting::protocol

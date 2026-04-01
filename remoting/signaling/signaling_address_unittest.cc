// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_address.h"

#include <memory>

#include "remoting/signaling/corp_messaging_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;

namespace remoting {

namespace {

std::unique_ptr<jingle_xmpp::XmlElement> GetEmptyJingleMessage() {
  return std::unique_ptr<
      jingle_xmpp::XmlElement>(jingle_xmpp::XmlElement::ForStr(
      "<iq xmlns='jabber:client'><jingle xmlns='urn:xmpp:jingle:1'/></iq>"));
}

constexpr char kLcsAddress[] =
    "user@domain.com/chromoting_lcs_KkMKIDB5NldsZndLalZZamZWVTZlYmhPT1RBa2p2TUl"
    "GX0lvEKT-HRhwIhB1V3QxYVkwdUptWlc3bnIxKVYHxmgZQ7i7";

constexpr char kFtlRegistrationId[] = "f6b43f10-566e-11e9-8647-d663bd873d93";

constexpr char kFtlAddress[] =
    "user@domain.com/chromoting_ftl_f6b43f10-566e-11e9-8647-d663bd873d93";

}  // namespace

TEST(SignalingAddressTest, ParseAddress) {
  const char kTestMessage[] =
      "<cli:iq"
      "      from='user@domain.com/chromoting_ftl_sender' "
      "      to='remoting@bot.talk.google.com' type='set' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM);
  EXPECT_FALSE(from.empty());

  EXPECT_EQ(from.id(), "user@domain.com/chromoting_ftl_sender");
  EXPECT_EQ(from.channel(), SignalingAddress::Channel::FTL);

  SignalingAddress to =
      SignalingAddress::Parse(message.get(), SignalingAddress::TO);
  EXPECT_FALSE(to.empty());

  EXPECT_EQ(to.id(), "remoting@bot.talk.google.com");
  EXPECT_EQ(to.channel(), SignalingAddress::Channel::XMPP);
}

TEST(SignalingAddressTest, ParseEmptySignalingId) {
  // Parse a message with invalid from field.
  const char kTestMessage[] =
      "<cli:iq from='' "
      "to='user@gmail.com/chromoting_ftl_receiver' type='result' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM);
  EXPECT_TRUE(from.empty());
}

TEST(SignalingAddressTest, ParseMissingSignalingId) {
  // Parse a message with a missing signaling ID field.
  const char kTestMessage[] =
      "<cli:iq to='user@gmail.com/chromoting_ftl_receiver' type='set' "
      "      xmlns:cli='jabber:client'>"
      "  <jingle action='session-info' "
      "        sid='2227053353' xmlns='urn:xmpp:jingle:1'>"
      "  </jingle>"
      "</cli:iq>";

  std::unique_ptr<jingle_xmpp::XmlElement> message(
      jingle_xmpp::XmlElement::ForStr(kTestMessage));

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM);
  EXPECT_TRUE(from.empty());
}

TEST(SignalingAddressTest, SetInMessageToXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/chromoting12345");
  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ(message->Attr(QName("", "to")), "user@domain.com/chromoting12345");
}

TEST(SignalingAddressTest, SetInMessageToFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);

  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ(message->Attr(QName("", "to")), kFtlAddress);
}

TEST(SignalingAddressTest, SetInMessageFromXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/resource");
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ(message->Attr(QName("", "from")), "user@domain.com/resource");
}

TEST(SignalingAddressTest, SetInMessageFromFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ(message->Attr(QName("", "from")), kFtlAddress);
}

TEST(SignalingAddressTest, CreateFtlSignalingAddress) {
  SignalingAddress addr = SignalingAddress::CreateFtlSignalingAddress(
      "user@domain.com", kFtlRegistrationId);
  EXPECT_EQ(addr.id(), kFtlAddress);

  std::string username;
  std::string registration_id;
  EXPECT_TRUE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_EQ(username, "user@domain.com");
  EXPECT_EQ(registration_id, kFtlRegistrationId);
}

TEST(SignalingAddressTest, GetFtlInfo_NotFtlInfo) {
  SignalingAddress addr(kLcsAddress);

  std::string username;
  std::string registration_id;
  EXPECT_FALSE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_TRUE(username.empty());
  EXPECT_TRUE(registration_id.empty());
}

TEST(SignalingAddressTest, CorpAddress) {
  const char kCorpToken[] = "this_is_a_corp_token_without_at_or_slash";
  SignalingAddress addr(kCorpToken);
  EXPECT_EQ(addr.channel(), SignalingAddress::Channel::CORP);
  EXPECT_EQ(addr.id(), kCorpToken);

  const char kCorpUserWithResource[] =
      "user@corp.google.com/some-uuid-resource";
  SignalingAddress addr1(kCorpUserWithResource);
  EXPECT_EQ(addr1.channel(), SignalingAddress::Channel::CORP);
  EXPECT_EQ(addr1.id(), kCorpUserWithResource);

  const char kCorpServiceWithoutResource[] = "some-uuid@type.corp.google.com";
  SignalingAddress addr2(kCorpServiceWithoutResource);
  EXPECT_EQ(addr2.channel(), SignalingAddress::Channel::CORP);
  EXPECT_EQ(addr2.id(), kCorpServiceWithoutResource);

  const char kCorpServiceWithResource[] =
      "some-uuid@type.corp.google.com/some-resource";
  SignalingAddress addr3(kCorpServiceWithResource);
  EXPECT_EQ(addr3.channel(), SignalingAddress::Channel::CORP);
  EXPECT_EQ(addr3.id(), kCorpServiceWithResource);
}

TEST(SignalingAddressTest, JidWithoutResourceIsXmpp) {
  const char kJid[] = "user@domain.com";
  SignalingAddress addr(kJid);
  EXPECT_EQ(addr.channel(), SignalingAddress::Channel::XMPP);
  EXPECT_EQ(addr.id(), kJid);
}

TEST(SignalingAddressTest, CreateFtlSystemAddress) {
  const char kSystemSenderId[] = "chromoting-backend-service";
  SignalingAddress addr =
      SignalingAddress::CreateFtlSystemAddress(kSystemSenderId);
  EXPECT_EQ(addr.channel(), SignalingAddress::Channel::FTL);
  EXPECT_TRUE(addr.is_system());
  EXPECT_EQ(addr.id(), kSystemSenderId);
}

}  // namespace remoting

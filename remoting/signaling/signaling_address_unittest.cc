// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_address.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;

namespace remoting {

namespace {

std::unique_ptr<jingle_xmpp::XmlElement> GetEmptyJingleMessage() {
  return std::unique_ptr<jingle_xmpp::XmlElement>(jingle_xmpp::XmlElement::ForStr(
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
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_FALSE(from.empty());
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("user@domain.com/chromoting_ftl_sender", from.id());
  EXPECT_EQ(SignalingAddress::Channel::FTL, from.channel());

  SignalingAddress to =
      SignalingAddress::Parse(message.get(), SignalingAddress::TO, &error);
  EXPECT_FALSE(to.empty());
  EXPECT_TRUE(error.empty());

  EXPECT_EQ("remoting@bot.talk.google.com", to.id());
  EXPECT_EQ(SignalingAddress::Channel::XMPP, to.channel());
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
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_TRUE(from.empty());
  EXPECT_TRUE(error.empty());
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
  std::string error;

  SignalingAddress from =
      SignalingAddress::Parse(message.get(), SignalingAddress::FROM, &error);
  EXPECT_TRUE(from.empty());
  EXPECT_TRUE(error.empty());
}

TEST(SignalingAddressTest, SetInMessageToXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/chromoting12345");
  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ("user@domain.com/chromoting12345", message->Attr(QName("", "to")));
}

TEST(SignalingAddressTest, SetInMessageToFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);

  addr.SetInMessage(message.get(), SignalingAddress::TO);
  EXPECT_EQ(kFtlAddress, message->Attr(QName("", "to")));
}

TEST(SignalingAddressTest, SetInMessageFromXmpp) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr("user@domain.com/resource");
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ("user@domain.com/resource", message->Attr(QName("", "from")));
}

TEST(SignalingAddressTest, SetInMessageFromFtl) {
  std::unique_ptr<jingle_xmpp::XmlElement> message = GetEmptyJingleMessage();
  SignalingAddress addr(kFtlAddress);
  addr.SetInMessage(message.get(), SignalingAddress::FROM);
  EXPECT_EQ(kFtlAddress, message->Attr(QName("", "from")));
}

TEST(SignalingAddressTest, CreateFtlSignalingAddress) {
  SignalingAddress addr = SignalingAddress::CreateFtlSignalingAddress(
      "user@domain.com", kFtlRegistrationId);
  EXPECT_EQ(kFtlAddress, addr.id());

  std::string username;
  std::string registration_id;
  EXPECT_TRUE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_EQ("user@domain.com", username);
  EXPECT_EQ(kFtlRegistrationId, registration_id);
}

TEST(SignalingAddressTest, GetFtlInfo_NotFtlInfo) {
  SignalingAddress addr(kLcsAddress);

  std::string username;
  std::string registration_id;
  EXPECT_FALSE(addr.GetFtlInfo(&username, &registration_id));
  EXPECT_TRUE(username.empty());
  EXPECT_TRUE(registration_id.empty());
}

}  // namespace remoting

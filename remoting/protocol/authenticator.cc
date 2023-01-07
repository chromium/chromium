// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator.h"

#include "remoting/base/constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

namespace {
const jingle_xmpp::StaticQName kAuthenticationQName = { kChromotingXmlNamespace,
                                                 "authentication" };
}  // namespace

// static
bool Authenticator::IsAuthenticatorMessage(const jingle_xmpp::XmlElement* message) {
  return message->Name() == kAuthenticationQName;
}

// static
std::unique_ptr<jingle_xmpp::XmlElement>
Authenticator::CreateEmptyAuthenticatorMessage() {
  return std::make_unique<jingle_xmpp::XmlElement>(kAuthenticationQName);
}

// static
const jingle_xmpp::XmlElement* Authenticator::FindAuthenticatorMessage(
    const jingle_xmpp::XmlElement* message) {
  return message->FirstNamed(kAuthenticationQName);
}

}  // namespace remoting::protocol

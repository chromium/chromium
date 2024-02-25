// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

namespace {
const jingle_xmpp::StaticQName kAuthenticationQName = {kChromotingXmlNamespace,
                                                       "authentication"};
}  // namespace

Authenticator::Authenticator() = default;
Authenticator::~Authenticator() = default;

// static
bool Authenticator::IsAuthenticatorMessage(
    const jingle_xmpp::XmlElement* message) {
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

void Authenticator::NotifyStateChangeAfterAccepted() {
  if (on_state_change_after_accepted_) {
    on_state_change_after_accepted_.Run();
  } else {
    LOG(WARNING)
        << "State change notification ignored because callback is not set.";
  }
}

void Authenticator::ChainStateChangeAfterAcceptedWithUnderlying(
    Authenticator& underlying) {
  underlying.set_state_change_after_accepted_callback(base::BindRepeating(
      &Authenticator::NotifyStateChangeAfterAccepted, base::Unretained(this)));
}

}  // namespace remoting::protocol

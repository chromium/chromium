// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signal_strategy.h"

#include "base/memory/ptr_util.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// static
std::unique_ptr<jingle_xmpp::XmlElement> SignalStrategy::GetXmlStanza(
    const SignalingMessage& message) {
  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (ftl_message && ftl_message->has_xmpp() &&
      ftl_message->xmpp().has_stanza()) {
    return base::WrapUnique<jingle_xmpp::XmlElement>(
        jingle_xmpp::XmlElement::ForStr(ftl_message->xmpp().stanza()));
  }

  const internal::PeerMessageStruct* peer_message =
      std::get_if<internal::PeerMessageStruct>(&message);
  if (peer_message) {
    const auto* iq_stanza_struct =
        std::get_if<internal::IqStanzaStruct>(&peer_message->payload);
    if (iq_stanza_struct) {
      return base::WrapUnique<jingle_xmpp::XmlElement>(
          jingle_xmpp::XmlElement::ForStr(iq_stanza_struct->xml));
    }
  }

  return nullptr;
}

bool SignalStrategy::Listener::OnSignalStrategyIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  return false;
}

bool SignalStrategy::IsSignInError() const {
  return false;
}

}  // namespace remoting

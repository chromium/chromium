// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signal_strategy.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/memory/ptr_util.h"
#include "base/observer_list_types.h"
#include "remoting/base/logging.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_message.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// static
std::optional<SignalingMessage> SignalStrategy::ParseStanzaXml(
    const std::string& xml) {
  auto stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
      jingle_xmpp::XmlElement::ForStr(xml));
  if (!stanza) {
    LOG(WARNING) << "Failed to parse XMPP: " << xml;
    return std::nullopt;
  }

  HOST_LOG << "Received incoming stanza:\n"
           << stanza->Str()
           << "\n=========================================================";

  SignalingAddress sender_address_from_iq =
      SignalingAddress::Parse(stanza.get(), SignalingAddress::FROM);
  if (sender_address_from_iq.empty()) {
    LOG(WARNING) << "Received stanza with invalid sender.";
    return std::nullopt;
  }

  JingleMessage jingle_message;
  std::string error;
  if (JingleMessageFromXml(stanza.get(), &jingle_message, &error)) {
    return SignalingMessage(std::move(jingle_message));
  }

  JingleMessageReply jingle_reply;
  if (JingleMessageReplyFromXml(stanza.get(), &jingle_reply)) {
    jingle_reply.message_id = stanza->Attr(kQNameId);
    jingle_reply.from =
        SignalingAddress::Parse(stanza.get(), SignalingAddress::FROM);
    jingle_reply.to =
        SignalingAddress::Parse(stanza.get(), SignalingAddress::TO);
    return SignalingMessage(std::move(jingle_reply));
  }

  LOG(WARNING) << "Failed to parse incoming Jingle message or reply: " << error;
  return std::nullopt;
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

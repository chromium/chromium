// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_

#include <memory>
#include <string>

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

struct IceTransportInfo;
class JingleMessage;

// Converts between JingleMessage and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageToXml(
    const JingleMessage& message);
bool JingleMessageFromXml(const jingle_xmpp::XmlElement* stanza,
                          JingleMessage* message,
                          std::string* error);

// Helper to check if an XML element represents a Jingle message.
bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza);

// Converts between IceTransportInfo and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport);
bool IceTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                             IceTransportInfo* transport);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_

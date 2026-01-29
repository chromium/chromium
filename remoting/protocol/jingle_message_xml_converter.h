// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_

#include <memory>

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

struct IceTransportInfo;

// Converts between IceTransportInfo and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport);
bool IceTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                             IceTransportInfo* transport);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGE_XML_CONVERTER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_JINGLE_MESSAGE_XML_CONVERTER_H_
#define REMOTING_SIGNALING_JINGLE_MESSAGE_XML_CONVERTER_H_

#include <memory>
#include <string>

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {
struct Attachment;
class ContentDescription;
struct IceTransportInfo;
struct JingleAuthentication;
class JingleMessage;
struct JingleMessageReply;
struct JingleTransportInfo;
struct SessionInfo;
}  // namespace remoting

namespace remoting {

// Converts between JingleMessage and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageToXml(
    const JingleMessage& message);
bool JingleMessageFromXml(const jingle_xmpp::XmlElement* stanza,
                          JingleMessage* message,
                          std::string* error);

// Converts between JingleMessageReply and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply);
std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply,
    const JingleMessage& original_message);
std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply,
    const jingle_xmpp::XmlElement* request_stanza);
bool JingleMessageReplyFromXml(const jingle_xmpp::XmlElement* stanza,
                               JingleMessageReply* reply);

// Helper to check if an XML element represents a Jingle message.
bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza);

// Converts between JingleTransportInfo and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> JingleTransportInfoToXml(
    const JingleTransportInfo& transport);
bool JingleTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                                JingleTransportInfo* transport);

// Converts between IceTransportInfo and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport);
bool IceTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                             IceTransportInfo* transport);

// Converts between Attachment and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> AttachmentToXml(
    const Attachment& attachment);
bool AttachmentFromXml(const jingle_xmpp::XmlElement* element,
                       Attachment* attachment);

// Converts between JingleAuthentication and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> JingleAuthenticationToXml(
    const JingleAuthentication& authentication);
bool JingleAuthenticationFromXml(const jingle_xmpp::XmlElement* element,
                                 JingleAuthentication* authentication);

// Appends SessionInfo children to the given Jingle element.
void SessionInfoToXml(const SessionInfo& session_info,
                      jingle_xmpp::XmlElement* jingle_element);

// Parses SessionInfo from the given Jingle element.
bool SessionInfoFromXml(const jingle_xmpp::XmlElement* jingle_element,
                        SessionInfo* session_info);

// Converts between ContentDescription and its XML representation.
std::unique_ptr<jingle_xmpp::XmlElement> ContentDescriptionToXml(
    const ContentDescription& description);
std::unique_ptr<ContentDescription> ContentDescriptionFromXml(
    const jingle_xmpp::XmlElement* element,
    bool webrtc_transport);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_JINGLE_MESSAGE_XML_CONVERTER_H_

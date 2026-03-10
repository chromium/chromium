// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_JINGLE_MESSAGE_PROTO_CONVERTER_H_
#define REMOTING_SIGNALING_JINGLE_MESSAGE_PROTO_CONVERTER_H_

#include <memory>
#include <string>

#include "remoting/proto/ftl/v1/xmpp.pb.h"

namespace remoting {

class JingleMessage;
struct JingleMessageReply;

// Converts between JingleMessage and its proto representation.
ftl::IqStanza JingleMessageToProto(const JingleMessage& message);
bool JingleMessageFromProto(const ftl::IqStanza& stanza,
                            JingleMessage* message,
                            std::string* error);

// Converts between JingleMessageReply and its proto representation.
ftl::IqStanza JingleMessageReplyToProto(const JingleMessageReply& reply);
bool JingleMessageReplyFromProto(const ftl::IqStanza& stanza,
                                 JingleMessageReply* reply);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_JINGLE_MESSAGE_PROTO_CONVERTER_H_

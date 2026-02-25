// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_MESSAGE_H_
#define REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

#include <memory>
#include <variant>

#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/messaging_service.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

using SignalingMessage = std::variant<ftl::ChromotingMessage,
                                      internal::PeerMessageStruct,
                                      std::unique_ptr<jingle_xmpp::XmlElement>>;

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

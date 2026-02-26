// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_MESSAGE_H_
#define REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

#include <memory>
#include <variant>

#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/jingle_data_structures.h"

namespace remoting {

// TODO: joedow - Move ChromotingMessage and PeerMessageStruct out of this
// type and into the signal strategies which use them.
using SignalingMessage = std::variant<ftl::ChromotingMessage,
                                      internal::PeerMessageStruct,
                                      JingleMessage,
                                      JingleMessageReply>;

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_MESSAGE_H_
#define REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

#include <variant>

#include "remoting/signaling/jingle_data_structures.h"

namespace remoting {

// TODO: joedow - Consolidate the JingleMessage types.
using SignalingMessage = std::variant<JingleMessage, JingleMessageReply>;

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_MESSAGE_H_

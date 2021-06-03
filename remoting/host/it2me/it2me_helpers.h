// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_
#define REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_

#include <string>

namespace base {
class Value;
}

namespace remoting {

enum class It2MeHostState;

// Parses a serialized JSON message from either the CRD website client or an
// It2MeNativeMessageHost instance.  Returns true if the message was
// successfully parsed. If parsing fails, the out params will not be valid.
bool ParseIt2MeNativeMessageJson(const std::string& message,
                                 std::string& message_type,
                                 base::Value& parsed_message);

// Provides a human readable name for a given It2MeHostState. This is used both
// for logging and in host state changed JSON messages.
std::string It2MeHostStateToString(It2MeHostState host_state);

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_

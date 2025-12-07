// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CONSTANTS_H_
#define REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CONSTANTS_H_

#include <stdint.h>

namespace remoting {

// FourCC encoding the text "Bye!".
// The AgentProcess receiver will observe this disconnect reason if it is
// requested by the broker to terminate itself. If the receiver is
// disconnected without a reason code (i.e. custom_reason=0), then it implies
// that the broker might have exited unexpectedly (e.g. crashed).
inline constexpr uint32_t kTerminateAgentProcessBrokerReason = 1115252001;

}  // namespace remoting

#endif  // REMOTING_HOST_MAC_AGENT_PROCESS_BROKER_CONSTANTS_H_

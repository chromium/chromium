// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CAPABILITY_NAMES_H_
#define REMOTING_PROTOCOL_CAPABILITY_NAMES_H_

namespace remoting {
namespace protocol {

// Used for negotiating client-host capabilities for touch events.
constexpr char kTouchEventsCapability[] = "touchEvents";

constexpr char kSendInitialResolution[] = "sendInitialResolution";
constexpr char kRateLimitResizeRequests[] = "rateLimitResizeRequests";

constexpr char kFileTransferCapability[] = "fileTransfer";
constexpr char kRtcLogTransferCapability[] = "rtcLogTransfer";

// TODO(joedow): Ideally these would be dynamically created via the
// DataChannelManager, we should consider moving them there if we begin using
// WebRTC data channels for individual features more frequently.
constexpr char kLockWorkstationAction[] = "lockWorkstationAction";
constexpr char kSendAttentionSequenceAction[] = "sendAttentionSequenceAction";

// Host supports ICE or SDP restart request from control message. Only used for
// WebRTC clients.
constexpr char kWebrtcIceSdpRestartAction[] = "webrtcIceSdpRestartAction";

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CAPABILITY_NAMES_H_

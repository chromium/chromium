// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CAPABILITY_NAMES_H_
#define REMOTING_PROTOCOL_CAPABILITY_NAMES_H_

namespace remoting::protocol {

// Used for negotiating client-host capabilities for touch events.
inline constexpr char kTouchEventsCapability[] = "touchEvents";

inline constexpr char kSendInitialResolution[] = "sendInitialResolution";
inline constexpr char kRateLimitResizeRequests[] = "rateLimitResizeRequests";

inline constexpr char kFileTransferCapability[] = "fileTransfer";
inline constexpr char kRtcLogTransferCapability[] = "rtcLogTransfer";

inline constexpr char kRemoteOpenUrlCapability[] = "remoteOpenUrl";
inline constexpr char kRemoteWebAuthnCapability[] = "remoteWebAuthn";

// TODO(joedow): Ideally these would be dynamically created via the
// DataChannelManager, we should consider moving them there if we begin using
// WebRTC data channels for individual features more frequently.
inline constexpr char kLockWorkstationAction[] = "lockWorkstationAction";
inline constexpr char kSendAttentionSequenceAction[] =
    "sendAttentionSequenceAction";

// Host supports ICE or SDP restart request from control message. Only used for
// WebRTC clients.
inline constexpr char kWebrtcIceSdpRestartAction[] =
    "webrtcIceSdpRestartAction";

// Host supports creating one video-stream per monitor.
inline constexpr char kMultiStreamCapability[] = "multiStream";

// Host supports display layouts controlled by the client.
inline constexpr char kClientControlledLayoutCapability[] =
    "clientControlledLayout";

// Host supports injection of events with fractional coordinates.
inline constexpr char kFractionalCoordinatesCapability[] =
    "fractionalCoordinates";

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CAPABILITY_NAMES_H_

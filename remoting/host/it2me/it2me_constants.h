// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_
#define REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_

namespace remoting {

// ID used to identify the current message. Must be included in the response if
// the sender includes it.
extern const char kMessageId[];

// The type of the message received. The type is used to retrieve and validate
// the message payload.
extern const char kMessageType[];

// Initial message sent from the client to the host to request the host's
// version and supported features. It has no parameters.
extern const char kHelloMessage[];
// Hello response parameters.
extern const char kHostVersion[];
extern const char kSupportedFeatures[];
// Response sent back to the client after the Hello message has been handled.
extern const char kHelloResponse[];

// Sent from the client to the host to begin the connection process.
extern const char kConnectMessage[];
// Connect message parameters.
extern const char kUserName[];
extern const char kAuthServiceWithToken[];
extern const char kLocalJid[];
extern const char kDirectoryBotJidValue[];
extern const char kSuppressUserDialogs[];
extern const char kSuppressNotifications[];
extern const char kTerminateUponInput[];
extern const char kUseElevatedHost[];
extern const char kUseSignalingProxy[];
extern const char kIceConfig[];
// Response sent back to the client after the Connect message has been handled.
extern const char kConnectResponseConnect[];

// Message sent from the host to the client when the connection state changes.
// No response from the client is returned for this message.
extern const char kHostStateChangedMessage[];
// HostStateChanged message parameters.
extern const char kState[];
// Host state values which are associated with the |kState| field of the
// HostStateChanged message.
extern const char kHostStateError[];
extern const char kHostStateStarting[];
extern const char kHostStateRequestedAccessCode[];
extern const char kHostStateDomainError[];
extern const char kHostStateReceivedAccessCode[];
extern const char kHostStateDisconnected[];
extern const char kHostStateConnecting[];
extern const char kHostStateConnected[];
// Included in the message sent for ReceivedAccessCode state.
extern const char kAccessCode[];
extern const char kAccessCodeLifetime[];
// Included in the message sent for Connected state.
extern const char kClient[];

// Sent from the client to the host to disconnect the current connection.
extern const char kDisconnectMessage[];
// Response sent to the client after the Disconnect message has been handled.
extern const char kDisconnectResponse[];

// Sent from the client to the host when an IQ stanza has been received over the
// signaling channel. Only applies when a signaling proxy is used.
extern const char kIncomingIqMessage[];
// Response sent to the client after the incoming iq message has been handled.
extern const char kIncomingIqResponse[];

// Message sent from the host to the client when an IQ stanza is ready to be
// sent to the other end of the signaling channel. Only applies when a signaling
// proxy is used. No response from the client is returned for this message.
extern const char kSendOutgoingIqMessage[];

// Parameter used for both incoming and outgoing IQ messages.
extern const char kIq[];

// Generic message sent from the host to the client when an error occurs.
extern const char kErrorMessage[];
extern const char kErrorMessageCode[];
extern const char kErrorMessageDescription[];

// Sent from the host when there is a change in the NAT traversal policies.
extern const char kNatPolicyChangedMessage[];
extern const char kNatPolicyChangedMessageNatEnabled[];
extern const char kNatPolicyChangedMessageRelayEnabled[];

// Sent from the host when there is a problem reading the local policy.
extern const char kPolicyErrorMessage[];

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_
#define REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_

#include "remoting/host/native_messaging/native_messaging_constants.h"

namespace remoting {

// These state values are defined in the website client as well.  Remember to
// update both enums when making changes.
enum class It2MeHostState {
  kDisconnected,
  kStarting,
  kRequestedAccessCode,
  kReceivedAccessCode,
  kConnecting,
  kConnected,
  kError,
  kInvalidDomainError,
};

// Indicates that an OAuth access token can be provided to the host which will
// use it for service requests (e.g. ICE config, signaling, host registration).
extern const char kFeatureAccessTokenAuth[];

// Indicates that the host supports setting an authorized helper.
extern const char kFeatureAuthorizedHelper[];

// Sent from the client to the host to begin the connection process.
extern const char kConnectMessage[];
// Connect message parameters.
extern const char kUserName[];
extern const char kAuthServiceWithToken[];
extern const char kAccessToken[];
extern const char kSignalingAccessToken[];
extern const char kApiAccessToken[];
extern const char kLocalJid[];
extern const char kIsEnterpriseAdminUser[];
extern const char kUseElevatedHost[];
extern const char kIceConfig[];
extern const char kAuthorizedHelper[];
extern const char kIsCorpUser[];
// Response sent back to the client after the Connect message has been handled.
extern const char kConnectResponse[];

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
// Included in the message sent for the ReceivedAccessCode state.
extern const char kAccessCode[];
extern const char kAccessCodeLifetime[];
// Included in the message sent for the Connected state.
extern const char kClient[];
// Included in the message sent for the Disconnected state.
extern const char kDisconnectReason[];

// Sent from the client to the host to disconnect the current connection.
extern const char kDisconnectMessage[];
// Response sent to the client after the Disconnect message has been handled.
extern const char kDisconnectResponse[];

// Generic message sent from the host to the client when an error occurs.
extern const char kErrorMessage[];
extern const char kErrorMessageCode[];

// Sent from the host when there is a change in the NAT traversal policies.
extern const char kNatPolicyChangedMessage[];
extern const char kNatPolicyChangedMessageNatEnabled[];
extern const char kNatPolicyChangedMessageRelayEnabled[];

// Sent from the host when there is a problem reading the local policy.
extern const char kPolicyErrorMessage[];

// Sent from the website to notify that the OAuth access tokens have changed.
extern const char kUpdateAccessTokensMessage[];

// Keys used for storing and retrieving params used for reconnectable sessions.
extern const char kSessionParamsDict[];
extern const char kEnterpriseParamsDict[];
extern const char kReconnectParamsDict[];
extern const char kReconnectSupportId[];
extern const char kReconnectHostSecret[];
extern const char kReconnectPrivateKey[];
extern const char kReconnectFtlDeviceId[];
extern const char kReconnectClientFtlAddress[];

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_CONSTANTS_H_

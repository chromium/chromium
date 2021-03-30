// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_constants.h"

namespace remoting {

const char kMessageId[] = "id";
const char kMessageType[] = "type";

const char kHelloMessage[] = "hello";
const char kHostVersion[] = "version";
const char kSupportedFeatures[] = "supportedFeatures";
const char kHelloResponse[] = "helloResponse";

const char kConnectMessage[] = "connect";
const char kUserName[] = "userName";
const char kAuthServiceWithToken[] = "authServiceWithToken";
const char kLocalJid[] = "localJid";
const char kDirectoryBotJidValue[] = "remoting@bot.talk.google.com";
const char kSuppressUserDialogs[] = "suppressUserDialogs";
const char kSuppressNotifications[] = "suppressNotifications";
const char kTerminateUponInput[] = "terminateUponInput";
const char kUseElevatedHost[] = "useElevatedHost";
const char kUseSignalingProxy[] = "useSignalingProxy";
const char kIceConfig[] = "iceConfig";
const char kConnectResponseConnect[] = "connectResponse";

const char kHostStateChangedMessage[] = "hostStateChanged";
const char kState[] = "state";
const char kHostStateError[] = "ERROR";
const char kHostStateStarting[] = "STARTING";
const char kHostStateRequestedAccessCode[] = "REQUESTED_ACCESS_CODE";
const char kHostStateDomainError[] = "INVALID_DOMAIN_ERROR";
const char kHostStateReceivedAccessCode[] = "RECEIVED_ACCESS_CODE";
const char kHostStateDisconnected[] = "DISCONNECTED";
const char kHostStateConnecting[] = "CONNECTING";
const char kHostStateConnected[] = "CONNECTED";
const char kAccessCode[] = "accessCode";
const char kAccessCodeLifetime[] = "accessCodeLifetime";
const char kClient[] = "client";

const char kDisconnectMessage[] = "disconnect";
const char kDisconnectResponse[] = "disconnectResponse";

const char kIncomingIqMessage[] = "incomingIq";
const char kIncomingIqResponse[] = "incomingIqResponse";

const char kSendOutgoingIqMessage[] = "sendOutgoingIq";

const char kIq[] = "iq";

const char kErrorMessage[] = "error";
const char kErrorMessageCode[] = "error_code";
const char kErrorMessageDescription[] = "description";

const char kNatPolicyChangedMessage[] = "natPolicyChanged";
const char kNatPolicyChangedMessageNatEnabled[] = "natTraversalEnabled";
const char kNatPolicyChangedMessageRelayEnabled[] = "relayConnectionsAllowed";

const char kPolicyErrorMessage[] = "policyError";

}  // namespace remoting

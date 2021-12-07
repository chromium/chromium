// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

namespace remoting {

extern const char kRemoteWebAuthnDataChannelName[];

// NMH message types.
extern const char kIsUvpaaMessageType[];
extern const char kGetRemoteStateMessageType[];

// NMH message keys.
extern const char kIsUvpaaResponseIsAvailableKey[];
extern const char kGetRemoteStateResponseIsRemotedKey[];

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

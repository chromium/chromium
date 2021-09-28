// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace remoting {

extern const char kRemoteWebAuthnDataChannelName[];

// NMH message types.
extern const char kIsUvpaaMessageType[];

// NMH message keys.
extern const char kIsUvpaaResponseIsAvailableKey[];

const mojo::NamedPlatformChannel::ServerName& GetRemoteWebAuthnChannelName();

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

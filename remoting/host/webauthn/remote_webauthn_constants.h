// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

#include "base/containers/span.h"
#include "base/files/file_path.h"

namespace remoting {

extern const char kRemoteWebAuthnDataChannelName[];

// NMH message types.
extern const char kIsUvpaaMessageType[];
extern const char kGetRemoteStateMessageType[];
extern const char kCreateMessageType[];
extern const char kGetMessageType[];
extern const char kCancelMessageType[];
extern const char kClientDisconnectedMessageType[];

// NMH message keys.
extern const char kIsUvpaaResponseIsAvailableKey[];
extern const char kGetRemoteStateResponseIsRemotedKey[];
extern const char kGetRemoteStateResponseDesktopSessionTypeKey[];
extern const char kCancelResponseWasCanceledKey[];
extern const char kCreateRequestDataKey[];
extern const char kCreateResponseDataKey[];
extern const char kGetRequestDataKey[];
extern const char kGetResponseDataKey[];
extern const char kWebAuthnErrorKey[];
extern const char kWebAuthnErrorNameKey[];
extern const char kWebAuthnErrorMessageKey[];

// Returns the IDs of the extensions that are allowed to use the remote WebAuthn
// native messaging host. Used both to validate the calling extension and to
// name the remote state change wakeup files.
base::span<const base::FilePath::StringViewType>
GetRemoteWebAuthnExtensionIds();

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_CONSTANTS_H_

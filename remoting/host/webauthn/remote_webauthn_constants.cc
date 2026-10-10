// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_constants.h"

#include <array>

namespace remoting {

const char kRemoteWebAuthnDataChannelName[] = "remote-webauthn";

const char kIsUvpaaMessageType[] = "isUvpaa";
const char kGetRemoteStateMessageType[] = "getRemoteState";
const char kCreateMessageType[] = "create";
const char kGetMessageType[] = "get";
const char kCancelMessageType[] = "cancel";
const char kClientDisconnectedMessageType[] = "clientDisconnected";

const char kIsUvpaaResponseIsAvailableKey[] = "isAvailable";
const char kGetRemoteStateResponseIsRemotedKey[] = "isRemoted";
const char kGetRemoteStateResponseDesktopSessionTypeKey[] =
    "desktopSessionType";
const char kCancelResponseWasCanceledKey[] = "wasCanceled";
const char kCreateRequestDataKey[] = "requestData";
const char kCreateResponseDataKey[] = "responseData";
const char kGetRequestDataKey[] = "requestData";
const char kGetResponseDataKey[] = "responseData";
const char kWebAuthnErrorKey[] = "error";
const char kWebAuthnErrorNameKey[] = "name";
const char kWebAuthnErrorMessageKey[] = "message";

base::span<const base::FilePath::StringViewType>
GetRemoteWebAuthnExtensionIds() {
  static constexpr auto kIds = std::to_array<base::FilePath::StringViewType>({
      // LINT.IfChange(extension_ids)
      // Prod security key extension ID
      FILE_PATH_LITERAL("djjmngfglakhkhmgcfdmjalogilepkhd"),

      // Prod companion extension ID
      FILE_PATH_LITERAL("inomeogfingihgjfjlpeplalcfajhgai"),

  // Debug builds also include the dev extension IDs so that a locally built
  // host works with either the prod or the dev extension.
#if !defined(NDEBUG)
      // Dev security key extension ID
      FILE_PATH_LITERAL("kbapnajlciffffomeaphfpckfdcfopef"),

      // Dev companion extension ID
      FILE_PATH_LITERAL("pbnaomcgbfiofkfobmlhmdobjchjkphi"),
#endif
      // LINT.ThenChange(/remoting/host/BUILD.gn:extension_ids)
  });
  return kIds;
}

}  // namespace remoting

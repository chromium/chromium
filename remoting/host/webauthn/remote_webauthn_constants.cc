// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_constants.h"

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
const char kCancelResponseWasCanceledKey[] = "wasCanceled";
const char kCreateRequestDataKey[] = "requestData";
const char kCreateResponseDataKey[] = "responseData";
const char kGetRequestDataKey[] = "requestData";
const char kGetResponseDataKey[] = "responseData";
const char kWebAuthnErrorKey[] = "error";
const char kWebAuthnErrorNameKey[] = "name";
const char kWebAuthnErrorMessageKey[] = "message";

}  // namespace remoting

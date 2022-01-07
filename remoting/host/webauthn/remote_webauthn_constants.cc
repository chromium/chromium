// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_constants.h"

namespace remoting {

const char kRemoteWebAuthnDataChannelName[] = "remote-webauthn";

const char kIsUvpaaMessageType[] = "isUvpaa";
const char kGetRemoteStateMessageType[] = "getRemoteState";
const char kCreateMessageType[] = "create";

const char kIsUvpaaResponseIsAvailableKey[] = "isAvailable";
const char kGetRemoteStateResponseIsRemotedKey[] = "isRemoted";
const char kCreateRequestDataKey[] = "requestData";
const char kCreateResponseErrorNameKey[] = "errorName";
const char kCreateResponseDataKey[] = "responseData";

}  // namespace remoting

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_passkey_client.h"

namespace webauthn {

IOSPasskeyClient::RequestInfo::RequestInfo(std::string frame_id,
                                           std::string request_id)
    : frame_id(std::move(frame_id)), request_id(std::move(request_id)) {}

IOSPasskeyClient::RequestInfo::RequestInfo(
    IOSPasskeyClient::RequestInfo&& other) = default;
IOSPasskeyClient::RequestInfo::~RequestInfo() = default;

}  // namespace webauthn

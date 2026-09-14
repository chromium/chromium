// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_UTIL_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_UTIL_H_

#import <string>
#import <string_view>

#import "base/containers/flat_map.h"

namespace sync_pb {
class GetDeviceAuthorizationKeyRequest;
}  // namespace sync_pb

// Builds the content bindings map for device integrity attestation based on
// `request` and random `salt`.
base::flat_map<std::string, std::string> BuildDeviceIntegrityContentBindings(
    const sync_pb::GetDeviceAuthorizationKeyRequest& request,
    std::string_view salt);

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_UTIL_H_

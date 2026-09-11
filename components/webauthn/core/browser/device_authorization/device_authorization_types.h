// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"

namespace webauthn {

// Type aliases for nested device authorization protobuf messages.
using DeviceAuthorizationKey =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKey;
using DeviceAuthorizationKeys =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys;

// Callback types for device authorization operations.
using CreateDeviceAuthRequestCallback =
    base::OnceCallback<void(sync_pb::GetDeviceAuthorizationKeyRequest)>;
using FetchDeviceAuthKeysCallback =
    base::OnceCallback<void(std::optional<DeviceAuthorizationKeys>)>;

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_

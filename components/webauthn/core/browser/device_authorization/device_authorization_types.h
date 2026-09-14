// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_

#include <optional>
#include <variant>

#include "base/functional/callback.h"
#include "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"

namespace webauthn {

// Type aliases for nested device authorization protobuf messages.
using DeviceAuthorizationKey =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKey;
using DeviceAuthorizationKeys =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys;
using DeviceAuthorizationReAuthParams =
    sync_pb::GetDeviceAuthorizationKeyResponse::ReAuthParams;

// Describes the result of a device authorization key fetch operation.
// TODO(crbug.com/405036154): Define more granular errors if needed by callers.
struct DeviceAuthFetchResult {
  enum class Status {
    kSuccess,
    kReAuthRequired,
    kError,
  };

  Status status() const {
    if (std::holds_alternative<DeviceAuthorizationKeys>(result)) {
      return Status::kSuccess;
    }
    if (std::holds_alternative<DeviceAuthorizationReAuthParams>(result)) {
      return Status::kReAuthRequired;
    }
    return Status::kError;
  }

  const DeviceAuthorizationKeys* keys() const {
    return std::get_if<DeviceAuthorizationKeys>(&result);
  }

  const DeviceAuthorizationReAuthParams* reauth_params() const {
    return std::get_if<DeviceAuthorizationReAuthParams>(&result);
  }

  std::variant<std::monostate,
               DeviceAuthorizationKeys,
               DeviceAuthorizationReAuthParams>
      result;
};

// Callback types for device authorization operations.
using PopulatePlatformDataCallback =
    base::OnceCallback<void(sync_pb::GetDeviceAuthorizationKeyRequest)>;
using FetchDeviceAuthKeysCallback =
    base::OnceCallback<void(DeviceAuthFetchResult)>;

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_TYPES_H_

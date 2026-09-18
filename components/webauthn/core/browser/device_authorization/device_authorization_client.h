// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_CLIENT_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_CLIENT_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_types.h"

class GaiaId;

namespace webauthn {

// Abstracts device authorization keys operations that depend on the embedder's
// environment.
class DeviceAuthorizationClient {
 public:
  virtual ~DeviceAuthorizationClient() = default;

  // Asynchronously returns keys stored locally on the device for the given
  // `gaia_id`, or `std::nullopt` if there are none.
  virtual void GetCachedKeys(const GaiaId& gaia_id,
                             GetCachedKeysCallback callback) = 0;

  // Asynchronously persists fetched keys on the device for the given `gaia_id`.
  // Calls `callback` with true on success, false on failure.
  virtual void StoreKeys(const GaiaId& gaia_id,
                         const CachedDeviceAuthorizationKeys& keys,
                         StoreKeysCallback callback) = 0;

  // Asynchronously populates embedder-specific platform data (e.g. device
  // integrity signals) for the given `gaia_id` into `request`.
  virtual void PopulatePlatformData(
      const GaiaId& gaia_id,
      sync_pb::GetDeviceAuthorizationKeyRequest request,
      PopulatePlatformDataCallback callback) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_CLIENT_H_

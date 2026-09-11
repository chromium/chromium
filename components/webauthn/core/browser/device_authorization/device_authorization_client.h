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

  // Returns keys stored locally on the device for the given `gaia_id`, or
  // `std::nullopt` if there are none.
  // TODO(crbug.com/405036154): Make the APIs async.
  virtual std::optional<DeviceAuthorizationKeys> GetCachedKeys(
      const GaiaId& gaia_id) = 0;

  // Persists fetched keys on the device for the given `gaia_id`. Returns true
  // on success.
  virtual bool StoreKeys(const GaiaId& gaia_id,
                         const DeviceAuthorizationKeys& keys) = 0;

  // Asynchronously creates the device authorization request with
  // embedder-specific parameters (e.g. device integrity signals).
  // TODO(crbug.com/405036154): Allow specifying which params are needed.
  virtual void CreateDeviceAuthorizationRequest(
      CreateDeviceAuthRequestCallback callback) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_CLIENT_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_H_

#include <optional>

#include "components/keyed_service/core/keyed_service.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_types.h"

namespace webauthn {

// Handles device authorization keys operations including fetching from the
// server, storing on device, and dispatching logic to platform embedders.
class DeviceAuthorizationService : public KeyedService {
 public:
  ~DeviceAuthorizationService() override = default;

  // Retrieves device authorization keys from local cache if present and valid;
  // otherwise, fetches them from the server. Calls `callback` with keys on
  // success or `std::nullopt` on failure.
  // TODO(crbug.com/405036154): Pass a struct of options here (e.g. to
  // explicitly request ReAuth or specify fetch mode).
  virtual void GetOrFetchKeys(FetchDeviceAuthKeysCallback callback) = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_H_

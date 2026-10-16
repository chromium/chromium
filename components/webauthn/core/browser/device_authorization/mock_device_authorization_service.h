// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_MOCK_DEVICE_AUTHORIZATION_SERVICE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_MOCK_DEVICE_AUTHORIZATION_SERVICE_H_

#include <string>

#include "components/webauthn/core/browser/device_authorization/device_authorization_service.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webauthn {

// Mock `DeviceAuthorizationService` for testing.
class MockDeviceAuthorizationService : public DeviceAuthorizationService {
 public:
  MockDeviceAuthorizationService();
  ~MockDeviceAuthorizationService() override;

  MOCK_METHOD(void,
              GetOrFetchKeys,
              (FetchDeviceAuthKeysCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchKeysWithReAuthToken,
              (std::string reauth_proof_token,
               FetchDeviceAuthKeysCallback callback),
              (override));
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_MOCK_DEVICE_AUTHORIZATION_SERVICE_H_

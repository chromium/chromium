// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_

#include <memory>

#include "components/webauthn/core/browser/cmtg_device_key_provider.h"

namespace webauthn {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CmtgDeviceKeysResult)
enum class CmtgDeviceKeysResult {
  kSuccess = 0,
  kNetworkError = 1,
  kMaxValue = kNetworkError,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/webauthn/enums.xml:CmtgDeviceKeysResult)

// Default implementation of CmtgDeviceKeyProvider that vends device keys from
// Cryptauth. Currently implemented as a stub for testing and early integration
// that vends a constant randomly-generated key for the lifetime of the process.
class CryptauthCmtgDeviceKeyProvider : public CmtgDeviceKeyProvider {
 public:
  CryptauthCmtgDeviceKeyProvider();
  ~CryptauthCmtgDeviceKeyProvider() override;

  std::unique_ptr<Request> GetDeviceKeys(Callback callback) override;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_

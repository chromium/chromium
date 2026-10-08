// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace webauthn {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CmtgDeviceKeysResult)
enum class CmtgDeviceKeysResult {
  kSuccess = 0,
  kNetworkError = 1,
  kParseError = 2,
  kAccessTokenError = 3,
  kMaxValue = kAccessTokenError,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/webauthn/enums.xml:CmtgDeviceKeysResult)

// Default implementation of CmtgDeviceKeyProvider that vends device keys from
// the CryptAuth CMTG wrapper key service.
class CryptauthCmtgDeviceKeyProvider : public CmtgDeviceKeyProvider {
 public:
  // `url_loader_factory` must be non-null.
  CryptauthCmtgDeviceKeyProvider(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~CryptauthCmtgDeviceKeyProvider() override;

  // CmtgDeviceKeyProvider:
  std::unique_ptr<Request> GetDeviceKeys(Operation operation,
                                         Callback callback) override;

 private:
  const raw_ref<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_CRYPTAUTH_CMTG_DEVICE_KEY_PROVIDER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_TRUSTED_VAULT_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_TRUSTED_VAULT_CLIENT_H_

#include "components/trusted_vault/trusted_vault_client.h"

namespace ios_web_view {

// ChromeWebView implementation of TrustedVaultClient.
// This class uses the Chrome trusted vault service to store the shared keys.
class WebViewTrustedVaultClient : public trusted_vault::TrustedVaultClient {
 public:
  explicit WebViewTrustedVaultClient();
  ~WebViewTrustedVaultClient() override;

  // TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
          callback) override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> callback) override;
  void GetIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(bool)> callback) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure callback) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

  // Not copyable or movable
  WebViewTrustedVaultClient(const WebViewTrustedVaultClient&) = delete;
  WebViewTrustedVaultClient& operator=(const WebViewTrustedVaultClient&) =
      delete;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_TRUSTED_VAULT_CLIENT_H_

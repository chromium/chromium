// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_
#define IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_

#include "components/sync/driver/trusted_vault_client.h"

// iOS version of TrustedVaultClient. This class uses the Chrome trusted vault
// service to store the shared keys.
class IOSTrustedVaultClient : public syncer::TrustedVaultClient {
 public:
  IOSTrustedVaultClient();
  ~IOSTrustedVaultClient() override;

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
  void MarkKeysAsStale(const CoreAccountInfo& account_info,
                       base::OnceCallback<void(bool)> callback) override;
  void GetIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(bool)> callback) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure callback) override;

  // Not copyable or movable
  IOSTrustedVaultClient(const IOSTrustedVaultClient&) = delete;
  IOSTrustedVaultClient& operator=(const IOSTrustedVaultClient&) = delete;
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_

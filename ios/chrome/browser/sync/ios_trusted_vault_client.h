// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_
#define IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_

#include "components/sync/driver/trusted_vault_client.h"

class ChromeAccountManagerService;
class TrustedVaultClientBackend;
@protocol SystemIdentity;

// iOS version of TrustedVaultClient. This class uses the Chrome trusted vault
// service to store the shared keys.
class IOSTrustedVaultClient : public syncer::TrustedVaultClient {
 public:
  IOSTrustedVaultClient(ChromeAccountManagerService* account_manager_service,
                        TrustedVaultClientBackend* trusted_vault_service);
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
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> callback) override;
  void GetIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(bool)> callback) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure callback) override;
  void ClearDataForAccount(const CoreAccountInfo& account_info) override;

  // Not copyable or movable
  IOSTrustedVaultClient(const IOSTrustedVaultClient&) = delete;
  IOSTrustedVaultClient& operator=(const IOSTrustedVaultClient&) = delete;

 private:
  // Returns the identity for `account_info`.
  id<SystemIdentity> IdentityForAccount(const CoreAccountInfo& account_info);

  ChromeAccountManagerService* const account_manager_service_ = nullptr;
  TrustedVaultClientBackend* const backend_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_TRUSTED_VAULT_CLIENT_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_CLIENT_H_
#define IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_CLIENT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "components/trusted_vault/trusted_vault_client.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

class ChromeAccountManagerService;
class TrustedVaultClientBackend;
@protocol SystemIdentity;

// iOS version of TrustedVaultClient. This class uses the Chrome trusted vault
// service to store the shared keys.
class IOSTrustedVaultClient : public trusted_vault::TrustedVaultClient {
 public:
  IOSTrustedVaultClient(
      ChromeAccountManagerService* account_manager_service,
      signin::IdentityManager* identity_manager,
      TrustedVaultClientBackend* trusted_vault_service,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
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
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

  // Not copyable or movable
  IOSTrustedVaultClient(const IOSTrustedVaultClient&) = delete;
  IOSTrustedVaultClient& operator=(const IOSTrustedVaultClient&) = delete;

 private:
  // Returns the identity for `account_info`.
  id<SystemIdentity> IdentityForAccount(const CoreAccountInfo& account_info);

  const raw_ptr<ChromeAccountManagerService> account_manager_service_;
  const raw_ptr<TrustedVaultClientBackend> backend_;
  // TODO(crbug.com/343007092): Need to set the right security domain path.
  const trusted_vault::SecurityDomainId security_domain_id_;
  base::WeakPtrFactory<IOSTrustedVaultClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_CLIENT_H_

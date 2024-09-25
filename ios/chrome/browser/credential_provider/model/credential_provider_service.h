// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "ios/chrome/common/credential_provider/memory_credential_store.h"

class FaviconLoader;

@protocol MutableCredentialStore;

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace password_manager {
class AffiliatedMatchHelper;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

// A browser-context keyed service that is used to keep the Credential Provider
// Extension data up to date.
class CredentialProviderService
    : public KeyedService,
      public password_manager::PasswordStoreConsumer,
      public password_manager::PasswordStoreInterface::Observer,
      public signin::IdentityManager::Observer,
      public syncer::SyncServiceObserver,
      public webauthn::PasskeyModel::Observer {
 public:
  // Initializes the service.
  CredentialProviderService(
      PrefService* prefs,
      scoped_refptr<password_manager::PasswordStoreInterface>
          profile_password_store,
      scoped_refptr<password_manager::PasswordStoreInterface>
          account_password_store,
      webauthn::PasskeyModel* passkey_model,
      id<MutableCredentialStore> credential_store,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      affiliations::AffiliationService* affiliation_service,
      FaviconLoader* favicon_loader);

  CredentialProviderService(const CredentialProviderService&) = delete;
  CredentialProviderService& operator=(const CredentialProviderService&) =
      delete;

  ~CredentialProviderService() override;

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

 private:
  // Request all the credentials to sync them. Before adding the fresh ones,
  // the old ones are deleted.
  void RequestSyncAllCredentials();

  // Replaces all data with credentials created from the passed forms and then
  // syncs to disk. Errors are treated as an empty list of credentials.
  void SyncAllCredentials(password_manager::PasswordStoreInterface* store,
                          password_manager::LoginsResultOrError forms_or_error);

  // Syncs the credential store to disk.
  void SyncStore();

  // Add credentials from `forms`. Currently simply calls either the legacy or
  // refactored version of this function.
  void AddCredentials(MemoryCredentialStore* store,
                      std::vector<password_manager::PasswordForm> forms);

  // Add credentials from `forms`. This is the original legacy version.
  void AddCredentialsLegacy(MemoryCredentialStore* store,
                            std::vector<password_manager::PasswordForm> forms);

  // Add credentials from `forms`. This is the refactored version for better
  // performance.
  void AddCredentialsRefactored(
      MemoryCredentialStore* store,
      std::vector<password_manager::PasswordForm> forms);

  // Add credentials from passkeys.
  void AddCredentials(
      MemoryCredentialStore* store,
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys);

  // Removes credentials from `forms`.
  void RemoveCredentials(MemoryCredentialStore* store,
                         std::vector<password_manager::PasswordForm> forms);

  // Removes credentials from `passkeys`.
  void RemoveCredentials(
      MemoryCredentialStore* store,
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys);

  // Syncs account id for validation.
  void UpdateAccountId();

  // Syncs the current logged in user's email to the extension if they are
  // syncing passwords.
  void UpdateUserEmail();

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error) override;

  // Completion called after the affiliations are injected in the added forms.
  // If no affiliation matcher is available, it is called right away. Errors are
  // treated as an empty list of credentials.
  void OnInjectedAffiliationAfterLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error);

  // PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Observer for when `saving_passwords_enabled_` changes.
  void OnSavingPasswordsEnabledChanged();

  // For each of the 2 PasswordStoreInterfaces (profile and account), returns
  // the corresponding in-memory store used for password deduplication. See
  // comment in {profile,account}_credential_store_ declaration.
  MemoryCredentialStore* GetCredentialStore(
      password_manager::PasswordStoreInterface* store) const;

  // The pref service.
  const raw_ptr<PrefService> prefs_;

  // The interfaces for getting and manipulating a user's saved passwords.
  const scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;

  // Passkey store.
  raw_ptr<webauthn::PasskeyModel> passkey_model_;

  // Identity manager to observe.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Sync Service to observe.
  const raw_ptr<syncer::SyncService> sync_service_;

  // Helper which injects branding information from affiliation service.
  const std::unique_ptr<password_manager::AffiliatedMatchHelper>
      affiliated_helper_;

  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  const raw_ptr<FaviconLoader> favicon_loader_;

  // In-memory stores used to dedupe entries from `profile_password_store_` and
  // `account_password_store_` before persisting via `dual_credential_store_`.
  // TODO(crbug.com/40260886): This is super hacky. Refactor this class to use
  // SavedPasswordsPresenter, which deduplicates internally.
  MemoryCredentialStore* const profile_credential_store_ =
      [[MemoryCredentialStore alloc] init];
  MemoryCredentialStore* const account_credential_store_ =
      [[MemoryCredentialStore alloc] init];

  // The interface for saving and updating credentials. Stores deduplicated
  // results from `profile_password_store_` and `account_password_store_`.
  const id<MutableCredentialStore> dual_credential_store_;

  // The preference associated with
  // password_manager::prefs::kCredentialsEnableService.
  BooleanPrefMember saving_passwords_enabled_;

  // Weak pointer factory.
  base::WeakPtrFactory<CredentialProviderService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_H_

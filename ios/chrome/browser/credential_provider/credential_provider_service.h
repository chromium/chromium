// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service_observer.h"
#import "ios/chrome/browser/signin/authentication_service.h"

@protocol MutableCredentialStore;

namespace syncer {
class SyncService;
}

// A browser-context keyed service that is used to keep the Credential Provider
// Extension data up to date.
class CredentialProviderService
    : public KeyedService,
      public password_manager::PasswordStoreConsumer,
      public password_manager::PasswordStoreInterface::Observer,
      public signin::IdentityManager::Observer,
      public syncer::SyncServiceObserver {
 public:
  // Initializes the service.
  CredentialProviderService(
      scoped_refptr<password_manager::PasswordStore> password_store,
      AuthenticationService* authentication_service,
      id<MutableCredentialStore> credential_store,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);
  ~CredentialProviderService() override;

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  // Request all the credentials to sync them. Before adding the fresh ones,
  // the old ones are deleted.
  void RequestSyncAllCredentials();

  // Evaluates if a credential refresh is needed, and request all the
  // credentials to sync them if needed.
  void RequestSyncAllCredentialsIfNeeded();

  // Replaces all data with credentials created from the passed forms and then
  // syncs to disk.
  void SyncAllCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms);

  // Syncs the credential store to disk.
  void SyncStore(bool set_first_time_sync_flag);

  // Add credentials from |forms|.
  void AddCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms);

  // Removes credentials from |forms|.
  void RemoveCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms);

  // Syncs account_validation_id_.
  void UpdateAccountValidationId();

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // Completion called after the affiliations are injected in the added forms.
  // If no affiliation matcher is available, it is called right away.
  void OnInjectedAffiliationAfterLoginsChanged(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms);

  // syncer::SyncServiceObserver:
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The interface for getting the primary account identifier.
  AuthenticationService* authentication_service_ = nullptr;

  // Identity manager to observe.
  signin::IdentityManager* identity_manager_ = nullptr;

  // Sync Service to observe.
  syncer::SyncService* sync_service_ = nullptr;

  // The interface for saving and updating credentials.
  id<MutableCredentialStore> credential_store_ = nil;

  // The current validation ID or nil.
  NSString* account_validation_id_ = nil;

  // Weak pointer factory.
  base::WeakPtrFactory<CredentialProviderService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderService);
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "build/build_config.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/affiliation/affiliation_service.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_change.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/password_manager/core/browser/password_store_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#import "ios/chrome/browser/credential_provider/credential_provider_util.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreInterface;
using password_manager::AffiliationService;

// ASCredentialIdentityStoreError enum to report UMA metrics. Must be in sync
// with iOSCredentialIdentityStoreErrorForReporting in
// tools/metrics/histograms/enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CredentialIdentityStoreErrorForReporting {
  kUnknownError,
  kInternal,
  kDisabled,
  kBusy,
  kMaxValue = kBusy
};

// Converts a UIKit interface style to an interface style for reporting.
CredentialIdentityStoreErrorForReporting
ErrorForReportingForASCredentialIdentityStoreErrorCode(
    ASCredentialIdentityStoreErrorCode errorCode) {
  switch (errorCode) {
    case ASCredentialIdentityStoreErrorCodeInternalError:
      return CredentialIdentityStoreErrorForReporting::kInternal;
    case ASCredentialIdentityStoreErrorCodeStoreDisabled:
      return CredentialIdentityStoreErrorForReporting::kDisabled;
    case ASCredentialIdentityStoreErrorCodeStoreBusy:
      return CredentialIdentityStoreErrorForReporting::kBusy;
  }
  return CredentialIdentityStoreErrorForReporting::kUnknownError;
}

BOOL ShouldSyncAllCredentials() {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  DCHECK(user_defaults);
  return ![user_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
}

BOOL ShouldSyncASIdentityStore() {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  DCHECK(user_defaults);
  BOOL isIdentityStoreSynced = [user_defaults
      boolForKey:kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted];
  BOOL areCredentialsSynced = [user_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  return !isIdentityStoreSynced && areCredentialsSynced;
}

void SyncASIdentityStore(id<CredentialStore> credential_store) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
#if !defined(NDEBUG)
    dispatch_assert_queue_not(dispatch_get_main_queue());
#endif  // !defined(NDEBUG)
    if (state.enabled) {
      NSArray<id<Credential>>* credentials = credential_store.credentials;
      NSMutableArray<ASPasswordCredentialIdentity*>* storeIdentities =
          [NSMutableArray arrayWithCapacity:credentials.count];
      for (id<Credential> credential in credentials) {
        [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                       initWithCredential:credential]];
      }
      auto replaceCompletion = ^(BOOL success, NSError* error) {
        // Sometimes ASCredentialIdentityStore fails. Log this to measure the
        // impact of these failures and move on.
        if (!success) {
          ASCredentialIdentityStoreErrorCode code =
              static_cast<ASCredentialIdentityStoreErrorCode>(error.code);
          CredentialIdentityStoreErrorForReporting errorForReporting =
              ErrorForReportingForASCredentialIdentityStoreErrorCode(code);
          base::UmaHistogramEnumeration(
              "IOS.CredentialExtension.Service.Error."
              "ReplaceCredentialIdentitiesWithIdentities",
              errorForReporting);
        }
        NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
        NSString* key =
            kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;
        [user_defaults setBool:success forKey:key];
      };
      [ASCredentialIdentityStore.sharedStore
          replaceCredentialIdentitiesWithIdentities:storeIdentities
                                         completion:replaceCompletion];
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    PrefService* prefs,
    scoped_refptr<PasswordStoreInterface> password_store,
    AuthenticationService* authentication_service,
    id<MutableCredentialStore> credential_store,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    password_manager::AffiliationService* affiliation_service,
    FaviconLoader* favicon_loader)
    : password_store_(password_store),
      authentication_service_(authentication_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      affiliated_helper_(
          std::make_unique<AffiliatedMatchHelper>(affiliation_service)),
      favicon_loader_(favicon_loader),
      credential_store_(credential_store) {
  DCHECK(password_store_);
  password_store_->AddObserver(this);

  DCHECK(authentication_service_);
  UpdateAccountId();
  UpdateUserEmail();

  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  bool is_sync_active = false;
  if (sync_service_) {
    sync_service_->AddObserver(this);
    is_sync_active = sync_service_->IsSyncFeatureActive();
  }

  // If Sync is active, wait for the configuration to finish before syncing.
  // This will wait for affiliated_match_helper to be available.
  if (!is_sync_active) {
    RequestSyncAllCredentialsIfNeeded();
  }

  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, prefs,
      base::BindRepeating(
          &CredentialProviderService::OnSavingPasswordsEnabledChanged,
          base::Unretained(this)));

  // Make sure the initial value of the pref is stored.
  OnSavingPasswordsEnabledChanged();
}

CredentialProviderService::~CredentialProviderService() {}

void CredentialProviderService::Shutdown() {
  password_store_->RemoveObserver(this);
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void CredentialProviderService::RequestSyncAllCredentials() {
  UpdateAccountId();
  UpdateUserEmail();
  password_store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void CredentialProviderService::RequestSyncAllCredentialsIfNeeded() {
  if (ShouldSyncASIdentityStore()) {
    SyncASIdentityStore(credential_store_);
  }
  if (ShouldSyncAllCredentials()) {
    RequestSyncAllCredentials();
  }
}

void CredentialProviderService::SyncAllCredentials(
    absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                  password_manager::PasswordStoreBackendError> forms_or_error) {
  std::vector<std::unique_ptr<PasswordForm>> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));
  [credential_store_ removeAllCredentials];
  AddCredentials(std::move(forms));
  SyncStore(true);
}

void CredentialProviderService::SyncStore(bool set_first_time_sync_flag) {
  __weak id<CredentialStore> weak_credential_store = credential_store_;
  [credential_store_ saveDataWithCompletion:^(NSError* error) {
    if (error) {
      return;
    }
    if (set_first_time_sync_flag) {
      NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
      for (NSString* key in UnusedUserDefaultsCredentialProviderKeys()) {
        [user_defaults removeObjectForKey:key];
      }
      NSString* key = kUserDefaultsCredentialProviderFirstTimeSyncCompleted;
      [user_defaults setBool:YES forKey:key];
    }
    if (weak_credential_store) {
      SyncASIdentityStore(weak_credential_store);
    }
  }];
}

void CredentialProviderService::AddCredentials(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  // User is adding a password (not batch add from user login).
  const bool should_skip_max_verification = forms.size() == 1;
  const bool sync_enabled = sync_service_->IsSyncFeatureEnabled();

  for (const auto& form : forms) {
    NSString* favicon_key = GetFaviconFileKey(form->url);
    // Fetch the favicon and save it to the storage.
    FetchFaviconForURLToPath(favicon_loader_, form->url, favicon_key,
                             should_skip_max_verification, sync_enabled);

    ArchivableCredential* credential =
        [[ArchivableCredential alloc] initWithPasswordForm:*form
                                                   favicon:favicon_key
                                      validationIdentifier:account_id_];
    DCHECK(credential);
    [credential_store_ addCredential:credential];
  }
}

void CredentialProviderService::RemoveCredentials(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  for (const auto& form : forms) {
    NSString* recordID = RecordIdentifierForPasswordForm(*form);
    DCHECK(recordID);
    if ([credential_store_ credentialWithRecordIdentifier:recordID]) {
      [credential_store_ removeCredentialWithRecordIdentifier:recordID];
    }
  }
}

void CredentialProviderService::UpdateAccountId() {
  id<SystemIdentity> identity = authentication_service_->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
  if (authentication_service_->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin)) {
    account_id_ = identity.gaiaID;
  } else {
    account_id_ = nil;
  }
  [app_group::GetGroupUserDefaults()
      setObject:account_id_
         forKey:AppGroupUserDefaultsCredentialProviderUserID()];
}

void CredentialProviderService::UpdateUserEmail() {
  const bool sync_enabled = sync_service_->IsSyncFeatureEnabled();
  const bool passwords_sync_enabled =
      sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPasswords);

  NSString* user_email = nil;
  if (sync_enabled && passwords_sync_enabled) {
    id<SystemIdentity> identity = authentication_service_->GetPrimaryIdentity(
        signin::ConsentLevel::kSync);
    user_email = identity.userEmail;
  }

  [app_group::GetGroupUserDefaults()
      setObject:user_email
         forKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
}

void CredentialProviderService::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  auto callback = base::BindOnce(&CredentialProviderService::SyncAllCredentials,
                                 weak_ptr_factory_.GetWeakPtr());
  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      std::move(results), std::move(callback));
}

void CredentialProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // The service uses the account consented for Sync, only process
  // an update if the consent has changed.
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      RequestSyncAllCredentials();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void CredentialProviderService::OnLoginsChanged(
    password_manager::PasswordStoreInterface* /*store*/,
    const PasswordStoreChangeList& changes) {
  std::vector<std::unique_ptr<PasswordForm>> forms_to_add;
  std::vector<std::unique_ptr<PasswordForm>> forms_to_remove;
  for (const PasswordStoreChange& change : changes) {
    if (change.form().blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        forms_to_add.push_back(std::make_unique<PasswordForm>(change.form()));
        break;
      case PasswordStoreChange::UPDATE:
        forms_to_remove.push_back(
            std::make_unique<PasswordForm>(change.form()));
        forms_to_add.push_back(std::make_unique<PasswordForm>(change.form()));
        break;
      case PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(
            std::make_unique<PasswordForm>(change.form()));
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  RemoveCredentials(std::move(forms_to_remove));

  auto callback = base::BindOnce(
      &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
      weak_ptr_factory_.GetWeakPtr());

  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      std::move(forms_to_add), std::move(callback));
}

void CredentialProviderService::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
}

void CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged(
    absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                  password_manager::PasswordStoreBackendError> forms_or_error) {
  std::vector<std::unique_ptr<PasswordForm>> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));
  AddCredentials(std::move(forms));
  SyncStore(false);
}

void CredentialProviderService::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  RequestSyncAllCredentialsIfNeeded();
}

void CredentialProviderService::OnStateChanged(syncer::SyncService* sync) {
  // When the state changes, it's possible that password syncing has
  // started/stopped, so the user's email must be updated.
  UpdateUserEmail();
  RequestSyncAllCredentialsIfNeeded();
}

void CredentialProviderService::OnSavingPasswordsEnabledChanged() {
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passwords_enabled_.GetValue()]
         forKey:AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()];
}

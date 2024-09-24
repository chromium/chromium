// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_service.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "build/build_config.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_store/password_store_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_store.h"

namespace {

using affiliations::AffiliationService;
using password_manager::AffiliatedMatchHelper;
using password_manager::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreInterface;

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

void SyncASIdentityStore(id<CredentialStore> credential_store) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
#if !defined(NDEBUG)
    dispatch_assert_queue_not(dispatch_get_main_queue());
#endif  // !defined(NDEBUG)
    if (state.enabled) {
      NSArray<id<Credential>>* credentials = credential_store.credentials;
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
      };
      if (@available(iOS 17.0, *)) {
        NSMutableArray<id<ASCredentialIdentity>>* storeIdentities =
            [NSMutableArray arrayWithCapacity:credentials.count];
        for (id<Credential> credential in credentials) {
          if (credential.isPasskey) {
            [storeIdentities addObject:[[ASPasskeyCredentialIdentity alloc]
                                           cr_initWithCredential:credential]];
          } else {
            [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                           cr_initWithCredential:credential]];
          }
        }
        [ASCredentialIdentityStore.sharedStore
            replaceCredentialIdentityEntries:storeIdentities
                                  completion:replaceCompletion];
      } else {
        NSMutableArray<ASPasswordCredentialIdentity*>* storeIdentities =
            [NSMutableArray arrayWithCapacity:credentials.count];
        for (id<Credential> credential in credentials) {
          [storeIdentities addObject:[[ASPasswordCredentialIdentity alloc]
                                         cr_initWithCredential:credential]];
        }
        [ASCredentialIdentityStore.sharedStore
            replaceCredentialIdentitiesWithIdentities:storeIdentities
                                           completion:replaceCompletion];
      }
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

bool CanSendHistoryData(syncer::SyncService* sync_service) {
  // SESSIONS and HISTORY both contain history-like data, so it's sufficient if
  // either of them is being uploaded.
  return syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::SESSIONS) ==
             syncer::UploadState::ACTIVE ||
         syncer::GetUploadToGoogleState(sync_service,
                                        syncer::DataType::HISTORY) ==
             syncer::UploadState::ACTIVE;
}

void RecordNumberFaviconsFetched(size_t fetched_favicon_count) {
  base::UmaHistogramCounts10000("IOS.CredentialExtension.NumberFaviconsFetched",
                                fetched_favicon_count);
}

}  // namespace

CredentialProviderService::CredentialProviderService(
    PrefService* prefs,
    scoped_refptr<PasswordStoreInterface> profile_password_store,
    scoped_refptr<PasswordStoreInterface> account_password_store,
    webauthn::PasskeyModel* passkey_model,
    id<MutableCredentialStore> credential_store,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    affiliations::AffiliationService* affiliation_service,
    FaviconLoader* favicon_loader)
    : prefs_(prefs),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      passkey_model_(passkey_model),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      affiliated_helper_(
          std::make_unique<AffiliatedMatchHelper>(affiliation_service)),
      favicon_loader_(favicon_loader),
      dual_credential_store_(credential_store) {
  CHECK(profile_password_store_);
  CHECK(identity_manager_);
  CHECK(sync_service_);
  CHECK(favicon_loader_);
  CHECK(dual_credential_store_);

  profile_password_store_->AddObserver(this);
  if (account_password_store_) {
    account_password_store_->AddObserver(this);
  }
  if (passkey_model_) {
    passkey_model_->AddObserver(this);
  }

  UpdateAccountId();
  UpdateUserEmail();

  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);

  // This class should usually handle incremental PasswordStore updates in
  // OnLoginsChanged(), but there could be bugs. E.g. maybe an update is fired
  // before the observer is added. So re-write the data on startup as a
  // safeguard. Post a task for performance.
  // Note: in reality this re-write does the same IO work as saving a new
  // password. The implementations of MutableCredentialStore write *every*
  // password to disk, even in OnLoginsChanged().
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CredentialProviderService::RequestSyncAllCredentials,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(5));

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
  profile_password_store_->RemoveObserver(this);
  if (account_password_store_) {
    account_password_store_->RemoveObserver(this);
  }
  if (passkey_model_) {
    passkey_model_->RemoveObserver(this);
  }
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void CredentialProviderService::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  std::vector<PasswordForm> forms_to_add, forms_to_remove;
  for (const PasswordStoreChange& change : changes) {
    if (change.form().blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        forms_to_add.push_back(change.form());
        break;
      case PasswordStoreChange::UPDATE:
        // Only act on updates if they involve a password change. This is
        // because using a passwords triggers this code path, since it updates
        // the use count and use date. Ideally we shouldn't care about this, but
        // for now the whole password file is re-written on every change, which
        // is inefficient. Username changes are not considered updates, but
        // instead treated as a new credential (REMOVE then ADD).
        if (!IsCPEPerformanceImprovementsEnabled() ||
            change.password_changed()) {
          forms_to_remove.push_back(change.form());
          forms_to_add.push_back(change.form());
        }
        break;
      case PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(change.form());
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  if (IsCPEPerformanceImprovementsEnabled()) {
    if (!forms_to_remove.empty()) {
      RemoveCredentials(GetCredentialStore(store), std::move(forms_to_remove));

      // Need to commit the removal to disk if there will not be forms added
      // afterwards.
      if (forms_to_add.empty()) {
        SyncStore();
      }
    }

    if (!forms_to_add.empty()) {
      auto callback = base::BindOnce(
          &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));

      affiliated_helper_->InjectAffiliationAndBrandingInformation(
          std::move(forms_to_add), std::move(callback));
    }
  } else {
    RemoveCredentials(GetCredentialStore(store), std::move(forms_to_remove));

    auto callback = base::BindOnce(
        &CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));

    affiliated_helper_->InjectAffiliationAndBrandingInformation(
        std::move(forms_to_add), std::move(callback));
  }
}

void CredentialProviderService::RequestSyncAllCredentials() {
  profile_password_store_->GetAutofillableLogins(
      weak_ptr_factory_.GetWeakPtr());
  if (account_password_store_) {
    account_password_store_->GetAutofillableLogins(
        weak_ptr_factory_.GetWeakPtr());
  }
}

void CredentialProviderService::SyncAllCredentials(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError forms_or_error) {
  std::vector<PasswordForm> forms =
      password_manager::GetLoginsOrEmptyListOnFailure(
          std::move(forms_or_error));

  MemoryCredentialStore* memoryCredentialStore = GetCredentialStore(store);
  AddCredentials(memoryCredentialStore, std::move(forms));
  // We only sync passkeys into the account store.
  if (passkey_model_ && (store == account_password_store_)) {
    AddCredentials(memoryCredentialStore, passkey_model_->GetAllPasskeys());
  }
  SyncStore();
}

void CredentialProviderService::SyncStore() {
  base::UmaHistogramBoolean(kSyncStoreHistogramName, true);

  [dual_credential_store_ removeAllCredentials];
  for (id<Credential> credential in profile_credential_store_.credentials) {
    [dual_credential_store_ addCredential:credential];
  }
  for (id<Credential> credential in account_credential_store_.credentials) {
    [dual_credential_store_ addCredential:credential];
  }

  __weak id<CredentialStore> weak_credential_store = dual_credential_store_;
  [dual_credential_store_ saveDataWithCompletion:^(NSError* error) {
    if (error) {
      return;
    }
    if (weak_credential_store) {
      SyncASIdentityStore(weak_credential_store);
    }
  }];
}

void CredentialProviderService::AddCredentials(
    MemoryCredentialStore* store,
    std::vector<PasswordForm> forms) {
  if (IsCPEPerformanceImprovementsEnabled()) {
    AddCredentialsRefactored(store, forms);
  } else {
    AddCredentialsLegacy(store, forms);
  }
}

void CredentialProviderService::AddCredentialsLegacy(
    MemoryCredentialStore* store,
    std::vector<PasswordForm> forms) {
  // User is adding a password (not batch add from user login).
  const bool should_skip_max_verification = forms.size() == 1;
  const bool fallback_to_google_server_allowed =
      CanSendHistoryData(sync_service_);
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* gaia = base::SysUTF8ToNSString(account.gaia);

  int fetched_favicon_count = 0;

  for (const PasswordForm& form : forms) {
    NSString* favicon_key;
    // Only fetch favicon for valid URL. FaviconLoader::FaviconForPageUrl does
    // not take Android facet URI.
    if (form.url.is_valid()) {
      ++fetched_favicon_count;
      favicon_key = GetFaviconFileKey(form.url);

      // Fetch the favicon and save it to the storage.
      FetchFaviconForURLToPath(favicon_loader_, form.url, favicon_key,
                               should_skip_max_verification,
                               fallback_to_google_server_allowed);
    }

    // Only store password with valid Android facet URI or valid URL.
    if (affiliations::IsValidAndroidFacetURI(form.signon_realm) ||
        form.url.is_valid()) {
      ArchivableCredential* credential =
          [[ArchivableCredential alloc] initWithPasswordForm:form
                                                     favicon:favicon_key
                                                        gaia:gaia];
      DCHECK(credential);
      [store addCredential:credential];
    }
  }

  RecordNumberFaviconsFetched(fetched_favicon_count);
}

void CredentialProviderService::AddCredentialsRefactored(
    MemoryCredentialStore* store,
    std::vector<PasswordForm> forms) {
  // Dont' rate limit the favicon fetch when adding a single password.
  const bool should_skip_max_verification = forms.size() == 1;
  const bool fallback_to_google_server_allowed =
      CanSendHistoryData(sync_service_);
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* gaia = base::SysUTF8ToNSString(account.gaia);

  // Get the list of existing favicon files, along with their creation date.
  NSDictionary<NSString*, NSDate*>* favicon_dict =
      GetFaviconsListAndFreshness();
  int fetched_favicon_count = 0;

  for (const PasswordForm& form : forms) {
    NSString* favicon_key;
    if (form.url.is_valid()) {
      favicon_key = GetFaviconFileKey(form.url);

      if (ShouldFetchFavicon(favicon_key, favicon_dict)) {
        ++fetched_favicon_count;

        // Fetch the favicon and save it to the storage.
        FetchFaviconForURLToPath(favicon_loader_, form.url, favicon_key,
                                 should_skip_max_verification,
                                 fallback_to_google_server_allowed);
      }
    }

    // Only store password with valid Android facet URI or valid URL.
    if (affiliations::IsValidAndroidFacetURI(form.signon_realm) ||
        form.url.is_valid()) {
      ArchivableCredential* credential =
          [[ArchivableCredential alloc] initWithPasswordForm:form
                                                     favicon:favicon_key
                                                        gaia:gaia];
      DCHECK(credential);
      [store addCredential:credential];
    }
  }

  RecordNumberFaviconsFetched(fetched_favicon_count);
}

void CredentialProviderService::AddCredentials(
    MemoryCredentialStore* store,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
  // User is adding a passkey (not batch add from user login).
  const bool should_skip_max_verification = passkeys.size() == 1;
  const bool fallback_to_google_server = CanSendHistoryData(sync_service_);
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* gaia = base::SysUTF8ToNSString(account.gaia);

  for (const auto& passkey : passkeys) {
    // Only fetch favicon for valid URL.
    GURL url(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()}));
    if (url.is_valid()) {
      NSString* favicon_key = GetFaviconFileKey(url);

      // Fetch the favicon and save it to the storage.
      FetchFaviconForURLToPath(favicon_loader_, url, favicon_key,
                               should_skip_max_verification,
                               fallback_to_google_server);

      ArchivableCredential* credential =
          [[ArchivableCredential alloc] initWithFavicon:favicon_key
                                                   gaia:gaia
                                                passkey:passkey];
      DCHECK(credential);
      [store addCredential:credential];
    }
  }
}

void CredentialProviderService::RemoveCredentials(
    MemoryCredentialStore* store,
    std::vector<PasswordForm> forms) {
  for (const auto& form : forms) {
    NSString* recordID = RecordIdentifierForPasswordForm(form);
    DCHECK(recordID);
    [store removeCredentialWithRecordIdentifier:recordID];
  }
}

void CredentialProviderService::RemoveCredentials(
    MemoryCredentialStore* store,
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
  for (const auto& passkey : passkeys) {
    NSString* recordID = RecordIdentifierForPasskey(passkey);
    DCHECK(recordID);
    [store removeCredentialWithRecordIdentifier:recordID];
  }
}

void CredentialProviderService::UpdateAccountId() {
  CoreAccountInfo account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* account_id = nil;
  if (!account.IsEmpty() &&
      identity_manager_->FindExtendedAccountInfo(account).IsManaged()) {
    account_id = base::SysUTF8ToNSString(account.gaia);
  }
  [app_group::GetGroupUserDefaults()
      setObject:account_id
         forKey:AppGroupUserDefaultsCredentialProviderUserID()];
}

void CredentialProviderService::UpdateUserEmail() {
  std::optional accountForSaving =
      password_manager::sync_util::GetAccountForSaving(prefs_, sync_service_);
  [app_group::GetGroupUserDefaults()
      setObject:accountForSaving ? base::SysUTF8ToNSString(*accountForSaving)
                                 : nil
         forKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
}

void CredentialProviderService::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results) {
  auto callback =
      base::BindOnce(&CredentialProviderService::SyncAllCredentials,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(store));
  affiliated_helper_->InjectAffiliationAndBrandingInformation(
      password_manager::GetLoginsOrEmptyListOnFailure(std::move(results)),
      std::move(callback));
}

void CredentialProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      UpdateAccountId();
      UpdateUserEmail();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void CredentialProviderService::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
}

void CredentialProviderService::OnInjectedAffiliationAfterLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  AddCredentials(GetCredentialStore(store),
                 password_manager::GetLoginsOrEmptyListOnFailure(
                     std::move(results_or_error)));
  SyncStore();
}

void CredentialProviderService::OnStateChanged(syncer::SyncService* sync) {
  // When the state changes, it's possible that password syncing has
  // started/stopped, so the user's email must be updated.
  UpdateUserEmail();
}

// PasskeyModel::Observer:
void CredentialProviderService::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  // Passkeys get saved only into the account store.
  if (!account_password_store_) {
    return;
  }

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys_to_add;
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys_to_remove;
  for (const webauthn::PasskeyModelChange& change : changes) {
    const sync_pb::WebauthnCredentialSpecifics& passkey = change.passkey();
    switch (change.type()) {
      case webauthn::PasskeyModelChange::ChangeType::ADD:
        passkeys_to_add.push_back(passkey);
        break;
      case webauthn::PasskeyModelChange::ChangeType::REMOVE:
        passkeys_to_remove.push_back(passkey);
        break;
      case webauthn::PasskeyModelChange::ChangeType::UPDATE:
        // TODO(crbug.com/330355124): do something more optimal than this.
        passkeys_to_add.push_back(passkey);
        passkeys_to_remove.push_back(passkey);
        break;
      default:
        NOTREACHED();
    }
  }

  if (passkeys_to_add.empty() && passkeys_to_remove.empty()) {
    return;
  }

  if (!passkeys_to_remove.empty()) {
    RemoveCredentials(account_credential_store_, passkeys_to_remove);
  }

  if (!passkeys_to_add.empty()) {
    AddCredentials(account_credential_store_, passkeys_to_add);
  }

  SyncStore();
}

void CredentialProviderService::OnPasskeyModelShuttingDown() {
  if (passkey_model_) {
    passkey_model_->RemoveObserver(this);
  }
  passkey_model_ = nullptr;
}

void CredentialProviderService::OnPasskeyModelIsReady(bool is_ready) {}

void CredentialProviderService::OnSavingPasswordsEnabledChanged() {
  [app_group::GetGroupUserDefaults()
      setObject:[NSNumber numberWithBool:saving_passwords_enabled_.GetValue()]
         forKey:AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()];
}

MemoryCredentialStore* CredentialProviderService::GetCredentialStore(
    password_manager::PasswordStoreInterface* store) const {
  return store == profile_password_store_ ? profile_credential_store_
                                          : account_credential_store_;
}

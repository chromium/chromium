// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service.h"

#import "base/auto_reset.h"
#import "base/check_is_test.h"
#import "base/containers/to_vector.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#import "components/browser_sync/sync_to_signin_migration.h"
#import "components/policy/core/common/management/platform_management_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#import "components/signin/public/identity_manager/signin_constants.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/account_pref_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer.h"
#import "ios/chrome/browser/signin/model/refresh_access_token_error.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_util.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

using signin::constants::kNoHostedDomainFound;

namespace {

// Enum for Signin.IOSDeviceRestoreSignedInState histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSDeviceRestoreSignedinState : int {
  // Case when the user is not signed in before the device restore.
  kUserNotSignedInBeforeDeviceRestore = 0,
  // Case when the user is signed in before the device restore but not after.
  kUserSignedInBeforeDeviceRestoreAndSignedOutAfterDeviceRestore = 1,
  // Case when the user is signed in before and after the device restore.
  kUserSignedInBeforeAndAfterDeviceRestore = 2,
  kMaxValue = kUserSignedInBeforeAndAfterDeviceRestore,
};

// Returns the account id associated with `identity`.
CoreAccountId SystemIdentityToAccountID(
    signin::IdentityManager* identity_manager,
    id<SystemIdentity> identity) {
  std::string email = base::SysNSStringToUTF8([identity userEmail]);
  return identity_manager->PickAccountIdForAccount(identity.gaiaId, email);
}

}  // namespace

AuthenticationService::AuthenticationService(
    PrefService* pref_service,
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      account_manager_service_(account_manager_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      weak_pointer_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);
}

AuthenticationService::~AuthenticationService() {
  DCHECK(!delegate_);
}

// static
void AuthenticationService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSigninShouldPromptForSigninAgain,
                                false);
}

void AuthenticationService::Initialize(
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  CHECK(delegate);
  CHECK(!initialized());
  bool has_primary_account_before_initialize =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  int account_count_before_initialize =
      identity_manager_->GetAccountsWithRefreshTokens().size();
  delegate_ = std::move(delegate);
  signin::Tribool device_restore_session = IsFirstSessionAfterDeviceRestore();
  initialized_ = true;

  identity_manager_observation_.Observe(identity_manager_.get());
  HandleForgottenIdentity(nil,
                          device_restore_session == signin::Tribool::kTrue);

  // Clean up account-scoped settings, in case any accounts were removed from
  // the device while Chrome wasn't running.
  ClearAccountSettingsPrefsOfRemovedAccounts();

  crash_keys::SetCurrentlySignedIn(
      HasPrimaryIdentity(signin::ConsentLevel::kSignin));

  account_manager_service_observation_.Observe(account_manager_service_.get());

  // Synchronize local state and profile signin prefs. This is needed because
  // many low level services still rely on the profile pref.
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  const PrefService::Preference* signin_allowed_on_device =
      local_pref_service->FindPreference(prefs::kSigninAllowedOnDevice);
  CHECK(signin_allowed_on_device);
  const PrefService::Preference* signin_allowed_profile =
      pref_service_->FindPreference(prefs::kSigninAllowed);
  CHECK(signin_allowed_profile);
  // One time migration from the profile prefs to the local state if the local
  // state is still using the default value. Otherwise update the profile pref
  // to match the local state value which is the source of truth.
  if (signin_allowed_on_device->IsDefaultValue()) {
    local_pref_service->Set(prefs::kSigninAllowedOnDevice,
                            *signin_allowed_profile->GetValue());
  } else {
    pref_service_->Set(prefs::kSigninAllowed,
                       *signin_allowed_on_device->GetValue());
  }

  // Register for prefs::kBrowserSigninPolicy.
  local_pref_change_registrar_.Init(local_pref_service);
  PrefChangeRegistrar::NamedChangeCallback browser_signin_policy_callback =
      base::BindRepeating(&AuthenticationService::OnBrowserSigninPolicyChanged,
                          base::Unretained(this));
  local_pref_change_registrar_.Add(prefs::kBrowserSigninPolicy,
                                   browser_signin_policy_callback);

  // Register for prefs::kSigninAllowedOnDevice.
  PrefChangeRegistrar::NamedChangeCallback signin_allowed_on_device_callback =
      base::BindRepeating(
          &AuthenticationService::OnSigninAllowedOnDeviceChanged,
          base::Unretained(this));
  local_pref_change_registrar_.Add(prefs::kSigninAllowedOnDevice,
                                   signin_allowed_on_device_callback);

  // Register for prefs::kSigninAllowed.
  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback signin_allowed_callback =
      base::BindRepeating(&AuthenticationService::OnSigninAllowedChanged,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kSigninAllowed, signin_allowed_callback);

  // Migrate primary identity info to widgets if needed.
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSString* primary_account =
      [shared_defaults objectForKey:app_group::kPrimaryAccount];

  if (!primary_account || primary_account.length == 0) {
    id<SystemIdentity> identity =
        GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    if (!identity.gaiaId.empty()) {
      [shared_defaults setObject:identity.gaiaId.ToNSString()
                          forKey:app_group::kPrimaryAccount];
    }
  }

  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  ReloadCredentialsFromIdentities();

  OnApplicationWillEnterForeground();
  bool has_primary_account_after_initialize =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  DCHECK(!has_primary_account_after_initialize ||
         has_primary_account_before_initialize);
  if (device_restore_session == signin::Tribool::kTrue) {
    // Records device restore histograms.
    if (has_primary_account_before_initialize) {
      base::UmaHistogramCounts100("Signin.IOSDeviceRestoreIdentityCountBefore",
                                  account_count_before_initialize);
      int account_count_after_initialize =
          identity_manager_->GetAccountsWithRefreshTokens().size();
      base::UmaHistogramCounts100("Signin.IOSDeviceRestoreIdentityCountAfter",
                                  account_count_after_initialize);
    }
    IOSDeviceRestoreSignedinState signed_in_state =
        IOSDeviceRestoreSignedinState::kUserNotSignedInBeforeDeviceRestore;
    if (has_primary_account_before_initialize) {
      signed_in_state =
          has_primary_account_after_initialize
              ? IOSDeviceRestoreSignedinState::
                    kUserSignedInBeforeAndAfterDeviceRestore
              : IOSDeviceRestoreSignedinState::
                    kUserSignedInBeforeDeviceRestoreAndSignedOutAfterDeviceRestore;
    }
    base::UmaHistogramEnumeration("Signin.IOSDeviceRestoreSignedInState",
                                  signed_in_state);
  }

  ProfileInitializationOutcome outcome =
      PerformProfileInitializationIfNecessary();
  base::UmaHistogramEnumeration(
      "Signin.IOSAuthenticationServiceInitializationOutcome", outcome);
}

void AuthenticationService::Shutdown() {
  identity_manager_observation_.Reset();
  account_manager_service_observation_.Reset();
  delegate_.reset();
}

void AuthenticationService::AddObserver(
    AuthenticationServiceObserver* observer) {
  observer_list_.AddObserver(observer);
  // Handle messages for late observers.
  if (primary_account_was_restricted_) {
    observer->OnPrimaryAccountRestricted();
  }
}

void AuthenticationService::RemoveObserver(
    AuthenticationServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

AuthenticationService::ServiceStatus AuthenticationService::GetServiceStatus()
    const {
  if (!account_manager_service_->IsServiceSupported()) {
    return ServiceStatus::SigninDisabledByInternal;
  }
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      local_pref_service->GetInteger(prefs::kBrowserSigninPolicy));
  switch (policy_mode) {
    case BrowserSigninMode::kDisabled:
      return ServiceStatus::SigninDisabledByPolicy;
    case BrowserSigninMode::kForced:
      return ServiceStatus::SigninForcedByPolicy;
    case BrowserSigninMode::kEnabled:
      break;
  }
  if (!local_pref_service->GetBoolean(prefs::kSigninAllowedOnDevice)) {
    return ServiceStatus::SigninDisabledByUser;
  }
  return ServiceStatus::SigninAllowed;
}

void AuthenticationService::OnApplicationWillEnterForeground() {
  if (GetServiceStatus() !=
      AuthenticationService::ServiceStatus::SigninDisabledByInternal) {
    UMA_HISTOGRAM_COUNTS_100(
        "Signin.IOSNumberOfDeviceAccounts",
        [account_manager_service_->GetAllIdentities() count]);
  }

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  using std::swap;
  std::map<CoreAccountId, id<RefreshAccessTokenError>> cached_mdm_errors;
  swap(cached_mdm_errors_, cached_mdm_errors);

  if (!cached_mdm_errors.empty()) {
    signin::DeviceAccountsSynchronizer* device_accounts_synchronizer =
        identity_manager_->GetDeviceAccountsSynchronizer();
    for (const auto& pair : cached_mdm_errors) {
      const CoreAccountId& account_id = pair.first;
      // For some reasons, it is possible to have a MDM error for an unknown
      // identity. This MDM error can be ignored.
      // See crbug.com/1482236.
      if (identity_manager_->HasAccountWithRefreshToken(account_id)) {
        device_accounts_synchronizer->ReloadAccountFromSystem(account_id);
      }
    }
  }
}

bool AuthenticationService::SigninEnabled() const {
  switch (GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      return YES;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return NO;
  }
}

void AuthenticationService::SetReauthPromptForSignInAndSync() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, true);
}

void AuthenticationService::ResetReauthPromptForSignInAndSync() {
  pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, false);
}

bool AuthenticationService::ShouldReauthPromptForSignInAndSync() const {
  return pref_service_->GetBoolean(prefs::kSigninShouldPromptForSigninAgain);
}

bool AuthenticationService::HasPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  return GetPrimaryIdentity(consent_level) != nil;
}

bool AuthenticationService::HasPrimaryIdentityManaged(
    signin::ConsentLevel consent_level) const {
  return identity_manager_
             ->FindExtendedAccountInfo(
                 identity_manager_->GetPrimaryAccountInfo(consent_level))
             .IsManaged() == signin::Tribool::kTrue;
}

bool AuthenticationService::ShouldClearDataForSignedInPeriodOnSignOut() const {
  // Data on the device should be cleared on signout when all conditions are
  // met:
  // 1. The user is signed in with a managed account.
  // 2. The app management configuration key is present.
  // Note: data will be cleared from the time of sign-in in this case.
  return HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin) &&
         !policy::PlatformManagementService::GetInstance()->IsManaged();
}

id<SystemIdentity> AuthenticationService::GetPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  return GetPrimarySystemIdentity(consent_level, identity_manager_,
                                  account_manager_service_);
}

void AuthenticationService::SignIn(id<SystemIdentity> identity,
                                   signin_metrics::AccessPoint access_point) {
  CHECK(SigninEnabled()) << "Service status "
                         << static_cast<int>(GetServiceStatus());
  DCHECK(account_manager_service_->IsValidIdentity(identity));

  primary_account_was_restricted_ = false;

  ResetReauthPromptForSignInAndSync();

  // TODO(crbug.com/40266839): Move this reset to a place more consistent with
  // bookmarks.
  ResetLastUsedBookmarkFolder(pref_service_);

  // Load all credentials from SSO library. This must load the credentials
  // for the primary account too.
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(CoreAccountId());

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      identity.gaiaId, base::SysNSStringToUTF8(identity.userEmail));

  // Ensure that the account the user is trying to sign into has been loaded
  // from the SSO library.
  CHECK(identity_manager_->HasAccountWithRefreshToken(account_id));
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  CHECK(!account_info.IsEmpty());

  // `PrimaryAccountMutator::SetPrimaryAccount` simply ignores the call
  // if there is already a signed in user. Check that there is no signed in
  // account or that the new signed in account matches the old one to avoid a
  // mismatch between the old and the new authenticated accounts.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    DCHECK(identity_manager_->GetPrimaryAccountMutator());
    signin::PrimaryAccountMutator::PrimaryAccountError error =
        identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
            account_id, signin::ConsentLevel::kSignin, access_point);
    CHECK_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
             error);
  }

  // The primary account should now be set to the expected account_id.
  // If CHECK_EQ() fails, having the CHECK() before would help to understand if
  // the primary account is empty or different that `account_id`.
  // Related to crbug.com/1308448.
  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  CHECK(!primary_account.empty());
  CHECK_EQ(account_id, primary_account);
  pref_service_->SetTime(prefs::kLastSigninTimestamp, base::Time::Now());

  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  local_pref_service->SetTime(
      prefs::kIdentityConfirmationSnackbarLastPromptTime, base::Time::Now());
  local_pref_service->SetInteger(
      prefs::kIdentityConfirmationSnackbarDisplayCount, 0);
  crash_keys::SetCurrentlySignedIn(true);
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    ProceduralBlock completion) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (completion) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(completion));
    }
    return;
  }

  // TODO(crbug.com/40266839): Move this reset to a place more consistent with
  // bookmarks.
  ResetLastUsedBookmarkFolder(pref_service_);

  const bool is_managed =
      HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
  const bool is_migrated_from_syncing =
      browser_sync::WasPrimaryAccountMigratedFromSyncingToSignedIn(
          identity_manager_, pref_service_);
  const bool should_clear_data_for_signed_in_period =
      ShouldClearDataForSignedInPeriodOnSignOut();

  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();
  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(signout_source);
  crash_keys::SetCurrentlySignedIn(false);
  cached_mdm_errors_.clear();

  // ClearPrimaryAccount() removed all the accounts from IdentityManager.
  // Populate them again.
  ReloadCredentialsFromIdentities();

  base::OnceClosure callback_closure =
      completion ? base::BindOnce(completion) : base::DoNothing();

  // Note: Once `kSeparateProfilesForManagedAccounts` is launched, the "clear
  // browsing data" cases are only reachable for managed accounts that were
  // already signed in before that feature was enabled. Once those users have
  // been migrated, this code can be cleaned up.
  if (is_managed && is_migrated_from_syncing) {
    // If `is_clear_data_feature_for_managed_users_enabled` is false, browsing
    // data for managed account needs to be cleared only if sync has started at
    // least once. This also includes the case where a previously-syncing user
    // was migrated to signed-in.
    delegate_->ClearBrowsingData(std::move(callback_closure));
  } else if (should_clear_data_for_signed_in_period) {
    delegate_->ClearBrowsingDataForSignedinPeriod(std::move(callback_closure));
  } else if (completion) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_closure));
  }
}

AuthenticationService::ProfileInitializationOutcome
AuthenticationService::PerformProfileInitializationIfNecessary() {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  if (!profile_manager) {
    // Skip if there is no profile manager, but this is possible only for test.
    CHECK_IS_TEST();
    return ProfileInitializationOutcome::kNoneForTesting;
  }
  ProfileAttributesStorageIOS* attributes_storage =
      profile_manager->GetProfileAttributesStorage();

  const std::string profile_name = account_manager_service_->GetProfileName();

  // Once this method returns, the profile is considered fully initialized. (If
  // the profile was already initialized, this is a no-op.)
  base::ScopedClosureRunner mark_profile_initialized(base::BindOnce(
      [](ProfileAttributesStorageIOS* attributes_storage,
         std::string_view profile_name) {
        attributes_storage->UpdateAttributesForProfileWithName(
            profile_name, base::BindOnce([](ProfileAttributesIOS& attrs) {
              attrs.SetFullyInitialized();
            }));
      },
      attributes_storage, profile_name));

  const bool was_already_initialized =
      attributes_storage->GetAttributesForProfileWithName(profile_name)
          .IsFullyInitialized();

  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    return was_already_initialized
               ? ProfileInitializationOutcome::
                     kFeatureDisabledAlreadyInitialized
               : ProfileInitializationOutcome::kFeatureDisabledNewlyInitialized;
  }

  // When opening a managed profile for the first time, the user needs to be
  // signed in automatically.

  const bool is_personal_profile =
      profile_name == attributes_storage->GetPersonalProfileName();
  if (is_personal_profile) {
    // Nothing to do if the current profile is the personal profile.
    return was_already_initialized
               ? ProfileInitializationOutcome::
                     kPersonalProfileAlreadyInitialized
               : ProfileInitializationOutcome::kPersonalProfileNewlyInitialized;
  }

  const bool is_signed_in = HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (is_signed_in) {
    // Nothing to do if the managed profile is already signed in.
    return was_already_initialized
               ? ProfileInitializationOutcome::kManagedProfileAlreadyInitialized
               : ProfileInitializationOutcome::
                     kManagedProfileNewlyInitializedButAlreadySignedIn;
  }

  NSArray<id<SystemIdentity>>* identities_for_profile =
      account_manager_service_->GetAllIdentities();
  if (identities_for_profile.count == 0) {
    return was_already_initialized
               ? ProfileInitializationOutcome::
                     kManagedProfileAlreadyInitializedNoAccounts
               : ProfileInitializationOutcome::
                     kManagedProfileNewlyInitializedNoAccounts;
  }

  SignIn(identities_for_profile[0],
         signin_metrics::AccessPoint::kManagedProfileAutoSigninIos);
  if (identities_for_profile.count > 1) {
    return was_already_initialized
               ? ProfileInitializationOutcome::
                     kManagedProfileAlreadyInitializedMultipleAccountsAndNewlySignedIn
               : ProfileInitializationOutcome::
                     kManagedProfileNewlyInitializedMultipleAccounts;
  }
  return was_already_initialized
             ? ProfileInitializationOutcome::
                   kManagedProfileAlreadyInitializedButNewlySignedIn
             : ProfileInitializationOutcome::kManagedProfileNewlyInitialized;
}

id<RefreshAccessTokenError> AuthenticationService::GetCachedMDMError(
    id<SystemIdentity> identity) {
  CoreAccountId account_id =
      SystemIdentityToAccountID(identity_manager_, identity);
  auto it = cached_mdm_errors_.find(account_id);
  if (it == cached_mdm_errors_.end()) {
    return nil;
  }

  if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id)) {
    // Account has no error, invalidate the cache.
    cached_mdm_errors_.erase(it);
    return nil;
  }

  return it->second;
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    id<SystemIdentity> identity) {
  return GetCachedMDMError(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    id<SystemIdentity> identity) {
  id<RefreshAccessTokenError> cached_error = GetCachedMDMError(identity);
  if (!cached_error) {
    return false;
  }

  GetApplicationContext()->GetSystemIdentityManager()->HandleMDMNotification(
      identity, ActiveIdentities(), cached_error, base::DoNothing());

  return true;
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  for (auto& observer : observer_list_) {
    observer.OnPrimaryIdentityChanged();
  }
}

void AuthenticationService::OnIdentitiesInProfileChanged() {
  ClearAccountSettingsPrefsOfRemovedAccounts();
  ReloadCredentialsFromIdentities();
}

bool AuthenticationService::HandleMDMError(id<SystemIdentity> identity,
                                           id<RefreshAccessTokenError> error) {
  id<RefreshAccessTokenError> cached_error = GetCachedMDMError(identity);
  if (cached_error && [cached_error isEqualToError:error]) {
    // Same status as the last error, ignore it to avoid spamming users.
    return false;
  }

  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (base::FeatureList::IsEnabled(switches::kAllowlistScopesForMdmErrors)) {
    bool scope_limited_error_suppressed =
        system_identity_manager->IsScopeLimitedError(error);
    base::UmaHistogramBoolean("Signin.ScopeLimitedErrorSuppressed",
                              scope_limited_error_suppressed);
    if (scope_limited_error_suppressed) {
      return false;
    }
  }

  // Stop displaying the MDM error dialog on the NTP when
  // kHandleMdmErrorsForDasherAccounts is enabled.
  if (!base::FeatureList::IsEnabled(
          switches::kHandleMdmErrorsForDasherAccounts) &&
      system_identity_manager->HandleMDMNotification(
          identity, ActiveIdentities(), error,
          base::BindOnce(&AuthenticationService::MDMErrorHandled,
                         weak_pointer_factory_.GetWeakPtr(), identity))) {
    CoreAccountId account_id =
        SystemIdentityToAccountID(identity_manager_, identity);
    DUMP_WILL_BE_CHECK(!account_id.empty())
        << "Unexpected identity with empty account id: [gaiaID = "
        << identity.gaiaId.ToNSString()
        << "; userEmail = " << identity.userEmail << "]";
    cached_mdm_errors_[account_id] = error;
    return true;
  }

  return false;
}

void AuthenticationService::MDMErrorHandled(id<SystemIdentity> identity,
                                            bool is_blocked) {
  // If the identity is blocked, sign-out of the account. As only managed
  // account can be blocked, this will clear the associated browsing data.
  if (!is_blocked) {
    return;
  }

  if (![identity isEqual:GetPrimaryIdentity(signin::ConsentLevel::kSignin)]) {
    return;
  }

  SignOut(signin_metrics::ProfileSignout::kAbortSignin, nil);
}

void AuthenticationService::OnRefreshTokenUpdated(id<SystemIdentity> identity) {
  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      identity.gaiaId, base::SysNSStringToUTF8(identity.userEmail));
  if (!identity_manager_->HasAccountWithRefreshToken(account_id)) {
    return;
  }
  identity_manager_->GetDeviceAccountsSynchronizer()->ReloadAccountFromSystem(
      account_id);
}

void AuthenticationService::OnAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    const std::set<std::string>& scopes) {
  if (!identity) {
    DLOG(ERROR)
        << "Unexpected call of OnAccessTokenRefreshFailed with null identity";
    return;
  }

  if (HandleMDMError(identity, error)) {
    return;
  }

  if (!error.isInvalidGrantError) {
    // If the failure is not due to an invalid grant, the identity is not
    // invalid and there is nothing to do.
    return;
  }

  // Handle the failure of access token refresh on the next message loop cycle.
  // `identity` is now invalid and the authentication service might need to
  // react to this loss of identity.
  // Note that no reload of the credentials is necessary here, as `identity`
  // might still be accessible in SSO, and `OnIdentityListChanged` will handle
  // this when `identity` will actually disappear from SSO.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AuthenticationService::HandleForgottenIdentity,
                                GetWeakPtr(), identity,
                                /*device_restore=*/false));
}

void AuthenticationService::HandleForgottenIdentity(
    id<SystemIdentity> invalid_identity,
    bool device_restore) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // User is not signed in. Nothing to do here.
    return;
  }

  // Tests if the primary identity still exists.
  id<SystemIdentity> authenticated_identity =
      GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (authenticated_identity &&
      ![authenticated_identity isEqual:invalid_identity]) {
    // `authenticated_identity` exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  const CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const bool account_filtered_out =
      account_manager_service_->IsEmailRestricted(account_info.email);

  // Metrics.
  signin_metrics::ProfileSignout signout_source;
  if (account_filtered_out) {
    // Account filtered out by enterprise policy.
    signout_source = signin_metrics::ProfileSignout::kPrefChanged;
  } else if (device_restore) {
    // Account removed from the device after a device restore.
    signout_source = signin_metrics::ProfileSignout::
        kIosAccountRemovedFromDeviceAfterRestore;
  } else {
    // Account removed from the device by another app or the token being
    // invalid.
    signout_source = signin_metrics::ProfileSignout::kAccountRemovedFromDevice;
  }

  // Store the pre-device-restore identity in-memory in order to prompt user
  // to sign-in again later.
  if (device_restore) {
    AccountInfo extended_account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(
            account_info.account_id);
    syncer::SyncUserSettings* user_settings = sync_service_->GetUserSettings();
    bool history_sync_enabled = user_settings->GetSelectedTypes().HasAll(
        {syncer::UserSelectableType::kHistory,
         syncer::UserSelectableType::kTabs});
    StorePreRestoreIdentity(pref_service_, extended_account_info,
                            history_sync_enabled);
  }

  // Sign the user out.
  SignOut(signout_source, nil);

  // Should prompt the user if the identity was not removed by the user.
  bool should_prompt = !GetApplicationContext()
                            ->GetSystemIdentityManager()
                            ->IdentityRemovedByUser(account_info.gaia);
  if (should_prompt && account_filtered_out) {
    FirePrimaryAccountRestricted();
  } else if (should_prompt &&
             IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    // If the device is restored, the restore shorty UI will be shown.
    // Therefore, the reauth UI should be skipped.
    SetReauthPromptForSignInAndSync();
  }
}

void AuthenticationService::ReloadCredentialsFromIdentities() {
  if (is_reloading_credentials_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&is_reloading_credentials_, true);

  HandleForgottenIdentity(nil, /*device_restore=*/false);

  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
}

void AuthenticationService::FirePrimaryAccountRestricted() {
  primary_account_was_restricted_ = true;
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountRestricted();
  }
}

void AuthenticationService::OnSigninAllowedChanged(const std::string& name) {
  DCHECK_EQ(prefs::kSigninAllowed, name);
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kSigninAllowedOnDevice,
      pref_service_->GetBoolean(prefs::kSigninAllowed));
}

void AuthenticationService::OnSigninAllowedOnDeviceChanged(
    const std::string& name) {
  DCHECK_EQ(prefs::kSigninAllowedOnDevice, name);
  pref_service_->SetBoolean(
      prefs::kSigninAllowed,
      GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kSigninAllowedOnDevice));
  FireServiceStatusNotification();
}

void AuthenticationService::OnBrowserSigninPolicyChanged(
    const std::string& name) {
  DCHECK_EQ(prefs::kBrowserSigninPolicy, name);
  FireServiceStatusNotification();
}

void AuthenticationService::FireServiceStatusNotification() {
  for (auto& observer : observer_list_) {
    observer.OnServiceStatusChanged();
  }
}

void AuthenticationService::ClearAccountSettingsPrefsOfRemovedAccounts() {
  std::vector<GaiaId> available_gaia_ids;
  for (id<SystemIdentity> identity in account_manager_service_
           ->GetAllIdentities()) {
    available_gaia_ids.emplace_back(identity.gaiaId);
  }
  sync_service_->GetUserSettings()->KeepAccountSettingsPrefsOnlyForUsers(
      available_gaia_ids);
  syncer::KeepAccountKeyedPrefValuesOnlyForUsers(
      pref_service_, prefs::kSigninHasAcceptedManagementDialog,
      base::ToVector(available_gaia_ids, &signin::GaiaIdHash::FromGaiaId));
}

NSArray<id<SystemIdentity>>* AuthenticationService::ActiveIdentities() {
  return GetPrimaryIdentity(signin::ConsentLevel::kSignin)
             ? account_manager_service_->GetAllIdentities()
             : @[];
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum describing the different sync states per login methods.
enum LoginMethodAndSyncState {
  // Legacy values retained to keep definitions in histograms.xml in sync.
  CLIENT_LOGIN_SYNC_OFF,
  CLIENT_LOGIN_SYNC_ON,
  SHARED_AUTHENTICATION_SYNC_OFF,
  SHARED_AUTHENTICATION_SYNC_ON,
  // NOTE: Add new login methods and sync states only immediately above this
  // line. Also, make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  LOGIN_METHOD_AND_SYNC_STATE_COUNT
};

// A fake account id used in the list of last signed in accounts when migrating
// an email for which the corresponding account was removed.
constexpr char kFakeAccountIdForRemovedAccount[] = "0000000000000";

// Returns the account id associated with |identity|.
CoreAccountId ChromeIdentityToAccountID(
    signin::IdentityManager* identity_manager,
    ChromeIdentity* identity) {
  std::string gaia_id = base::SysNSStringToUTF8([identity gaiaID]);
  return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id).account_id;
}

}  // namespace

AuthenticationService::AuthenticationService(
    PrefService* pref_service,
    SyncSetupService* sync_setup_service,
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_setup_service_(sync_setup_service),
      account_manager_service_(account_manager_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      user_approved_account_list_manager_(pref_service),
      weak_pointer_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(sync_setup_service_);
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
  registry->RegisterListPref(prefs::kSigninLastAccounts);
  registry->RegisterBooleanPref(prefs::kSigninLastAccountsMigrated, false);
}

void AuthenticationService::Initialize(
    std::unique_ptr<AuthenticationServiceDelegate> delegate) {
  CHECK(delegate);
  CHECK(!initialized());
  delegate_ = std::move(delegate);
  initialized_ = true;

  // The preference |kSigninAllowed| is not available for pre-MICE users. Except
  // for policy exceptions pre-MICE users cannot choose to be in a permanently
  // not signed-in state (ie. kSigninAllowed = false).
  // If a user sets the preference in MICE and then is rolled back to a pre-MICE
  // state, they will be returned to the default state (ie. kSigninAllowed =
  // true).
  if (!signin::IsMobileIdentityConsistencyEnabled()) {
    pref_service_->ClearPref(prefs::kSigninAllowed);
  }

  MigrateAccountsStoredInPrefsIfNeeded();

  HandleForgottenIdentity(nil, true /* should_prompt */);

  crash_keys::SetCurrentlySignedIn(
      HasPrimaryIdentity(signin::ConsentLevel::kSignin));

  identity_service_observation_.Observe(
      ios::GetChromeBrowserProvider().GetChromeIdentityService());

  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  // As UpdateHaveAccountsChangedAtColdStart is only called while the
  // application is cold starting, |keychain_reload| must be set to true.
  ReloadCredentialsFromIdentities(/*keychain_reload=*/true);

  identity_manager_observation_.Observe(identity_manager_);
  OnApplicationWillEnterForeground();
}

void AuthenticationService::Shutdown() {
  user_approved_account_list_manager_.Shutdown();
  identity_manager_observation_.Reset();
  delegate_.reset();
}

void AuthenticationService::OnApplicationWillEnterForeground() {
  if (HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    bool can_sync_start = sync_setup_service_->CanSyncFeatureStart();
    LoginMethodAndSyncState loginMethodAndSyncState =
        can_sync_start ? SHARED_AUTHENTICATION_SYNC_ON
                       : SHARED_AUTHENTICATION_SYNC_OFF;
    UMA_HISTOGRAM_ENUMERATION("Signin.IOSLoginMethodAndSyncState",
                              loginMethodAndSyncState,
                              LOGIN_METHOD_AND_SYNC_STATE_COUNT);
  }
  UMA_HISTOGRAM_COUNTS_100(
      "Signin.IOSNumberOfDeviceAccounts",
      [account_manager_service_->GetAllIdentities() count]);

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  using std::swap;
  std::map<CoreAccountId, NSDictionary*> cached_mdm_infos;
  swap(cached_mdm_infos_, cached_mdm_infos);

  if (!cached_mdm_infos.empty()) {
    signin::DeviceAccountsSynchronizer* device_accounts_synchronizer =
        identity_manager_->GetDeviceAccountsSynchronizer();
    for (const auto& cached_mdm_info : cached_mdm_infos) {
      device_accounts_synchronizer->ReloadAccountFromSystem(
          cached_mdm_info.first);
    }
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

bool AuthenticationService::IsAccountListApprovedByUser() const {
  DCHECK(HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  std::vector<CoreAccountInfo> accounts_info =
      identity_manager_->GetAccountsWithRefreshTokens();
  return user_approved_account_list_manager_.IsAccountListApprouvedByUser(
      accounts_info);
}

void AuthenticationService::ApproveAccountList() {
  DCHECK(HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  if (IsAccountListApprovedByUser())
    return;
  std::vector<CoreAccountInfo> current_accounts_info =
      identity_manager_->GetAccountsWithRefreshTokens();
  user_approved_account_list_manager_.SetApprovedAccountList(
      current_accounts_info);
}

void AuthenticationService::MigrateAccountsStoredInPrefsIfNeeded() {
  if (identity_manager_->GetAccountIdMigrationState() ==
      signin::IdentityManager::AccountIdMigrationState::MIGRATION_NOT_STARTED) {
    return;
  }
  DCHECK_EQ(signin::IdentityManager::AccountIdMigrationState::MIGRATION_DONE,
            identity_manager_->GetAccountIdMigrationState());
  if (pref_service_->GetBoolean(prefs::kSigninLastAccountsMigrated)) {
    // Already migrated.
    return;
  }

  std::vector<CoreAccountId> account_ids =
      user_approved_account_list_manager_.GetApprovedAccountIDList();
  std::vector<base::Value> accounts_pref_value;
  for (const auto& account_id : account_ids) {
    if (identity_manager_->HasAccountWithRefreshToken(account_id)) {
      accounts_pref_value.emplace_back(account_id.ToString());
    } else {
      // The account for |email| was removed since the last application cold
      // start. Insert |kFakeAccountIdForRemovedAccount| to ensure the user
      // account list has to be approved by the user and the removal won't be
      // silently ignored.
      accounts_pref_value.emplace_back(kFakeAccountIdForRemovedAccount);
    }
  }
  pref_service_->Set(prefs::kSigninLastAccounts,
                     base::Value(std::move(accounts_pref_value)));
  pref_service_->SetBoolean(prefs::kSigninLastAccountsMigrated, true);
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
      .IsManaged();
}

ChromeIdentity* AuthenticationService::GetPrimaryIdentity(
    signin::ConsentLevel consent_level) const {
  // There is no authenticated identity if there is no signed in user or if the
  // user signed in via the client login flow.
  if (!identity_manager_->HasPrimaryAccount(consent_level)) {
    return nil;
  }

  std::string authenticated_gaia_id =
      identity_manager_->GetPrimaryAccountInfo(consent_level).gaia;
  if (authenticated_gaia_id.empty())
    return nil;

  return account_manager_service_->GetIdentityWithGaiaID(authenticated_gaia_id);
}

ChromeIdentity* AuthenticationService::GetAuthenticatedIdentity() const {
  return GetPrimaryIdentity(signin::ConsentLevel::kSignin);
}

void AuthenticationService::SignIn(ChromeIdentity* identity) {
  CHECK(signin::IsSigninAllowed(pref_service_));
  DCHECK(account_manager_service_->IsValidIdentity(identity));

  ResetReauthPromptForSignInAndSync();

  // Load all credentials from SSO library. This must load the credentials
  // for the primary account too.
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(CoreAccountId());

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.userEmail));

  // Ensure that the account the user is trying to sign into has been loaded
  // from the SSO library and that hosted_domain is set (should be the proper
  // hosted domain or kNoHostedDomainFound that are both non-empty strings).
  CHECK(identity_manager_->HasAccountWithRefreshToken(account_id));
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  CHECK(!account_info.IsEmpty());
  CHECK(!account_info.hosted_domain.empty());

  // |PrimaryAccountManager::SetAuthenticatedAccountId| simply ignores the call
  // if there is already a signed in user. Check that there is no signed in
  // account or that the new signed in account matches the old one to avoid a
  // mismatch between the old and the new authenticated accounts.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    DCHECK(identity_manager_->GetPrimaryAccountMutator());
    // Initial sign-in to Chrome does not automatically turn on Sync features.
    // The Sync service will be enabled in a separate request to
    // |GrantSyncConsent|.
    identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
        account_id, signin::ConsentLevel::kSignin);
  }

  // The primary account should now be set to the expected account_id.
  CHECK_EQ(account_id, identity_manager_->GetPrimaryAccountId(
                           signin::ConsentLevel::kSignin));
  crash_keys::SetCurrentlySignedIn(true);
}

void AuthenticationService::GrantSyncConsent(ChromeIdentity* identity) {
  DCHECK(account_manager_service_->IsValidIdentity(identity));
  DCHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  const CoreAccountId account_id = identity_manager_->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.userEmail));
  const bool success =
      identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account_id, signin::ConsentLevel::kSync);

  CHECK(success);
  CHECK_EQ(account_id,
           identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync));

  // Sets the Sync setup handle to prepare for configuring the Sync data types
  // before Sync-the-feature actually starts.
  // TODO(crbug.com/1206680): Add EarlGrey tests to ensure that the Sync feature
  // only starts after GrantSyncConsent is called.
  sync_setup_service_->PrepareForFirstSyncSetup();

  // Kick-off sync: The authentication error UI (sign in infobar and warning
  // badge in settings screen) check the sync auth error state. Sync
  // needs to be kicked off so that it resets the auth error quickly once
  // |identity| is reauthenticated.
  sync_service_->GetUserSettings()->SetSyncRequested(true);
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    bool force_clear_browsing_data,
    ProceduralBlock completion) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (completion)
      completion();
    return;
  }

  const bool is_managed =
      HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
  // Get first setup complete value before to stop the sync service.
  const bool is_first_setup_complete =
      sync_setup_service_->IsFirstSetupComplete();

  sync_service_->StopAndClear();

  auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);
  account_mutator->ClearPrimaryAccount(
      signout_source, signin_metrics::SignoutDelete::kIgnoreMetric);
  crash_keys::SetCurrentlySignedIn(false);
  cached_mdm_infos_.clear();
  bool clear_browsing_data;
  if (base::FeatureList::IsEnabled(signin::kSimplifySignOutIOS)) {
    // With kSimplifySignOutIOS feature, browsing data for managed account needs
    // to be cleared only if sync has started at least once.
    clear_browsing_data =
        force_clear_browsing_data || (is_managed && is_first_setup_complete);
  } else {
    clear_browsing_data = force_clear_browsing_data || is_managed;
  }
  if (clear_browsing_data) {
    delegate_->ClearBrowsingData(completion);
  } else if (completion) {
    completion();
  }
}

NSDictionary* AuthenticationService::GetCachedMDMInfo(
    ChromeIdentity* identity) const {
  auto it = cached_mdm_infos_.find(
      ChromeIdentityToAccountID(identity_manager_, identity));

  if (it == cached_mdm_infos_.end()) {
    return nil;
  }

  if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          it->first)) {
    // Account has no error, invalidate the cache.
    cached_mdm_infos_.erase(it);
    return nil;
  }

  return it->second;
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    ChromeIdentity* identity) const {
  return GetCachedMDMInfo(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    ChromeIdentity* identity) {
  NSDictionary* cached_info = GetCachedMDMInfo(identity);
  if (!cached_info) {
    return false;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  identity_service->HandleMDMNotification(identity, cached_info, ^(bool){
                                                    });
  return true;
}

void AuthenticationService::ResetChromeIdentityServiceObserverForTesting() {
  DCHECK(!identity_service_observation_.IsObserving());
  identity_service_observation_.Observe(
      ios::GetChromeBrowserProvider().GetChromeIdentityService());
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      // TODO(crbug.com/1217673): Re-add DCHECK if approved account list
      // must be empty prior to a set call.
      ApproveAccountList();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      user_approved_account_list_manager_.ClearApprovedAccountList();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void AuthenticationService::OnIdentityListChanged(bool keychain_reload) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // IdentityManager::HasPrimaryAccount() needs to be called instead of
    // AuthenticationService::HasPrimaryIdentity() or
    // AuthenticationService::GetPrimaryIdentity().
    // If the primary identity has just been removed, GetPrimaryIdentity()
    // would return NO (since this method tests if the primary identity exists
    // in ChromeIdentityService).
    // In this case, we do need to call ReloadCredentialsFromIdentities().
    return;
  }
  // The list of identities may change while in an authorized call. Signing out
  // the authenticated user at this time may lead to crashes (e.g.
  // http://crbug.com/398431 ).
  // Handle the change of the identity list on the next message loop cycle.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AuthenticationService::ReloadCredentialsFromIdentities,
                     GetWeakPtr(), keychain_reload));
}

bool AuthenticationService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info) {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  ios::MDMDeviceStatus status = identity_service->GetMDMDeviceStatus(user_info);
  NSDictionary* cached_info = GetCachedMDMInfo(identity);

  if (cached_info &&
      identity_service->GetMDMDeviceStatus(cached_info) == status) {
    // Same status as the last error, ignore it to avoid spamming users.
    return false;
  }

  base::WeakPtr<AuthenticationService> weak_ptr = GetWeakPtr();
  ios::MDMStatusCallback callback = ^(bool is_blocked) {
    if (is_blocked && weak_ptr.get()) {
      // If the identity is blocked, sign out of the account. As only managed
      // account can be blocked, this will clear the associated browsing data.
      if (identity ==
          weak_ptr->GetPrimaryIdentity(signin::ConsentLevel::kSignin)) {
        weak_ptr->SignOut(signin_metrics::ABORT_SIGNIN,
                          /*force_clear_browsing_data=*/false, nil);
      }
    }
  };
  if (identity_service->HandleMDMNotification(identity, user_info, callback)) {
    cached_mdm_infos_[ChromeIdentityToAccountID(identity_manager_, identity)] =
        user_info;
    return true;
  }
  return false;
}

void AuthenticationService::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  if (HandleMDMNotification(identity, user_info)) {
    return;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  if (!identity_service->IsInvalidGrantError(user_info)) {
    // If the failure is not due to an invalid grant, the identity is not
    // invalid and there is nothing to do.
    return;
  }

  // Handle the failure of access token refresh on the next message loop cycle.
  // |identity| is now invalid and the authentication service might need to
  // react to this loss of identity.
  // Note that no reload of the credentials is necessary here, as |identity|
  // might still be accessible in SSO, and |OnIdentityListChanged| will handle
  // this when |identity| will actually disappear from SSO.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AuthenticationService::HandleForgottenIdentity,
                                base::Unretained(this), identity, true));
}

void AuthenticationService::OnChromeIdentityServiceWillBeDestroyed() {
  identity_service_observation_.Reset();
}

void AuthenticationService::HandleForgottenIdentity(
    ChromeIdentity* invalid_identity,
    bool should_prompt) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // User is not signed in. Nothing to do here.
    return;
  }

  ChromeIdentity* authenticated_identity =
      GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (authenticated_identity && authenticated_identity != invalid_identity) {
    // |authenticated_identity| exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  // Reauth prompt should only be set when the user is syncing, since reauth
  // turns on sync by default.
  should_prompt = should_prompt && identity_manager_->HasPrimaryAccount(
                                       signin::ConsentLevel::kSync);
  // Sign the user out.
  SignOut(signin_metrics::ACCOUNT_REMOVED_FROM_DEVICE,
          /*force_clear_browsing_data=*/false, nil);
  if (should_prompt)
    SetReauthPromptForSignInAndSync();
}

void AuthenticationService::ReloadCredentialsFromIdentities(
    bool keychain_reload) {
  if (is_reloading_credentials_)
    return;

  base::AutoReset<bool> auto_reset(&is_reloading_credentials_, true);

  HandleForgottenIdentity(nil, keychain_reload);
  if (!HasPrimaryIdentity(signin::ConsentLevel::kSignin))
    return;

  DCHECK(
      !user_approved_account_list_manager_.GetApprovedAccountIDList().empty());
  identity_manager_->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
  if (!keychain_reload) {
    // The changes come from Chrome, so we can approve this new account list,
    // since this change comes from the user.
    ApproveAccountList();
  }
}

bool AuthenticationService::IsAuthenticated() const {
  return HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

bool AuthenticationService::IsAuthenticatedIdentityManaged() const {
  return HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}

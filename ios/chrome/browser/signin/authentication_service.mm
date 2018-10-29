// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service.h"

#import <UIKit/UIKit.h>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_delegate.h"
#include "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/signin_util.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
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
std::string ChromeIdentityToAccountID(AccountTrackerService* account_tracker,
                                      ChromeIdentity* identity) {
  std::string gaia_id = base::SysNSStringToUTF8([identity gaiaID]);
  return account_tracker->FindAccountInfoByGaiaId(gaia_id).account_id;
}

}  // namespace

AuthenticationService::AuthenticationService(
    PrefService* pref_service,
    ProfileOAuth2TokenService* token_service,
    SyncSetupService* sync_setup_service,
    AccountTrackerService* account_tracker,
    SigninManager* signin_manager,
    browser_sync::ProfileSyncService* sync_service)
    : pref_service_(pref_service),
      token_service_(token_service),
      sync_setup_service_(sync_setup_service),
      account_tracker_(account_tracker),
      signin_manager_(signin_manager),
      sync_service_(sync_service),
      identity_service_observer_(this),
      weak_pointer_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(sync_setup_service_);
  DCHECK(account_tracker_);
  DCHECK(signin_manager_);
  DCHECK(sync_service_);
  token_service_->AddObserver(this);
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

  MigrateAccountsStoredInPrefsIfNeeded();

  HandleForgottenIdentity(nil, true /* should_prompt */);

  bool is_signed_in = IsAuthenticated();
  if (is_signed_in) {
    if (!sync_setup_service_->HasFinishedInitialSetup()) {
      // Sign out the user if sync was not configured after signing
      // in (see PM comments in http://crbug.com/339831 ).
      SignOut(signin_metrics::ABORT_SIGNIN, nil);
      SetPromptForSignIn(true);
      is_signed_in = false;
    }
  }
  breakpad_helper::SetCurrentlySignedIn(is_signed_in);

  OnApplicationEnterForeground();

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  foreground_observer_ =
      [center addObserverForName:UIApplicationWillEnterForegroundNotification
                          object:nil
                           queue:nil
                      usingBlock:^(NSNotification* notification) {
                        OnApplicationEnterForeground();
                      }];
  background_observer_ =
      [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                          object:nil
                           queue:nil
                      usingBlock:^(NSNotification* notification) {
                        OnApplicationEnterBackground();
                      }];

  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

void AuthenticationService::Shutdown() {
  token_service_->RemoveObserver(this);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:foreground_observer_];
  [center removeObserver:background_observer_];

  delegate_.reset();
}

void AuthenticationService::OnApplicationEnterForeground() {
  if (is_in_foreground_) {
    return;
  }

  // A change might have happened while in background, and SSOAuth didn't send
  // the corresponding notifications yet. Reload the credentials to catch up
  // with potentials changes.
  ReloadCredentialsFromIdentities(true /* should_prompt */);

  // Set |is_in_foreground_| only after handling forgotten identity.
  // This ensures that any changes made to the SSOAuth identities before this
  // are correctly seen as made while in background.
  is_in_foreground_ = true;

  // Accounts might have changed while the AuthenticationService was in
  // background. Check whether they changed, then store the current accounts.
  ComputeHaveAccountsChanged();
  StoreAccountsInPrefs();

  if (IsAuthenticated()) {
    bool sync_enabled = sync_setup_service_->IsSyncEnabled();
    LoginMethodAndSyncState loginMethodAndSyncState =
        sync_enabled ? SHARED_AUTHENTICATION_SYNC_ON
                     : SHARED_AUTHENTICATION_SYNC_OFF;
    UMA_HISTOGRAM_ENUMERATION("Signin.IOSLoginMethodAndSyncState",
                              loginMethodAndSyncState,
                              LOGIN_METHOD_AND_SYNC_STATE_COUNT);
  }
  UMA_HISTOGRAM_COUNTS_100("Signin.IOSNumberOfDeviceAccounts",
                           [ios::GetChromeBrowserProvider()
                                   ->GetChromeIdentityService()
                                   ->GetAllIdentities() count]);

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  ProfileOAuth2TokenServiceIOSDelegate* token_service_delegate =
      static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
          token_service_->GetDelegate());
  std::map<std::string, NSDictionary*> cached_mdm_infos(cached_mdm_infos_);
  cached_mdm_infos_.clear();
  for (const auto& cached_mdm_info : cached_mdm_infos) {
    token_service_delegate->AddOrUpdateAccount(cached_mdm_info.first);
  }
}

void AuthenticationService::OnApplicationEnterBackground() {
  is_in_foreground_ = false;
}

void AuthenticationService::SetPromptForSignIn(bool should_prompt) {
  if (ShouldPromptForSignIn() != should_prompt) {
    pref_service_->SetBoolean(prefs::kSigninShouldPromptForSigninAgain,
                              should_prompt);
  }
}

bool AuthenticationService::ShouldPromptForSignIn() {
  return pref_service_->GetBoolean(prefs::kSigninShouldPromptForSigninAgain);
}

void AuthenticationService::ComputeHaveAccountsChanged() {
  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  // While the AuthenticationService is in background, changes should be shown
  // to the user and |should_prompt| is true.
  ReloadCredentialsFromIdentities(!is_in_foreground_ /* should_prompt */);
  std::vector<std::string> new_accounts = token_service_->GetAccounts();
  std::vector<std::string> old_accounts = GetAccountsInPrefs();
  std::sort(new_accounts.begin(), new_accounts.end());
  std::sort(old_accounts.begin(), old_accounts.end());
  have_accounts_changed_ = old_accounts != new_accounts;
}

bool AuthenticationService::HaveAccountsChanged() {
  if (!is_in_foreground_) {
    // While AuthenticationService is in background, the value can change
    // without warning and needs to be recomputed every time.
    ComputeHaveAccountsChanged();
  }
  return have_accounts_changed_;
}

void AuthenticationService::MigrateAccountsStoredInPrefsIfNeeded() {
  if (account_tracker_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    return;
  }
  DCHECK_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker_->GetMigrationState());
  if (pref_service_->GetBoolean(prefs::kSigninLastAccountsMigrated)) {
    // Already migrated.
    return;
  }

  std::vector<std::string> emails = GetAccountsInPrefs();
  base::ListValue accounts_pref_value;
  for (const std::string& email : emails) {
    AccountInfo account_info = account_tracker_->FindAccountInfoByEmail(email);
    if (!account_info.email.empty()) {
      DCHECK(!account_info.gaia.empty());
      accounts_pref_value.AppendString(account_info.account_id);
    } else {
      // The account for |email| was removed since the last application cold
      // start. Insert |kFakeAccountIdForRemovedAccount| to ensure
      // |have_accounts_changed_| will be set to true and the removal won't be
      // silently ignored.
      accounts_pref_value.AppendString(kFakeAccountIdForRemovedAccount);
    }
  }
  pref_service_->Set(prefs::kSigninLastAccounts, accounts_pref_value);
  pref_service_->SetBoolean(prefs::kSigninLastAccountsMigrated, true);
}

void AuthenticationService::StoreAccountsInPrefs() {
  std::vector<std::string> accounts(token_service_->GetAccounts());
  base::ListValue accounts_pref_value;
  for (const std::string& account : accounts)
    accounts_pref_value.AppendString(account);
  pref_service_->Set(prefs::kSigninLastAccounts, accounts_pref_value);
}

std::vector<std::string> AuthenticationService::GetAccountsInPrefs() {
  std::vector<std::string> accounts;
  const base::ListValue* accounts_pref =
      pref_service_->GetList(prefs::kSigninLastAccounts);
  for (size_t i = 0; i < accounts_pref->GetSize(); ++i) {
    std::string account;
    if (accounts_pref->GetString(i, &account) && !account.empty()) {
      accounts.push_back(account);
    } else {
      NOTREACHED();
    }
  }
  return accounts;
}

ChromeIdentity* AuthenticationService::GetAuthenticatedIdentity() {
  // There is no authenticated identity if there is no signed in user or if the
  // user signed in via the client login flow.
  if (!IsAuthenticated())
    return nil;

  std::string authenticated_account_id =
      signin_manager_->GetAuthenticatedAccountId();

  std::string authenticated_gaia_id =
      account_tracker_->GetAccountInfo(authenticated_account_id).gaia;
  if (authenticated_gaia_id.empty())
    return nil;

  return ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->GetIdentityWithGaiaID(authenticated_gaia_id);
}

void AuthenticationService::SignIn(ChromeIdentity* identity,
                                   const std::string& hosted_domain) {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetChromeIdentityService()
             ->IsValidIdentity(identity));

  // The account info needs to be seeded for the primary account id before
  // signing in.
  // TODO(msarda): http://crbug.com/478770 Seed account information for
  // all secondary accounts.
  AccountInfo info;
  info.gaia = base::SysNSStringToUTF8([identity gaiaID]);
  info.email = GetCanonicalizedEmailForIdentity(identity);
  info.hosted_domain = hosted_domain;
  std::string new_authenticated_account_id =
      account_tracker_->SeedAccountInfo(info);
  std::string old_authenticated_account_id =
      signin_manager_->GetAuthenticatedAccountId();
  // |SigninManager::SetAuthenticatedAccountId| simply ignores the call if
  // there is already a signed in user. Check that there is no signed in account
  // or that the new signed in account matches the old one to avoid a mismatch
  // between the old and the new authenticated accounts.
  if (!old_authenticated_account_id.empty())
    CHECK_EQ(new_authenticated_account_id, old_authenticated_account_id);

  SetPromptForSignIn(false);
  sync_setup_service_->PrepareForFirstSyncSetup();

  // Update the SigninManager with the new logged in identity.
  std::string new_authenticated_username =
      account_tracker_->GetAccountInfo(new_authenticated_account_id).email;
  signin_manager_->OnExternalSigninCompleted(new_authenticated_username);

  // Reload all credentials to match the desktop model. Exclude all the
  // accounts ids that are the primary account ids on other profiles.
  ProfileOAuth2TokenServiceIOSDelegate* tokenServiceDelegate =
      static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
          token_service_->GetDelegate());
  tokenServiceDelegate->ReloadCredentials(new_authenticated_account_id);
  StoreAccountsInPrefs();

  // Kick-off sync: The authentication error UI (sign in infobar and warning
  // badge in settings screen) check the sync auth error state. Sync
  // needs to be kicked off so that it resets the auth error quickly once
  // |identity| is reauthenticated.
  // TODO(msarda): Remove this code once the authentication error UI checks
  // SigninGlobalError instead of the sync auth error state.
  // crbug.com/289493
  sync_service_->RequestStart();
  breakpad_helper::SetCurrentlySignedIn(true);
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    ProceduralBlock completion) {
  if (!IsAuthenticated()) {
    if (completion)
      completion();
    return;
  }

  bool is_managed = IsAuthenticatedIdentityManaged();

  sync_service_->RequestStop(syncer::SyncService::CLEAR_DATA);
  signin_manager_->SignOut(signout_source,
                           signin_metrics::SignoutDelete::IGNORE_METRIC);
  breakpad_helper::SetCurrentlySignedIn(false);
  cached_mdm_infos_.clear();
  if (is_managed) {
    delegate_->ClearBrowsingData(completion);
  } else if (completion) {
    completion();
  }
}

NSDictionary* AuthenticationService::GetCachedMDMInfo(
    ChromeIdentity* identity) {
  auto it = cached_mdm_infos_.find(
      ChromeIdentityToAccountID(account_tracker_, identity));

  if (it == cached_mdm_infos_.end()) {
    return nil;
  }

  if (!token_service_->RefreshTokenHasError(it->first)) {
    // Account has no error, invalidate the cache.
    cached_mdm_infos_.erase(it);
    return nil;
  }

  return it->second;
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    ChromeIdentity* identity) {
  return GetCachedMDMInfo(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    ChromeIdentity* identity) {
  NSDictionary* cached_info = GetCachedMDMInfo(identity);
  if (!cached_info) {
    return false;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  identity_service->HandleMDMNotification(identity, cached_info, ^(bool){
                                                    });
  return true;
}

void AuthenticationService::ResetChromeIdentityServiceObserverForTesting() {
  identity_service_observer_.RemoveAll();
  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnEndBatchChanges() {
  if (is_in_foreground_) {
    // Accounts maybe have been excluded or included from the current browser
    // state, without any change to the identity list.
    // Store the current list of accounts to make sure it is up-to-date.
    StoreAccountsInPrefs();
  }
}

void AuthenticationService::OnIdentityListChanged() {
  // The list of identities may change while in an authorized call. Signing out
  // the authenticated user at this time may lead to crashes (e.g.
  // http://crbug.com/398431 ).
  // Handle the change of the identity list on the next message loop cycle.
  // If the identity list changed while the authentication service was in
  // background, the user should be warned about it.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AuthenticationService::HandleIdentityListChanged,
                            GetWeakPtr(), !is_in_foreground_));
}

bool AuthenticationService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info) {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
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
      // If the identiy is blocked, sign out of the account. As only managed
      // account can be blocked, this will clear the associated browsing data.
      if (identity == weak_ptr->GetAuthenticatedIdentity()) {
        weak_ptr->SignOut(signin_metrics::ABORT_SIGNIN, nil);
      }
    }
  };
  if (identity_service->HandleMDMNotification(identity, user_info, callback)) {
    cached_mdm_infos_[ChromeIdentityToAccountID(account_tracker_, identity)] =
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
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
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
      FROM_HERE, base::Bind(&AuthenticationService::HandleForgottenIdentity,
                            base::Unretained(this), identity, true));
}

void AuthenticationService::OnChromeIdentityServiceWillBeDestroyed() {
  identity_service_observer_.RemoveAll();
}

void AuthenticationService::HandleIdentityListChanged(bool should_prompt) {
  ReloadCredentialsFromIdentities(should_prompt);

  if (is_in_foreground_) {
    // Update the accounts currently stored in the profile prefs.
    StoreAccountsInPrefs();
  }
}

void AuthenticationService::HandleForgottenIdentity(
    ChromeIdentity* invalid_identity,
    bool should_prompt) {
  if (!IsAuthenticated()) {
    // User is not signed in. Nothing to do here.
    return;
  }

  ChromeIdentity* authenticated_identity = GetAuthenticatedIdentity();
  if (authenticated_identity && authenticated_identity != invalid_identity) {
    // |authenticated_identity| exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  // Sign the user out.
  //
  // The authenticated id is removed from the device (either by the user or
  // when an invalid credentials is received from the server). There is no
  // upstream entry in enum |signin_metrics::ProfileSignout| for this event. The
  // temporary solution is to map this to |ABORT_SIGNIN|.
  //
  // TODO(msarda): http://crbug.com/416823 Add another entry in Chromium
  // upstream for |signin_metrics| that matches the device identity was lost.
  SignOut(signin_metrics::ABORT_SIGNIN, nil);
  SetPromptForSignIn(should_prompt);
}

void AuthenticationService::ReloadCredentialsFromIdentities(
    bool should_prompt) {
  if (is_reloading_credentials_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&is_reloading_credentials_, true);

  HandleForgottenIdentity(nil, should_prompt);
  if (GetAuthenticatedUserEmail()) {
    ProfileOAuth2TokenServiceIOSDelegate* token_service_delegate =
        static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
            token_service_->GetDelegate());
    token_service_delegate->ReloadCredentials();
  }
}

bool AuthenticationService::IsAuthenticated() {
  if (!is_in_foreground_) {
    // While AuthenticationService is in background, the list of accounts can
    // change without a OnIdentityListChanged notification being fired.
    // Reload credentials to ensure that the user is still authenticated.
    ReloadCredentialsFromIdentities(true /* should_prompt */);
  }
  return signin_manager_->IsAuthenticated();
}

NSString* AuthenticationService::GetAuthenticatedUserEmail() {
  if (!IsAuthenticated())
    return nil;
  std::string authenticated_username =
      signin_manager_->GetAuthenticatedAccountInfo().email;
  DCHECK_LT(0U, authenticated_username.length());
  return base::SysUTF8ToNSString(authenticated_username);
}

bool AuthenticationService::IsAuthenticatedIdentityManaged() {
  std::string hosted_domain =
      signin_manager_->GetAuthenticatedAccountInfo().hosted_domain;
  return !hosted_domain.empty() &&
         hosted_domain != AccountTrackerService::kNoHostedDomainFound;
}

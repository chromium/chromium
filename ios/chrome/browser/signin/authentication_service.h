// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_

#include <string>
#include <vector>

#import "base/ios/block_types.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

namespace syncer {
class SyncService;
}

class AuthenticationServiceDelegate;
class AuthenticationServiceFake;
@class ChromeIdentity;
class PrefService;
class SyncSetupService;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class AuthenticationService : public KeyedService,
                              public signin::IdentityManager::Observer,
                              public ios::ChromeIdentityService::Observer {
 public:
  AuthenticationService(PrefService* pref_service,
                        SyncSetupService* sync_setup_service,
                        signin::IdentityManager* identity_manager,
                        syncer::SyncService* sync_service);
  ~AuthenticationService() override;

  // Registers the preferences used by AuthenticationService;
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether the AuthenticationService has been initialized. It is
  // a fatal error to invoke any method on this object except Initialize()
  // if this method returns false.
  bool initialized() const { return initialized_; }

  // Initializes the AuthenticationService.
  void Initialize(std::unique_ptr<AuthenticationServiceDelegate> delegate);

  // KeyedService
  void Shutdown() override;

  // Reminds user to Sign in to Chrome when a new tab is opened.
  void SetPromptForSignIn();

  // Clears the reminder to Sign in to Chrome when a new tab is opened.
  void ResetPromptForSignIn();

  // Returns whether user should be prompted to Sign in to Chrome.
  bool ShouldPromptForSignIn() const;

  // Returns whether the token service accounts have changed since the last time
  // they were stored in the browser state prefs. This storing happens every
  // time the accounts change in foreground.
  // This reloads the cached accounts if the information might be stale.
  virtual bool HaveAccountsChangedWhileInBackground() const;

  // ChromeIdentity management

  // Returns true if the user is signed in.
  // While the AuthenticationService is in background, this will reload the
  // credentials to ensure the value is up to date.
  virtual bool IsAuthenticated() const;

  // Returns true if the user is signed in and the identity is considered
  // managed.
  virtual bool IsAuthenticatedIdentityManaged() const;

  // Retrieves the identity of the currently authenticated user or |nil| if
  // either the user is not authenticated, or is authenticated through
  // ClientLogin.
  // Virtual for testing.
  virtual ChromeIdentity* GetAuthenticatedIdentity() const;

  // Signs |identity| in to Chrome with |hosted_domain| as its hosted domain,
  // pauses sync and logs |identity| in to http://google.com.
  // Virtual for testing.
  virtual void SignIn(ChromeIdentity* identity);

  // Signs the authenticated user out of Chrome and clears the browsing
  // data if the account is managed. If force_clear_browsing_data is true,
  // clears the browsing data unconditionally.
  // Virtual for testing.
  virtual void SignOut(signin_metrics::ProfileSignout signout_source,
                       bool force_clear_browsing_data,
                       ProceduralBlock completion);

  // Returns whether there is a cached associated MDM error for |identity|.
  bool HasCachedMDMErrorForIdentity(ChromeIdentity* identity) const;

  // Shows the MDM Error dialog for |identity| if it has an associated MDM
  // error. Returns true if |identity| had an associated error, false otherwise.
  bool ShowMDMErrorDialogForIdentity(ChromeIdentity* identity);

  // Resets the ChromeIdentityService observer to the one available in the
  // ChromeBrowserProvider. Used for testing when changing the
  // ChromeIdentityService to or from a fake one.
  void ResetChromeIdentityServiceObserverForTesting();

  // Returns a weak pointer of this.
  base::WeakPtr<AuthenticationService> GetWeakPtr();

  // This needs to be invoked when the application enters foreground to
  // sync the accounts between the IdentityManager and the SSO library.
  void OnApplicationWillEnterForeground();

  // This needs to be invoked when the application enters background to
  // sync the accounts between the IdentityManager and the SSO library.
  void OnApplicationDidEnterBackground();

 private:
  friend class AuthenticationServiceTest;
  friend class AuthenticationServiceFake;

  // Migrates the token service accounts stored in prefs from emails to account
  // ids.
  void MigrateAccountsStoredInPrefsIfNeeded();

  // Saves the last known list of accounts from the token service when
  // the app is in foreground. This can be used when app comes back from
  // background to detect if any changes occurred to the list. Must only
  // be called when the application is in foreground.
  // See HaveAccountsChangesWhileInBackground().
  void StoreKnownAccountsWhileInForeground();

  // Gets the accounts previously stored as the foreground accounts in the
  // browser state prefs.
  // Returns the list of previously stored known accounts. This list
  // is only updated when the app is in foreground and used to detect
  // if any change occurred while the app was in background.
  // See HaveAccountsChangesWhileInBackground().
  std::vector<CoreAccountId> GetLastKnownAccountsFromForeground();

  // Returns the cached MDM infos associated with |identity|. If the cache
  // is stale for |identity|, the entry might be removed.
  NSDictionary* GetCachedMDMInfo(ChromeIdentity* identity) const;

  // Handles an MDM notification |user_info| associated with |identity|.
  // Returns whether the notification associated with |user_info| was fully
  // handled.
  bool HandleMDMNotification(ChromeIdentity* identity, NSDictionary* user_info);

  // Reloads the accounts to reflect the change in the SSO identities. If
  // |should_store_accounts_| is true, it will also store the available accounts
  // in the  browser state prefs.
  //
  // |in_foreground| indicates whether the application was in foreground when
  // the identity list change notification was received.
  void HandleIdentityListChanged();

  // Verifies that the authenticated user is still associated with a valid
  // ChromeIdentity. This method must only be called when the user is
  // authenticated with the shared authentication library. If there is no valid
  // ChromeIdentity associated with the currently authenticated user, or the
  // identity is |invalid_identity|, this method will sign the user out.
  //
  // |invalid_identity| is an additional identity to consider invalid. It can be
  // nil if there is no such additional identity to ignore.
  //
  // |should_prompt| indicates whether the user should be prompted with the
  // resign-in infobar if the method signs out.
  void HandleForgottenIdentity(ChromeIdentity* invalid_identity,
                               bool should_prompt);

  // Checks if the authenticated identity was removed by calling
  // |HandleForgottenIdentity|. Reloads the OAuth2 token service accounts if the
  // authenticated identity is still present.
  // |should_prompt| indicates whether the user should be prompted if the
  // authenticated identity was removed.
  void ReloadCredentialsFromIdentities(bool should_prompt);

  // Computes whether the available accounts have changed since the last time
  // they were stored in the  browser state prefs.
  //
  // This method should only be called when the application is in background
  // or when the application is entering foregorund.
  void UpdateHaveAccountsChangedWhileInBackground();

  // Returns whether the application is currently in the foreground or not.
  bool InForeground() const;

  // signin::IdentityManager::Observer implementation.
  void OnEndBatchOfRefreshTokenStateChanges() override;

  // ChromeIdentityServiceObserver implementation.
  void OnIdentityListChanged() override;
  void OnAccessTokenRefreshFailed(ChromeIdentity* identity,
                                  NSDictionary* user_info) override;
  void OnChromeIdentityServiceWillBeDestroyed() override;

  // The delegate for this AuthenticationService. It is invalid to call any
  // method on this object except Initialize() or Shutdown() if this pointer
  // is null.
  std::unique_ptr<AuthenticationServiceDelegate> delegate_;

  // Pointer to the KeyedServices used by AuthenticationService.
  PrefService* pref_service_ = nullptr;
  SyncSetupService* sync_setup_service_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;
  syncer::SyncService* sync_service_ = nullptr;

  // Whether Initialized has been called.
  bool initialized_ = false;

  // Whether the accounts have changed while the AuthenticationService was in
  // background. When the AuthenticationService is in background, this value
  // cannot be trusted.
  bool have_accounts_changed_while_in_background_ = false;

  // Whether the AuthenticationService is currently reloading credentials, used
  // to avoid an infinite reloading loop.
  bool is_reloading_credentials_ = false;

  // Map between account IDs and their associated MDM error.
  mutable std::map<CoreAccountId, NSDictionary*> cached_mdm_infos_;

  ScopedObserver<ios::ChromeIdentityService,
                 ios::ChromeIdentityService::Observer>
      identity_service_observer_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_;

  base::WeakPtrFactory<AuthenticationService> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticationService);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_H_

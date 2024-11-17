// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_H_

#import <CoreFoundation/CoreFoundation.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer.h"

class Browser;
@protocol PolicyChangeCommands;
class PolicyWatcherBrowserAgentObserver;

// NSUserDefault global key to track if the sync disable alert was shown.
extern NSString* kSyncDisabledAlertShownKey;

// Service that listens for policy-controlled prefs changes and sends commands
// to update the UI accordingly.
class PolicyWatcherBrowserAgent
    : public AuthenticationServiceObserver,
      public BrowserUserData<PolicyWatcherBrowserAgent> {
 public:
  ~PolicyWatcherBrowserAgent() override;

  void AddObserver(PolicyWatcherBrowserAgentObserver* observer);
  void RemoveObserver(PolicyWatcherBrowserAgentObserver* observer);

  // Notifies the BrowserAgent that a SignIn UI was dismissed as a result of a
  // policy SignOut.
  void SignInUIDismissed();

  // Starts observing the kSigninAllowed pref and trigger a SignOut if the pref
  // has changed before the BrowserAgent start the observation. `handler` is
  // used to send UI commands when the SignOut is done.
  void Initialize(id<PolicyChangeCommands> handler);

 private:
  friend class BrowserUserData<PolicyWatcherBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit PolicyWatcherBrowserAgent(Browser* browser);

  // Handler for changes to kSigninAllowed. When the pref changes to `false`,
  // sends a command to the SceneController to dismiss any in-progress sign-in
  // UI.
  void ForceSignOutIfSigninDisabled();

  // Handler for change to kSyncManaged. When the pref changes to `true`,
  // sends a command to the handler to show a prompt.
  void ShowSyncDisabledPromptIfNeeded();

  // Handler for changes to kAllowChromeDataInBackups. Excludes the entire app
  // container from iCloud backup when the pref changes to `false`, and removes
  // this exclusion when the pref changs to `true`.
  void UpdateAppContainerBackupExclusion();

  // Callback called when the sign out is complete.
  void OnSignOutComplete();

  // AuthenticationServiceObserver implementation.
  void OnPrimaryAccountRestricted() override;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;

  // The AuthenticationService.
  raw_ptr<AuthenticationService> auth_service_ = nullptr;

  // Registrar for local state pref change notifications.
  PrefChangeRegistrar prefs_change_observer_;

  // Registrar for profile pref change notifications.
  PrefChangeRegistrar profile_prefs_change_observer_;

  // List of observers notified of changes to the policy.
  base::ObserverList<PolicyWatcherBrowserAgentObserver, true> observers_;

  // Whether a Sign Out is currently in progress.
  bool sign_out_in_progress_ = false;

  // Handler to send commands.
  id<PolicyChangeCommands> handler_ = nil;

  // AuthenticationService observer.
  base::ScopedObservation<AuthenticationService, AuthenticationServiceObserver>
      auth_service_observation_{this};

  // WeakPtrFactory should be last.
  base::WeakPtrFactory<PolicyWatcherBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_WATCHER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_WATCHER_BROWSER_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/main/browser_user_data.h"

class Browser;
@protocol PolicySignoutPromptCommands;
class PolicyWatcherBrowserAgentObserver;
class PrefChangeRegistrar;

// Service that listens for policy-controlled prefs changes and sends commands
// to update the UI accordingly.
class PolicyWatcherBrowserAgent
    : public BrowserUserData<PolicyWatcherBrowserAgent> {
 public:
  ~PolicyWatcherBrowserAgent() override;

  void AddObserver(PolicyWatcherBrowserAgentObserver* observer);
  void RemoveObserver(PolicyWatcherBrowserAgentObserver* observer);

  // Notifies the BrowserAgent that a SignIn UI was dismissed as a result of a
  // policy SignOut.
  void SignInUIDismissed();

  // Starts observing the kSigninAllowed pref and trigger a SignOut if the pref
  // has changed before the BrowserAgent start the observation. |handler| is
  // used to send UI commands when the SignOut is done.
  void Initialize(id<PolicySignoutPromptCommands> handler);

 private:
  explicit PolicyWatcherBrowserAgent(Browser* browser);
  friend class BrowserUserData<PolicyWatcherBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Handler for changes to kSigninAllowed. When the pref changes to |false|,
  // sends a command to the SceneController to dismiss any in-progress sign-in
  // UI.
  void ForceSignOutIfSigninDisabled();

  // Callback called when the sign out is complete.
  void OnSignOutComplete();

  // The owning Browser.
  Browser* browser_;

  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> prefs_change_observer_;

  // List of observers notified of changes to the policy.
  base::ObserverList<PolicyWatcherBrowserAgentObserver, true> observers_;

  // Whether a Sign Out is currently in progress.
  bool sign_out_in_progress_ = false;

  // Handler to send commands.
  id<PolicySignoutPromptCommands> handler_ = nil;

  // WeakPtrFactory should be last.
  base::WeakPtrFactory<PolicyWatcherBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_WATCHER_BROWSER_AGENT_H_

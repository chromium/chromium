// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_WATCHER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_WATCHER_BROWSER_AGENT_H_

#include <memory>

#include "base/macros.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"

class Browser;
class PrefChangeRegistrar;

// Service that listens for policy-controlled prefs changes and sends commands
// to update the UI accordingly.
class PolicyWatcherBrowserAgent
    : public BrowserUserData<PolicyWatcherBrowserAgent> {
 public:
  ~PolicyWatcherBrowserAgent() override;

  // Sets the command dispatcher to use for sneding UI commands when prefs
  // change. Also starts observing the kSigninAllowed pref.
  void SetApplicationCommandsHandler(id<ApplicationCommands> handler);

 private:
  explicit PolicyWatcherBrowserAgent(Browser* browser);
  friend class BrowserUserData<PolicyWatcherBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Handler for changes to kSigninAllowed. When the pref changes to |false|,
  // sends a command to the SceneController to dismiss any in-progress sign-in
  // UI.
  void ForceSignOutIfSigninDisabled();

  // The owning Browser.
  Browser* browser_;

  // The command handler to use for sending ApplicationCommands. Must be set by
  //
  id<ApplicationCommands> application_commands_handler_;

  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> prefs_change_observer_;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BROWSER_AGENT_H_

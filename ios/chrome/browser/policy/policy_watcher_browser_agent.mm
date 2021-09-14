// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_watcher_browser_agent.h"

#import <Foundation/Foundation.h>

#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/policy_watcher_browser_agent_observer.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/commands/policy_change_commands.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* kSyncDisabledAlertShownKey = @"SyncDisabledAlertShown";

BROWSER_USER_DATA_KEY_IMPL(PolicyWatcherBrowserAgent)

PolicyWatcherBrowserAgent::PolicyWatcherBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  prefs_change_observer_.Init(GetApplicationContext()->GetLocalState());
  browser_prefs_change_observer_.Init(browser->GetBrowserState()->GetPrefs());
}

PolicyWatcherBrowserAgent::~PolicyWatcherBrowserAgent() = default;

void PolicyWatcherBrowserAgent::SignInUIDismissed() {
  // Do nothing if the sign out is still in progress.
  if (sign_out_in_progress_)
    return;

  [handler_ showPolicySignoutPrompt];
}

void PolicyWatcherBrowserAgent::Initialize(id<PolicyChangeCommands> handler) {
  DCHECK(!handler_);
  DCHECK(handler);
  handler_ = handler;

  auth_service_ = AuthenticationServiceFactory::GetForBrowserState(
      browser_->GetBrowserState());
  DCHECK(auth_service_);
  auth_service_observation_.Observe(auth_service_);

  // BrowserSignin policy: start observing the kSigninAllowed pref. When the
  // pref becomes false, send a UI command to sign the user out. This requires
  // the given command dispatcher to be fully configured.
  prefs_change_observer_.Add(
      prefs::kBrowserSigninPolicy,
      base::BindRepeating(
          &PolicyWatcherBrowserAgent::ForceSignOutIfSigninDisabled,
          base::Unretained(this)));

  // Try to sign out in case the policy changed since last time. This should be
  // done after the handler is set to make sure the UI can be displayed.
  ForceSignOutIfSigninDisabled();

  browser_prefs_change_observer_.Add(
      syncer::prefs::kSyncManaged,
      base::BindRepeating(
          &PolicyWatcherBrowserAgent::ShowSyncDisabledAlertIfNeeded,
          base::Unretained(this)));

  // Try to show the alert in case the policy changed since last time.
  ShowSyncDisabledAlertIfNeeded();
}

void PolicyWatcherBrowserAgent::ForceSignOutIfSigninDisabled() {
  DCHECK(handler_);
  DCHECK(auth_service_);
  if (!signin::IsSigninAllowedByPolicy()) {
    if (auth_service_->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
      sign_out_in_progress_ = true;
      base::UmaHistogramBoolean("Enterprise.BrowserSigninIOS.SignedOutByPolicy",
                                true);

      base::WeakPtr<PolicyWatcherBrowserAgent> weak_ptr =
          weak_factory_.GetWeakPtr();
      // Sign the user out, but keep synced data (bookmarks, passwords, etc)
      // locally to be consistent with the policy's behavior on other platforms.
      auth_service_->SignOut(
          signin_metrics::ProfileSignout::SIGNOUT_PREF_CHANGED,
          /*force_clear_browsing_data=*/false, ^{
            weak_ptr->OnSignOutComplete();
          });
    }

    for (auto& observer : observers_) {
      observer.OnSignInDisallowed(this);
    }
  }
}

void PolicyWatcherBrowserAgent::ShowSyncDisabledAlertIfNeeded() {
  NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
  BOOL syncDisabledAlertShown =
      [standard_defaults boolForKey:kSyncDisabledAlertShownKey];
  BOOL isSyncDisabledByAdministrator =
      browser_->GetBrowserState()->GetPrefs()->GetBoolean(
          syncer::prefs::kSyncManaged);

  if (!syncDisabledAlertShown && isSyncDisabledByAdministrator) {
    [handler_ showSyncDisabledAlert];
    // Will never trigger again unless policy changes.
    [standard_defaults setBool:YES forKey:kSyncDisabledAlertShownKey];
  } else if (syncDisabledAlertShown && !isSyncDisabledByAdministrator) {
    // Will trigger again, if policy is turned back on.
    [standard_defaults setBool:NO forKey:kSyncDisabledAlertShownKey];
  }
}

void PolicyWatcherBrowserAgent::AddObserver(
    PolicyWatcherBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void PolicyWatcherBrowserAgent::RemoveObserver(
    PolicyWatcherBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PolicyWatcherBrowserAgent::OnSignOutComplete() {
  SceneState* scene_state =
      SceneStateBrowserAgent::FromBrowser(browser_)->GetSceneState();
  sign_out_in_progress_ = false;
  BOOL sceneIsActive =
      scene_state.activationLevel >= SceneActivationLevelForegroundActive;
  if (sceneIsActive) {
    // Try to show the signout prompt in all cases: if there is a sign
    // in in progress, the UI will prevent the prompt from showing.
    [handler_ showPolicySignoutPrompt];
  } else {
    scene_state.appState.shouldShowPolicySignoutPrompt = YES;
  }
}

void PolicyWatcherBrowserAgent::OnPrimaryAccountRestricted() {
  [handler_ showEnterpriseSignout];
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_watcher_browser_agent.h"

#import <Foundation/Foundation.h>

#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/policy_watcher_browser_agent_observer.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/commands/policy_signout_commands.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(PolicyWatcherBrowserAgent)

PolicyWatcherBrowserAgent::PolicyWatcherBrowserAgent(Browser* browser)
    : browser_(browser),
      prefs_change_observer_(std::make_unique<PrefChangeRegistrar>()) {
  prefs_change_observer_->Init(browser_->GetBrowserState()->GetPrefs());
}

PolicyWatcherBrowserAgent::~PolicyWatcherBrowserAgent() {}

void PolicyWatcherBrowserAgent::SignInUIDismissed() {
  // Do nothing if the sign out is still in progress.
  if (sign_out_in_progress_)
    return;

  [handler_ showPolicySignoutPrompt];
}

void PolicyWatcherBrowserAgent::Initialize(
    id<PolicySignoutPromptCommands> handler) {
  DCHECK(!handler_);
  DCHECK(handler);
  handler_ = handler;

  // BrowserSignin policy: start observing the kSigninAllowed pref for non-OTR
  // browsers. When the pref becomes false, send a UI command to sign the user
  // out. This requires the given command dispatcher to be fully configured.
  if (browser_->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  prefs_change_observer_->Add(
      prefs::kSigninAllowed,
      base::BindRepeating(
          &PolicyWatcherBrowserAgent::ForceSignOutIfSigninDisabled,
          base::Unretained(this)));

  // Try to sign out in case the policy changed since last time. This should be
  // done after the handler is set to make sure the UI can be displayed.
  ForceSignOutIfSigninDisabled();
}

void PolicyWatcherBrowserAgent::ForceSignOutIfSigninDisabled() {
  DCHECK(handler_);
  if (!browser_->GetBrowserState()->GetPrefs()->GetBoolean(
          prefs::kSigninAllowed)) {
    AuthenticationService* service =
        AuthenticationServiceFactory::GetForBrowserState(
            browser_->GetBrowserState());

    if (service->IsAuthenticated()) {
      sign_out_in_progress_ = true;
      base::UmaHistogramBoolean("Enterprise.BrowserSigninIOS.SignedOutByPolicy",
                                true);

      SceneState* scene_state =
          SceneStateBrowserAgent::FromBrowser(browser_)->GetSceneState();
      // Sign the user out, but keep synced data (bookmarks, passwords, etc)
      // locally to be consistent with the policy's behavior on other platforms.
      service->SignOut(
          signin_metrics::ProfileSignout::SIGNOUT_PREF_CHANGED,
          /*force_clear_browsing_data=*/false, ^{
            sign_out_in_progress_ = false;
            BOOL sceneIsActive = scene_state.activationLevel >=
                                 SceneActivationLevelForegroundActive;
            if (sceneIsActive) {
              // Try to show the signout prompt in all cases: if there is a sign
              // in in progress, the UI will prevent the prompt from showing.
              [handler_ showPolicySignoutPrompt];
            } else {
              scene_state.appState.shouldShowPolicySignoutPrompt = YES;
            }
          });
    }

    for (auto& observer : observers_) {
      observer.OnSignInDisallowed(this);
    }
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

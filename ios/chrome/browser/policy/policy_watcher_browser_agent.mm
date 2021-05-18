// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_watcher_browser_agent.h"

#import <Foundation/Foundation.h>

#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/policy_watcher_browser_agent_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(PolicyWatcherBrowserAgent)

PolicyWatcherBrowserAgent::PolicyWatcherBrowserAgent(Browser* browser)
    : browser_(browser),
      prefs_change_observer_(std::make_unique<PrefChangeRegistrar>()) {
  prefs_change_observer_->Init(browser_->GetBrowserState()->GetPrefs());
}

void PolicyWatcherBrowserAgent::Initialize() {
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
  ForceSignOutIfSigninDisabled();
}

PolicyWatcherBrowserAgent::~PolicyWatcherBrowserAgent() {}

void PolicyWatcherBrowserAgent::ForceSignOutIfSigninDisabled() {
  if (!browser_->GetBrowserState()->GetPrefs()->GetBoolean(
          prefs::kSigninAllowed)) {
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

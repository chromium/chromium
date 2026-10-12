// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_DISABLED_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_DISABLED_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class AuthenticationService;
class Browser;

// Browser agent that observes `prefs::kSigninAllowedOnDevice` and signs out
// the user (switching to the personal profile first if needed) when sign-in is
// disabled on the device.
// Note: Sign-out due to enterprise policy (`prefs::kBrowserSigninPolicy`) is
// handled by `PolicyWatcherBrowserAgent`
// (`ios/chrome/browser/policy/model/policy_watcher_browser_agent.h`).
class SigninDisabledBrowserAgent
    : public BrowserUserData<SigninDisabledBrowserAgent> {
 public:
  ~SigninDisabledBrowserAgent() override;

 private:
  friend class BrowserUserData<SigninDisabledBrowserAgent>;

  explicit SigninDisabledBrowserAgent(Browser* browser);

  // Signs out the user (switching to the personal profile if needed) when
  // `prefs::kSigninAllowedOnDevice` is false.
  void SignOutIfNeeded();

  // AuthenticationService for the browser's profile.
  raw_ptr<AuthenticationService> authentication_service_ = nullptr;

  // Registrar for local state pref change notifications.
  PrefChangeRegistrar local_pref_change_registrar_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_DISABLED_BROWSER_AGENT_H_

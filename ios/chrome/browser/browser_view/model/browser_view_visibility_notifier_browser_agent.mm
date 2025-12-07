// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"

BrowserViewVisibilityNotifierBrowserAgent::
    BrowserViewVisibilityNotifierBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

BrowserViewVisibilityNotifierBrowserAgent::
    ~BrowserViewVisibilityNotifierBrowserAgent() = default;

base::CallbackListSubscription BrowserViewVisibilityNotifierBrowserAgent::
    RegisterBrowserVisibilityStateChangedCallback(
        const BrowserViewVisibilityStateChangedCallback& callback) {
  return callbacks_.Add(callback);
}

BrowserViewVisibilityStateChangedCallback
BrowserViewVisibilityNotifierBrowserAgent::GetNotificationCallback() {
  using Self = BrowserViewVisibilityNotifierBrowserAgent;
  return base::BindRepeating(&Self::BrowserViewVisibilityStateDidChange,
                             weak_ptr_factory_.GetWeakPtr());
}

void BrowserViewVisibilityNotifierBrowserAgent::
    BrowserViewVisibilityStateDidChange(
        BrowserViewVisibilityState current_state,
        BrowserViewVisibilityState previous_state) {
  callbacks_.Notify(current_state, previous_state);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_handler.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer.h"

BrowserViewVisibilityNotifierBrowserAgent::
    BrowserViewVisibilityNotifierBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  BrowserViewVisibilityStateChangeCallback callback =
      base::BindRepeating(&BrowserViewVisibilityNotifierBrowserAgent::
                              BrowserViewVisibilityStateDidChange,
                          AsWeakPtr());
  visibility_handler_ = [[BrowserViewVisibilityHandler alloc]
      initWithVisibilityChangeCallback:callback];
}

BrowserViewVisibilityNotifierBrowserAgent::
    ~BrowserViewVisibilityNotifierBrowserAgent() {}

void BrowserViewVisibilityNotifierBrowserAgent::AddObserver(
    BrowserViewVisibilityObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserViewVisibilityNotifierBrowserAgent::RemoveObserver(
    BrowserViewVisibilityObserver* observer) {
  observers_.RemoveObserver(observer);
}

id<BrowserViewVisibilityAudience>
BrowserViewVisibilityNotifierBrowserAgent::GetBrowserViewVisibilityAudience() {
  return visibility_handler_;
}

base::WeakPtr<BrowserViewVisibilityNotifierBrowserAgent>
BrowserViewVisibilityNotifierBrowserAgent::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserViewVisibilityNotifierBrowserAgent::
    BrowserViewVisibilityStateDidChange(
        BrowserViewVisibilityState current_state,
        BrowserViewVisibilityState previous_state) {
  for (auto& observer : observers_) {
    observer.BrowserViewVisibilityStateDidChange(current_state, previous_state);
  }
}

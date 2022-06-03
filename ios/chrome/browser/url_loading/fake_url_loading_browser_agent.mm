// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void FakeUrlLoadingBrowserAgent::InjectForBrowser(Browser* browser) {
  // No other instance should already have been attached.
  DCHECK(!FromBrowser(browser));
  browser->SetUserData(
      UrlLoadingBrowserAgent::UserDataKey(),
      base::WrapUnique(new FakeUrlLoadingBrowserAgent(browser)));
}

FakeUrlLoadingBrowserAgent*
FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
    UrlLoadingBrowserAgent* agent) {
  return static_cast<FakeUrlLoadingBrowserAgent*>(agent);
}

FakeUrlLoadingBrowserAgent::FakeUrlLoadingBrowserAgent(Browser* browser)
    : UrlLoadingBrowserAgent(browser) {}

void FakeUrlLoadingBrowserAgent::LoadUrlInCurrentTab(
    const UrlLoadParams& params) {
  last_params = params;
  load_current_tab_call_count++;
}

void FakeUrlLoadingBrowserAgent::LoadUrlInNewTab(const UrlLoadParams& params) {
  last_params = params;
  load_new_tab_call_count++;
}

void FakeUrlLoadingBrowserAgent::SwitchToTab(const UrlLoadParams& params) {
  last_params = params;
  switch_tab_call_count++;
}

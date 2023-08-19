// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/synced_sessions_util.h"

#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"

void OpenDistantTabInBackground(const synced_sessions::DistantTab* tab,
                                bool in_incognito,
                                UrlLoadingBrowserAgent* url_loader,
                                UrlLoadStrategy load_strategy) {
  UrlLoadParams params = UrlLoadParams::InNewTab(tab->virtual_url);
  params.SetInBackground(YES);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.load_strategy = load_strategy;
  params.in_incognito = in_incognito;
  url_loader->Load(params);
}

void OpenDistantSessionInBackground(
    const synced_sessions::DistantSession* session,
    bool in_incognito,
    UrlLoadingBrowserAgent* url_loader,
    UrlLoadStrategy load_strategy) {
  for (auto const& tab : session->tabs) {
    OpenDistantTabInBackground(tab.get(), in_incognito, url_loader,
                               load_strategy);
  }
}

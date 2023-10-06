// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"

#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ui/base/device_form_factor.h"

int GetDefaultNumberOfTabsToLoadSimultaneously() {
  return ui::GetDeviceFormFactor() ==
                 ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE
             ? 6
             : 20;
}

void OpenDistantTab(const synced_sessions::DistantTab* tab,
                    bool in_incognito,
                    bool instant_load,
                    UrlLoadingBrowserAgent* url_loader,
                    UrlLoadStrategy load_strategy) {
  UrlLoadParams params = UrlLoadParams::InNewTab(tab->virtual_url);
  params.SetInBackground(YES);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.load_strategy = load_strategy;
  params.in_incognito = in_incognito;
  params.instant_load = instant_load;
  params.placeholder_title = tab->title;
  url_loader->Load(params);
}

void OpenDistantSessionInBackground(
    const synced_sessions::DistantSession* session,
    bool in_incognito,
    int maximum_instant_load_tabs,
    UrlLoadingBrowserAgent* url_loader,
    UrlLoadStrategy first_tab_load_strategy) {
  const int tab_count = static_cast<int>(session->tabs.size());
  for (int i = 0; i < tab_count; i++) {
    OpenDistantTab(session->tabs[i].get(), in_incognito,
                   /*instant_load=*/i < maximum_instant_load_tabs, url_loader,
                   i == 0 ? first_tab_load_strategy : UrlLoadStrategy::NORMAL);
  }
}

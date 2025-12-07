// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/model/inactive_tabs/inactive_tabs_service.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/utils.h"

InactiveTabsService::InactiveTabsService(PrefService* pref_service,
                                         BrowserList* browser_list)
    : pref_service_(pref_service), browser_list_(browser_list) {
  DCHECK(pref_service_);
  DCHECK(browser_list_);

  // Register for prefs::kInactiveTabsTimeThreshold.
  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback inactive_tabs_pref_callback =
      base::BindRepeating(&InactiveTabsService::OnPrefChanged,
                          weak_factory_.GetWeakPtr());
  pref_change_registrar_.Add(prefs::kInactiveTabsTimeThreshold,
                             inactive_tabs_pref_callback);
}

InactiveTabsService::~InactiveTabsService() = default;

void InactiveTabsService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  pref_service_ = nullptr;
}

void InactiveTabsService::OnPrefChanged(const std::string& name) {
  DCHECK_EQ(name, prefs::kInactiveTabsTimeThreshold);

  // Update all regular browsers.
  auto regular_browsers =
      browser_list_->BrowsersOfType(BrowserList::BrowserType::kRegular);
  for (auto* regular_browser : regular_browsers) {
    CHECK(regular_browser);
    CHECK(!regular_browser->IsInactive());
    Browser* inactive_browser = regular_browser->GetInactiveBrowser();
    CHECK(inactive_browser);

    if (IsInactiveTabsExplicitlyDisabledByUser(pref_service_)) {
      RestoreAllInactiveTabs(inactive_browser, regular_browser);
    } else {
      MoveTabsFromInactiveToActive(inactive_browser, regular_browser);
      MoveTabsFromActiveToInactive(regular_browser, inactive_browser);
    }
  }
}

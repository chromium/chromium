// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace ios {

// static
HostContentSettingsMap* HostContentSettingsMapFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<HostContentSettingsMap*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get());
}

// static
HostContentSettingsMapFactory* HostContentSettingsMapFactory::GetInstance() {
  static base::NoDestructor<HostContentSettingsMapFactory> instance;
  return instance.get();
}

HostContentSettingsMapFactory::HostContentSettingsMapFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "HostContentSettingsMap",
          BrowserStateDependencyManager::GetInstance()) {}

HostContentSettingsMapFactory::~HostContentSettingsMapFactory() {}

scoped_refptr<RefcountedKeyedService>
HostContentSettingsMapFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  if (browser_state->IsOffTheRecord()) {
    // If off-the-record, retrieve the host content settings map of the parent
    // browser state to ensure the preferences have been migrated.
    GetForBrowserState(browser_state->GetOriginalChromeBrowserState());
  }
  return base::MakeRefCounted<HostContentSettingsMap>(
      browser_state->GetPrefs(), browser_state->IsOffTheRecord(),
      false /* store_last_modified */,
      false /* migrate_requesting_and_top_level_origin_settings */);
}

web::BrowserState* HostContentSettingsMapFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

}  // namespace ios

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace ios {

// static
HostContentSettingsMap* HostContentSettingsMapFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
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
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  if (browser_state->IsOffTheRecord()) {
    // If off-the-record, retrieve the host content settings map of the parent
    // browser state to ensure the preferences have been migrated.
    GetForBrowserState(browser_state->GetOriginalChromeBrowserState());
  }

  // TODO(crbug.com/1081711): Set restore_session to whether or not the phone
  // has been reset, which would mirror iOS's cookie store.
  const bool is_off_the_record = browser_state->IsOffTheRecord();
  const bool should_record_metrics = !is_off_the_record;
  return base::MakeRefCounted<HostContentSettingsMap>(
      browser_state->GetPrefs(), is_off_the_record,
      false /* store_last_modified */, false /*restore_session*/,
      should_record_metrics);
}

web::BrowserState* HostContentSettingsMapFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

}  // namespace ios

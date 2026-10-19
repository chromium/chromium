// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_browser_agent.h"

#import "base/strings/string_number_conversions.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Returns the unique string identifier for `web_state`.
std::string GetWebStateId(web::WebState* web_state) {
  CHECK(web_state);
  return base::NumberToString(web_state->GetUniqueIdentifier().identifier());
}

// Synchronizes the watermark text for `web_state` with the profile's
// `kDataProtectionWatermarkedTabs` preference dictionary. If `watermark_text`
// is empty, the tab's entry is removed.
void UpdateWatermarkInPrefs(web::WebState* web_state,
                            const std::string_view& watermark_text) {
  CHECK(web_state);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  if (!profile->GetPrefs()) {
    return;
  }

  ScopedDictPrefUpdate update(
      profile->GetPrefs(),
      enterprise_data_protection::kDataProtectionWatermarkedTabs);
  if (watermark_text.empty()) {
    update->Remove(GetWebStateId(web_state));
  } else {
    update->Set(GetWebStateId(web_state), watermark_text);
  }
}

}  // namespace

DataProtectionBrowserAgent::DataProtectionBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser);
}

DataProtectionBrowserAgent::~DataProtectionBrowserAgent() {
  StopObserving();
}

#pragma mark - TabsDependencyInstaller

void DataProtectionBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  observers_.AddObservation(DataProtectionTabHelper::FromWebState(web_state));
}

void DataProtectionBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  observers_.RemoveObservation(
      DataProtectionTabHelper::FromWebState(web_state));
}

void DataProtectionBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  UpdateWatermarkInPrefs(web_state, "");
}

void DataProtectionBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // No-op: active tab changes do not affect watermark preference mappings.
}

#pragma mark - DataProtectionTabHelperObserver
void DataProtectionBrowserAgent::WatermarkTextDidChange(
    web::WebState* web_state,
    const std::string& watermark_text) {
  UpdateWatermarkInPrefs(web_state, watermark_text);
}

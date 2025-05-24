// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/browser_policy_new_tab_page_rewriter.h"

#import "components/prefs/pref_service.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/browser_state.h"

bool WillHandleWebBrowserNewTabPageURLForPolicy(
    GURL* url,
    web::BrowserState* browser_state) {
  // Don't change the URL when incognito mode.
  if (browser_state->IsOffTheRecord()) {
    return false;
  }

  // Extract value of kNewTabPageLocationOverride.
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  PrefService* prefs = profile->GetPrefs();
  std::string new_tab_page_location_override =
      prefs->GetString(prefs::kNewTabPageLocationOverride);

  if (url->host() == kChromeUINewTabHost && url->SchemeIs(kChromeUIScheme) &&
      !new_tab_page_location_override.empty()) {
    GURL new_url = GURL(new_tab_page_location_override);
    if (new_url.is_valid() && new_url != *url) {
      // Overwrite the original URL with the new URL if it is valid.
      *url = new_url;
      return true;
    } else {
      return false;
    }
  }
  return false;
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp/browser_policy_new_tab_page_rewriter.h"

#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool WillHandleWebBrowserNewTabPageURLForPolicy(
    GURL* url,
    web::BrowserState* browser_state) {
  // Don't change the URL when incognito mode.
  if (browser_state->IsOffTheRecord()) {
    return false;
  }

  // Extract value of kNewTabPageLocationOverride.
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  PrefService* prefs = chrome_browser_state->GetPrefs();
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

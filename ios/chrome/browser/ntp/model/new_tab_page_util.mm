// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"

#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

bool IsURLNewTabPage(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() == kChromeUINewTabURL;
}

bool IsVisibleURLNewTabPage(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }

  // On construction, NewTabPageTabHelper::IsActive() is initialized based
  // on the visible URL, so for unrealized WebState, check whether the URL
  // corresponds to an NTP URL.
  if (!web_state->IsRealized()) {
    return IsUrlNtp(web_state->GetVisibleURL());
  }

  NewTabPageTabHelper* ntp_helper =
      NewTabPageTabHelper::FromWebState(web_state);
  return ntp_helper && ntp_helper->IsActive();
}

bool IsNTPWithoutHistory(web::WebState* web_state) {
  return IsVisibleURLNewTabPage(web_state) &&
         web_state->GetNavigationManager() &&
         !web_state->GetNavigationManager()->CanGoBack() &&
         !web_state->GetNavigationManager()->CanGoForward();
}

void InjectNTP(Browser* browser) {
  // Don't inject an NTP for an empty web state list.
  if (!browser->GetWebStateList()->count()) {
    return;
  }

  // Don't inject an NTP on an NTP.
  web::WebState* active_web_state =
      browser->GetWebStateList()->GetActiveWebState();
  if (IsUrlNtp(active_web_state->GetVisibleURL())) {
    return;
  }

  // Inject a live NTP.
  ProfileIOS* profile = browser->GetProfile();
  web::WebState::CreateParams create_params(profile);
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(create_params);
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  std::unique_ptr<web::NavigationItem> item(web::NavigationItem::Create());
  item->SetURL(GURL(kChromeUINewTabURL));
  items.push_back(std::move(item));
  web_state->GetNavigationManager()->Restore(0, std::move(items));
  if (!profile->IsOffTheRecord()) {
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    NewTabPageTabHelper::FromWebState(web_state.get())
        ->SetShowStartSurface(true);
  }
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
}

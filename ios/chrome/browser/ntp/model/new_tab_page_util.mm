// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"

#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
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

bool ShouldHideFeedWithSearchChoice(TemplateURLService* template_url_service) {
  return !search::DefaultSearchProviderIsGoogle(template_url_service) &&
         template_url_service->IsEeaChoiceCountry();
}

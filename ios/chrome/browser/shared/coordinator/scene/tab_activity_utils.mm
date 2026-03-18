// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/tab_activity_utils.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/web/public/web_state_id.h"

BOOL IsTabActivityValid(NSUserActivity* activity, ProfileIOS* profile) {
  web::WebStateID tabID = GetTabIDFromActivity(activity);

  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  const BrowserList::BrowserType browser_types =
      profile->IsOffTheRecord() ? BrowserList::BrowserType::kIncognito
                                : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browserList->BrowsersOfType(browser_types);

  BrowserAndIndex tabInfo = FindBrowserAndIndex(tabID, browsers);

  return tabInfo.tab_index != WebStateList::kInvalidIndex;
}

void HandleTabMoveActivity(NSUserActivity* activity, Browser* browser) {
  DCHECK(ActivityIsTabMove(activity));
  BOOL incognito = GetIncognitoFromTabMoveActivity(activity);
  web::WebStateID tabID = GetTabIDFromActivity(activity);

  // It's expected that the current interface matches `incognito`.
  DCHECK(browser->GetProfile()->IsOffTheRecord() == incognito);

  // Move the tab to the current interface's browser.
  MoveTabToBrowser(tabID, browser, /*destination_tab_index=*/0);
}

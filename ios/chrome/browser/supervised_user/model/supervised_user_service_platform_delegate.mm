// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

SupervisedUserServicePlatformDelegate::SupervisedUserServicePlatformDelegate(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

void SupervisedUserServicePlatformDelegate::CloseIncognitoTabs() {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state_);
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito)) {
    CloseAllWebStates(*browser->GetWebStateList(),
                      WebStateList::CLOSE_USER_ACTION);
  }
}

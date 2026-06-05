// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"

#import <set>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace actor {

BrowserAndIndex FindBrowserAndIndexFromProfile(ProfileIOS* profile,
                                               web::WebStateID target_id,
                                               bool include_incognito) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  std::set<Browser*> browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
  if (include_incognito) {
    const std::set<Browser*>& incognito_browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito);
    browsers.insert(incognito_browsers.begin(), incognito_browsers.end());
  }
  return FindBrowserAndIndex(target_id, browsers);
}

}  // namespace actor

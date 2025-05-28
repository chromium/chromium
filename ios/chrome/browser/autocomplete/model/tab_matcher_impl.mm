// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/tab_matcher_impl.h"

#import <set>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

TabMatcherImpl::TabMatcherImpl(ProfileIOS* profile) : profile_{profile} {
  DCHECK(profile);
}

bool TabMatcherImpl::IsTabOpenWithURL(const GURL& url,
                                      const AutocompleteInput* input) const {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  const BrowserList::BrowserType browser_types =
      profile_->IsOffTheRecord()
          ? BrowserList::BrowserType::kIncognito
          : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);
  for (Browser* browser : browsers) {
    if (browser->GetWebStateList()->GetIndexOfInactiveWebStateWithURL(url) !=
        WebStateList::kInvalidIndex) {
      return true;
    }
  }
  return false;
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/tab_url_provider_impl.h"

#import "base/time/clock.h"
#import "base/time/time.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabUrlProviderImpl::TabUrlProviderImpl(ChromeBrowserState* browser_state,
                                       base::Clock* clock)
    : browser_list_(BrowserListFactory::GetForBrowserState(browser_state)),
      clock_(clock) {}

TabUrlProviderImpl::~TabUrlProviderImpl() = default;

const std::vector<GURL> TabUrlProviderImpl::GetUrlsOfActiveTabs(
    const base::TimeDelta& duration_since_last_shown) {
  if (!browser_list_)
    return std::vector<GURL>();

  // Get all URLs from regular tabs.
  std::map<base::Time, GURL> urls;
  for (const auto* browser : browser_list_->AllRegularBrowsers()) {
    WebStateList* web_state_list = browser->GetWebStateList();
    DCHECK(web_state_list);
    for (int i = 0; i < web_state_list->count(); ++i) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      DCHECK(web_state);
      web::NavigationItem* navigation_item =
          web_state->GetNavigationManager()->GetLastCommittedItem();
      if (!navigation_item)
        continue;

      // Fallback to use last commit navigation timestamp since iOS web state
      // doesn't provide last active timestamp.
      // TODO(crbug.com/1238043): Use WebState::GetLastActiveTime() as
      // timestamp.
      if (navigation_item->GetTimestamp().is_null() ||
          clock_->Now() - navigation_item->GetTimestamp() >
              duration_since_last_shown) {
        continue;
      }

      urls.emplace(navigation_item->GetTimestamp(),
                   navigation_item->GetVirtualURL());
    }
  }

  // Output the URLs from sorted map in desending order.
  std::vector<GURL> res;
  for (auto it = urls.rbegin(); it != urls.rend(); ++it)
    res.push_back(it->second);

  return res;
}
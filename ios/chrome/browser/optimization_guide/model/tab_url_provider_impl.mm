// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/tab_url_provider_impl.h"

#import "base/containers/adapters.h"
#import "base/time/clock.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

TabUrlProviderImpl::TabUrlProviderImpl(BrowserList* browser_list,
                                       base::Clock* clock)
    : browser_list_(browser_list), clock_(clock) {}

TabUrlProviderImpl::~TabUrlProviderImpl() = default;

const std::vector<GURL> TabUrlProviderImpl::GetUrlsOfActiveTabs(
    const base::TimeDelta& duration_since_last_shown) {
  if (!browser_list_)
    return std::vector<GURL>();

  // Get all URLs from regular tabs.
  std::map<base::Time, GURL> urls;
  for (Browser* browser : browser_list_->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* web_state_list = browser->GetWebStateList();
    DCHECK(web_state_list);
    for (int i = 0; i < web_state_list->count(); ++i) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      DCHECK(web_state);

      const base::Time last_active_time = web_state->GetLastActiveTime();
      if (last_active_time.is_null() ||
          clock_->Now() - last_active_time > duration_since_last_shown) {
        continue;
      }

      urls.emplace(last_active_time, web_state->GetLastCommittedURL());
    }
  }

  // Output the URLs from sorted map in desending order.
  std::vector<GURL> res;
  for (const auto& [navigation_time, url] : base::Reversed(urls))
    res.push_back(url);

  return res;
}

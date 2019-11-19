// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/browsing_data_test_util.h"

#import <WebKit/WebKit.h>

#include "base/task/post_task.h"
#import "base/test/ios/wait_util.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

bool ClearBrowsingData(bool off_the_record, BrowsingDataRemoveMask mask) {
  ios::ChromeBrowserState* browser_state =
      off_the_record ? chrome_test_util::GetCurrentIncognitoBrowserState()
                     : chrome_test_util::GetOriginalBrowserState();

  __block bool did_complete = false;
  [chrome_test_util::GetMainController().sceneController
      removeBrowsingDataForBrowserState:browser_state
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:mask
                        completionBlock:^{
                          did_complete = true;
                        }];
  return WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForClearBrowsingDataTimeout, ^{
        return did_complete;
      });
}
}  // namespace

namespace chrome_test_util {

bool RemoveBrowsingCache() {
  return ClearBrowsingData(/*off_the_record=*/false,
                           BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE);
}

bool ClearBrowsingHistory() {
  return ClearBrowsingData(/*off_the_record=*/false,
                           BrowsingDataRemoveMask::REMOVE_HISTORY);
}

bool ClearAllBrowsingData(bool off_the_record) {
  return ClearBrowsingData(off_the_record, BrowsingDataRemoveMask::REMOVE_ALL);
}

bool ClearAllWebStateBrowsingData() {
  __block bool callback_finished = false;
  [[WKWebsiteDataStore defaultDataStore]
      removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes]
          modifiedSince:[NSDate distantPast]
      completionHandler:^{
        web::BrowserState* browser_state =
            chrome_test_util::GetOriginalBrowserState();
        web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
            .Purge();
        callback_finished = true;
      }];
  return WaitUntilConditionOrTimeout(20, ^{
    return callback_finished;
  });
}

bool ClearCertificatePolicyCache(bool off_the_record) {
  ios::ChromeBrowserState* browser_state =
      off_the_record ? GetCurrentIncognitoBrowserState()
                     : GetOriginalBrowserState();
  auto cache = web::BrowserState::GetCertificatePolicyCache(browser_state);
  __block BOOL policies_cleared = NO;
  base::PostTask(FROM_HERE, {web::WebThread::IO}, base::BindOnce(^{
                   cache->ClearCertificatePolicies();
                   policies_cleared = YES;
                 }));
  return WaitUntilConditionOrTimeout(2, ^{
    return policies_cleared;
  });
}

}  // namespace chrome_test_util

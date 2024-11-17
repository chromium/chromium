// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/browsing_data_test_util.h"

#import <WebKit/WebKit.h>

#import "base/logging.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/test/ios/wait_util.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/browser_state_utils.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

bool ClearBrowsingData(bool off_the_record, BrowsingDataRemoveMask mask) {
  ProfileIOS* profile = off_the_record
                            ? chrome_test_util::GetCurrentIncognitoProfile()
                            : chrome_test_util::GetOriginalProfile();

  __block bool did_complete = false;
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForProfile(profile);
  browsingDataRemover->Remove(browsing_data::TimePeriod::ALL_TIME, mask,
                              base::BindOnce(^{
                                did_complete = true;
                              }));

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

bool ClearCookiesAndSiteData() {
  return ClearBrowsingData(/*off_the_record=*/false,
                           BrowsingDataRemoveMask::REMOVE_SITE_DATA);
}

bool ClearAllWebStateBrowsingData() {
  __block bool callback_finished = false;
  WKWebsiteDataStore* data_store =
      web::GetDataStoreForBrowserState(GetOriginalProfile());
  [data_store removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes]
                  modifiedSince:[NSDate distantPast]
              completionHandler:^{
                callback_finished = true;
              }];
  return WaitUntilConditionOrTimeout(base::Seconds(20), ^{
    return callback_finished;
  });
}

bool ClearCertificatePolicyCache(bool off_the_record) {
  ProfileIOS* profile =
      off_the_record ? GetCurrentIncognitoProfile() : GetOriginalProfile();
  scoped_refptr<web::CertificatePolicyCache> cache =
      web::BrowserState::GetCertificatePolicyCache(profile);
  __block BOOL policies_cleared = NO;
  web::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                             cache->ClearCertificatePolicies();
                                             policies_cleared = YES;
                                           }));
  return WaitUntilConditionOrTimeout(base::Seconds(2), ^{
    return policies_cleared;
  });
}

int GetBrowsingHistoryEntryCount(NSError** error) {
  // Call the history service.
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  __block bool history_service_callback_called = false;
  __block int count = -1;
  base::CancelableTaskTracker task_tracker;
  history_service->GetHistoryCount(
      base::Time::Min(), base::Time::Max(),
      base::BindOnce(^(history::HistoryCountResult result) {
        if (result.success) {
          count = result.count;
        }
        history_service_callback_called = true;
      }),
      &task_tracker);

  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:4.0];
  while (!history_service_callback_called &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(base::Seconds(0.1));
  }

  NSString* error_message = nil;
  if (!history_service_callback_called) {
    error_message = @"History::GetHistoryCount callback never called, app will "
                     "probably crash later.";
  } else if (count == -1) {
    error_message = @"History::GetHistoryCount did not succeed.";
  }

  if (error_message != nil && error != nil) {
    NSDictionary* error_info = @{NSLocalizedDescriptionKey : error_message};
    *error = [NSError errorWithDomain:@"BrowsingDataTestDomain"
                                 code:0
                             userInfo:error_info];
    LOG(ERROR) << "Querying history service failed: " << error_message;
    return -1;
  }
  return count;
}

}  // namespace chrome_test_util

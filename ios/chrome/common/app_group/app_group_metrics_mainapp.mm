// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_metrics_mainapp.h"

#import <stdint.h>

#import "base/metrics/histogram_functions.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"

namespace {
// Delay in seconds before reporting the metrics coming from Open extensions.
// This delay is needed to be sure the shared user default
// is correctly synchronized.
const int kDispatchTimeInSeconds = 2;

// Maximum number of outcomes reported to UMA to avoid infinite loops.
const int kMaxNumberOfLogs = 10;

}  // namespace
namespace app_group {

namespace main_app {

void ProcessPendingLogs(ProceduralBlockWithData callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSURL* store_url = [file_manager
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* log_dir_url =
      [store_url URLByAppendingPathComponent:app_group::kPendingLogFileDirectory
                                 isDirectory:YES];

  NSArray* pending_logs =
      [file_manager contentsOfDirectoryAtPath:[log_dir_url path] error:nil];
  if (!pending_logs)
    return;
  for (NSString* pending_log : pending_logs) {
    if ([pending_log hasSuffix:app_group::kPendingLogFileSuffix]) {
      NSURL* file_url =
          [log_dir_url URLByAppendingPathComponent:pending_log isDirectory:NO];
      if (callback) {
        NSData* log_content = [file_manager contentsAtPath:[file_url path]];
        callback(log_content);
      }
      [file_manager removeItemAtURL:file_url error:nil];
    }
  }
}

void EnableMetrics(NSString* client_id,
                   NSString* brand_code,
                   int64_t install_date,
                   int64_t enable_metrics_date) {
  NSUserDefaults* shared_defaults = GetGroupUserDefaults();
  [shared_defaults setObject:client_id forKey:@(kChromeAppClientID)];

  [shared_defaults
      setObject:[NSString stringWithFormat:@"%lld", enable_metrics_date]
         forKey:@(kUserMetricsEnabledDate)];

  [shared_defaults setObject:[NSString stringWithFormat:@"%lld", install_date]
                      forKey:@(kInstallDate)];

  [shared_defaults setObject:brand_code forKey:@(kBrandCode)];

  // Reporting the Open extension should be done only once on each session.
  // When the open extension actually launch Chrome, it stores an Outcome entry
  // in the shared NSUserDefaults. Add a delay to let the synchronization happen
  // and be sure this entry is reported.
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      (int64_t)(kDispatchTimeInSeconds * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          LogOpenExtensionMetrics();
        });
  });
}

void DisableMetrics() {
  NSUserDefaults* shared_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()];
  [shared_defaults removeObjectForKey:@(kChromeAppClientID)];
  [shared_defaults removeObjectForKey:kContentExtensionDisplayCount];
  [shared_defaults removeObjectForKey:kSearchExtensionDisplayCount];
  [shared_defaults removeObjectForKey:kOpenExtensionOutcomes];
}

void LogOpenExtensionMetrics() {
  NSUserDefaults* shared_defaults = GetGroupUserDefaults();
  NSDictionary<NSString*, NSNumber*>* open_extension_dictionary =
      [shared_defaults dictionaryForKey:app_group::kOpenExtensionOutcomes];
  // Clear the outcomes after reporting
  [shared_defaults removeObjectForKey:kOpenExtensionOutcomes];

  for (NSString* key in open_extension_dictionary) {
    int event_count = [open_extension_dictionary valueForKey:key].intValue;
    app_group::OpenExtensionOutcome bucket_for_histogram =
        OutcomeTypeFromKey(key);

    if (event_count > kMaxNumberOfLogs) {
      event_count = kMaxNumberOfLogs;
    }

    for (int i = 0; i < event_count; i++) {
      base::UmaHistogramEnumeration("IOSOpenExtensionOutcome",
                                    bucket_for_histogram);
    }
  }
}

}  // namespace main_app

}  // namespace app_group

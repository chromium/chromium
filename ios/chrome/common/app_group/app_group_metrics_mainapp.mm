// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/app_group/app_group_metrics_mainapp.h"

#include <stdint.h>

#include "base/metrics/histogram_functions.h"
#include "base/threading/scoped_blocking_call.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace app_group {

namespace main_app {

void RecordWidgetUsage() {
  NSUserDefaults* shared_defaults = GetGroupUserDefaults();
  int content_extension_count =
      [shared_defaults integerForKey:kContentExtensionDisplayCount];
  base::UmaHistogramCounts1000("IOS.ContentExtension.DisplayCount",
                               content_extension_count);
  [shared_defaults setInteger:0 forKey:kContentExtensionDisplayCount];
  int search_extension_count =
      [shared_defaults integerForKey:kSearchExtensionDisplayCount];
  base::UmaHistogramCounts1000("IOS.SearchExtension.DisplayCount",
                               search_extension_count);
  [shared_defaults setInteger:0 forKey:kSearchExtensionDisplayCount];

  int credential_extension_count =
      [shared_defaults integerForKey:kCredentialExtensionDisplayCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.DisplayCount",
                               credential_extension_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionDisplayCount];
  int credential_extension_reauth_count =
      [shared_defaults integerForKey:kCredentialExtensionReauthCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.ReauthCount",
                               credential_extension_reauth_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionReauthCount];
  int credential_extension_copy_url_count =
      [shared_defaults integerForKey:kCredentialExtensionCopyURLCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.CopyURLCount",
                               credential_extension_copy_url_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionCopyURLCount];
  int credential_extension_copy_username_count =
      [shared_defaults integerForKey:kCredentialExtensionCopyUsernameCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.CopyUsernameCount",
                               credential_extension_copy_username_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionCopyUsernameCount];
  int credential_extension_copy_password_count =
      [shared_defaults integerForKey:kCredentialExtensionCopyPasswordCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.CopyPasswordCount",
                               credential_extension_copy_password_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionCopyPasswordCount];
  int credential_extension_show_password_count =
      [shared_defaults integerForKey:kCredentialExtensionShowPasswordCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.ShowPasswordCount",
                               credential_extension_show_password_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionShowPasswordCount];
  int credential_extension_search_count =
      [shared_defaults integerForKey:kCredentialExtensionSearchCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.SearchCount",
                               credential_extension_search_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionSearchCount];
  int credential_extension_password_use_count =
      [shared_defaults integerForKey:kCredentialExtensionPasswordUseCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.PasswordUseCount",
                               credential_extension_password_use_count);
  [shared_defaults setInteger:0 forKey:kCredentialExtensionPasswordUseCount];
  int credential_extension_quick_password_use_count =
      [shared_defaults integerForKey:kCredentialExtensionQuickPasswordUseCount];
  base::UmaHistogramCounts1000("IOS.CredentialExtension.QuickPasswordUseCount",
                               credential_extension_quick_password_use_count);
  [shared_defaults setInteger:0
                       forKey:kCredentialExtensionQuickPasswordUseCount];
  int credential_extension_fetch_password_failure_count = [shared_defaults
      integerForKey:kCredentialExtensionFetchPasswordFailureCount];
  base::UmaHistogramCounts1000(
      "IOS.CredentialExtension.FetchPasswordFailure",
      credential_extension_fetch_password_failure_count);
  [shared_defaults setInteger:0
                       forKey:kCredentialExtensionFetchPasswordFailureCount];
  int credential_extension_fetch_password_nil_argument_count = [shared_defaults
      integerForKey:kCredentialExtensionFetchPasswordNilArgumentCount];
  base::UmaHistogramCounts1000(
      "IOS.CredentialExtension.FetchPasswordNilArgument",
      credential_extension_fetch_password_nil_argument_count);
  [shared_defaults
      setInteger:0
          forKey:kCredentialExtensionFetchPasswordNilArgumentCount];
}

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
}

void DisableMetrics() {
  NSUserDefaults* shared_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()];
  [shared_defaults removeObjectForKey:@(kChromeAppClientID)];
  [shared_defaults removeObjectForKey:kContentExtensionDisplayCount];
  [shared_defaults removeObjectForKey:kSearchExtensionDisplayCount];
}

}  // namespace main_app

}  // namespace app_group

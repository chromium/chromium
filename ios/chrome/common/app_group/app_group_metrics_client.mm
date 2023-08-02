// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_metrics_client.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"

namespace {

// Number of log files to keep in the `kPendingLogFileDirectory`. Any older file
// may be deleted.
const int kMaxFileNumber = 100;

}  // namespace

namespace app_group {
namespace client_app {

void AddPendingLog(NSData* log, AppGroupApplications application) {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSURL* store_url = [file_manager
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];

  NSURL* log_dir_url =
      [store_url URLByAppendingPathComponent:app_group::kPendingLogFileDirectory
                                 isDirectory:YES];
  [file_manager createDirectoryAtURL:log_dir_url
         withIntermediateDirectories:YES
                          attributes:nil
                               error:nil];

  // File name are formated using creationtimestamp_extensionname_PendingLog
  // (e.g: 123456789_TodayExtension_PendingLog).
  NSString* file_name = [NSString
      stringWithFormat:@"%ld_%@%@",
                       static_cast<long>([[NSDate date] timeIntervalSince1970]),
                       ApplicationName(application),
                       app_group::kPendingLogFileSuffix];
  NSURL* ready_log_url =
      [log_dir_url URLByAppendingPathComponent:file_name isDirectory:NO];

  [file_manager createFileAtPath:[ready_log_url path]
                        contents:log
                      attributes:nil];
}

void CleanOldPendingLogs() {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSURL* store_url = [file_manager
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* log_dir_url =
      [store_url URLByAppendingPathComponent:app_group::kPendingLogFileDirectory
                                 isDirectory:YES];

  NSArray* pending_logs =
      [file_manager contentsOfDirectoryAtPath:[log_dir_url path] error:nil];

  if (kMaxFileNumber >= [pending_logs count])
    return;
  // sort by creation date
  NSMutableArray* files_and_properties =
      [NSMutableArray arrayWithCapacity:[pending_logs count]];
  for (NSString* file : pending_logs) {
    if (![file hasSuffix:app_group::kPendingLogFileSuffix])
      continue;
    NSURL* file_url =
        [log_dir_url URLByAppendingPathComponent:file isDirectory:NO];

    NSDictionary* properties =
        [file_manager attributesOfItemAtPath:[file_url path] error:nil];
    NSDate* mod_date = [properties objectForKey:NSFileModificationDate];

    [files_and_properties addObject:@{
      @"path" : file,
      @"lastModDate" : mod_date
    }];
  }

  // Sort files by modification date. Older files will be first.
  NSArray* sorted_files =
      [files_and_properties sortedArrayUsingComparator:^(id path1, id path2) {
        return [[path1 objectForKey:@"lastModDate"]
            compare:[path2 objectForKey:@"lastModDate"]];
      }];
  if (kMaxFileNumber >= [sorted_files count])
    return;
  NSUInteger first_file_to_keep = [sorted_files count] - kMaxFileNumber;
  for (NSUInteger file_index = 0; file_index < first_file_to_keep;
       file_index++) {
    NSString* path =
        [[sorted_files objectAtIndex:file_index] objectForKey:@"path"];
    NSURL* file_url =
        [log_dir_url URLByAppendingPathComponent:path isDirectory:NO];
    [file_manager removeItemAtURL:file_url error:nil];
  }
}

}  // namespace client_app
}  // namespace app_group

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_util_internal.h"

#import <UIKit/UIKit.h>

#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/browser/paths/paths.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum values for Signin.IOSDeviceRestoreSentinelError histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class SigninIOSDeviceRestoreSentinelError : int {
  // No error to create the sentinel file.
  kNoError = 0,
  // Failed to create the sentinel file.
  kSentinelFileCreationFailed = 1,
  // Failed to set ExcludeFromBackupFlag to sentinel file.
  kExcludedFromBackupFlagFailed = 2,
  kMaxValue = kExcludedFromBackupFlagFailed,
};

// Records Signin.IOSDeviceRestoreSentinelError histogram.
void RecordSentinelErrorHistogram(SigninIOSDeviceRestoreSentinelError error) {
  base::UmaHistogramEnumeration("Signin.IOSDeviceRestoreSentinelError", error);
}

// Creates a sentinel file asynchronously, and set ExcludeFromBackupFlag
// according to `exclude_from_backup`.
void CreateSentinelFileAsync(const base::FilePath sentinel_path,
                             bool exclude_from_backup) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
    NSFileManager* file_manager = [NSFileManager defaultManager];
    NSString* path_string = base::SysUTF8ToNSString(sentinel_path.value());
    BOOL create_success = [file_manager createFileAtPath:path_string
                                                contents:nil
                                              attributes:nil];
    if (!create_success) {
      RecordSentinelErrorHistogram(
          SigninIOSDeviceRestoreSentinelError::kSentinelFileCreationFailed);
      return;
    }
    if (!exclude_from_backup) {
      RecordSentinelErrorHistogram(
          SigninIOSDeviceRestoreSentinelError::kNoError);
      return;
    }
    NSURL* url = [NSURL fileURLWithPath:path_string];
    NSError* error = nil;
    [url setResourceValue:@(YES)
                   forKey:NSURLIsExcludedFromBackupKey
                    error:&error];
    DLOG_IF(ERROR, error != nil) << "Error setting excluded backup key, error: "
                                 << base::SysNSStringToUTF8(error.description);
    RecordSentinelErrorHistogram(
        (error == nil) ? SigninIOSDeviceRestoreSentinelError::kNoError
                       : SigninIOSDeviceRestoreSentinelError::
                             kExcludedFromBackupFlagFailed);
  });
}

}  // namespace

// File name for sentinel to backup in iOS backup device.
const base::FilePath::CharType kSentinelThatIsBackedUp[] =
    FILE_PATH_LITERAL("BackedUpSentinel");
// File name for sentinel to not backup in iOS backup device.
const base::FilePath::CharType kSentinelThatIsNotBackedUp[] =
    FILE_PATH_LITERAL("NotBackedUpSentinel");

// Computes the full path for a sentinel file with name `sentinel_name`.
// This method can return an emtpy string if failed.
base::FilePath PathForSentinel(const base::FilePath::CharType* sentinel_name) {
  base::FilePath user_data_path;
  if (!base::PathService::Get(ios::DIR_USER_DATA, &user_data_path)) {
    return base::FilePath();
  }
  return user_data_path.Append(sentinel_name);
}

signin::Tribool IsFirstSessionAfterDeviceRestoreInternal() {
  const base::FilePath backed_up_sentinel_path =
      PathForSentinel(kSentinelThatIsBackedUp);
  const base::FilePath not_backed_up_sentinel_path =
      PathForSentinel(kSentinelThatIsNotBackedUp);
  bool path_successfully_generated =
      !backed_up_sentinel_path.empty() && !not_backed_up_sentinel_path.empty();
  base::UmaHistogramBoolean("Signin.IOSDeviceRestoreSentinelPathGenerated",
                            path_successfully_generated);
  if (!path_successfully_generated) {
    return signin::Tribool::kUnknown;
  }
  bool does_backed_up_sentinel_file_exist =
      base::PathExists(backed_up_sentinel_path);
  bool does_not_backed_up_sentinel_file_exist =
      base::PathExists(not_backed_up_sentinel_path);
  if (!does_backed_up_sentinel_file_exist) {
    CreateSentinelFileAsync(backed_up_sentinel_path,
                            /* exclude_from_backup */ false);
  }
  if (!does_not_backed_up_sentinel_file_exist) {
    CreateSentinelFileAsync(not_backed_up_sentinel_path,
                            /* exclude_from_backup */ true);
  }
  if (does_backed_up_sentinel_file_exist) {
    return does_not_backed_up_sentinel_file_exist ? signin::Tribool::kFalse
                                                  : signin::Tribool::kTrue;
  }
  return signin::Tribool::kUnknown;
}

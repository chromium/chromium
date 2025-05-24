// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util_internal.h"

#import <UIKit/UIKit.h>

#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "base/version_info/channel.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/channel_info.h"

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

// Represents information about a sentinel file.
struct SentinelFileInfo {
  base::FilePath path;
  bool exclude_from_backup;
};

// Records Signin.IOSDeviceRestoreSentinelError histogram.
void RecordSentinelErrorHistogram(SigninIOSDeviceRestoreSentinelError error) {
  base::UmaHistogramEnumeration("Signin.IOSDeviceRestoreSentinelError", error);
}

// Creates a sentinel file synchronously, and set ExcludeFromBackupFlag
// according to `exclude_from_backup`. Returns true if it succeeds.
bool CreateSentinelFile(const base::FilePath sentinel_path,
                        bool exclude_from_backup) {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* path_string = base::SysUTF8ToNSString(sentinel_path.value());
  BOOL create_success = [file_manager createFileAtPath:path_string
                                              contents:nil
                                            attributes:nil];
  if (!create_success) {
    RecordSentinelErrorHistogram(
        SigninIOSDeviceRestoreSentinelError::kSentinelFileCreationFailed);
    return false;
  }
  if (!exclude_from_backup) {
    RecordSentinelErrorHistogram(SigninIOSDeviceRestoreSentinelError::kNoError);
    return true;
  }
  NSURL* url = [NSURL fileURLWithPath:path_string];
  NSError* error = nil;
  [url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:&error];

  if (error == nil) {
    RecordSentinelErrorHistogram(SigninIOSDeviceRestoreSentinelError::kNoError);
    return true;
  }

  RecordSentinelErrorHistogram(
      SigninIOSDeviceRestoreSentinelError::kExcludedFromBackupFlagFailed);

  DLOG(ERROR) << "Error setting excluded backup key, error: "
              << base::SysNSStringToUTF8(error.description);

  // Since the exclusion from backups failed, delete the file so the entire
  // process will be retried next time.
  [file_manager removeItemAtPath:path_string error:nil];
  return false;
}

// Creates the sentinel files for LoadDeviceRestoreDataInternal(...).
void CreateSentinelFiles(std::array<SentinelFileInfo, 2> infos) {
  for (const SentinelFileInfo& info : infos) {
    if (!base::PathExists(info.path) &&
        !CreateSentinelFile(info.path, info.exclude_from_backup)) {
      // If creating any of the sentinel files fail, stop and
      // do not try to create the other ones. This avoid returning
      // false positives.
      return;
    }
  }
}

// Whether a phone backup/restore state should be simulated.
// This can be triggered either by EG test flag or by Experimental settings.
bool ShouldSimulatePostDeviceRestore() {
  // We simulate post device restore if required either by experimental settings
  // or test flag.
  return tests_hook::SimulatePostDeviceRestore() ||
         experimental_flags::SimulatePostDeviceRestore();
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

signin::RestoreData LoadDeviceRestoreDataInternal(
    base::OnceClosure completion) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  const base::FilePath backed_up_sentinel_path =
      PathForSentinel(kSentinelThatIsBackedUp);
  const base::FilePath not_backed_up_sentinel_path =
      PathForSentinel(kSentinelThatIsNotBackedUp);
  bool path_successfully_generated =
      !backed_up_sentinel_path.empty() && !not_backed_up_sentinel_path.empty();
  base::UmaHistogramBoolean("Signin.IOSDeviceRestoreSentinelPathGenerated",
                            path_successfully_generated);
  if (!path_successfully_generated) {
    signin::RestoreData restore_data;
    restore_data.is_first_session_after_device_restore =
        signin::Tribool::kUnknown;
    return restore_data;
  }
  if (ShouldSimulatePostDeviceRestore()) {
    // This simulate a device restore. This setting is accessible only in the
    // experimental flags in Chrome settings. Therefore this should be avaible
    // only in canary and dev.
    auto current_channel = GetChannel();
    CHECK(current_channel != version_info::Channel::STABLE,
          base::NotFatalUntil::M140);
    DeleteFile(not_backed_up_sentinel_path);
  }
  bool does_backed_up_sentinel_file_exist =
      base::PathExists(backed_up_sentinel_path);
  bool does_not_backed_up_sentinel_file_exist =
      base::PathExists(not_backed_up_sentinel_path);

  // Create sentinel files, if they don't exist. The order is very specific:
  // 1) The not-backed-up file is created first (or already exists).
  // 2) The backed-up file is created second.
  //
  // If the process is somehow interrupted in the middle, or the first step
  // fails, the resulting state can be detected and, upon next invocation
  // (which in practice means upon next browser startup), it resumes normally
  // and meanwhile Tribool::kUnknown is returned.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::LOWEST},
      base::BindOnce(&CreateSentinelFiles,
                     std::array<SentinelFileInfo, 2>{
                         SentinelFileInfo{
                             .path = not_backed_up_sentinel_path,
                             .exclude_from_backup = true,
                         },
                         SentinelFileInfo{
                             .path = backed_up_sentinel_path,
                             .exclude_from_backup = false,
                         },
                     }),
      std::move(completion));

  signin::RestoreData restore_data;
  if (!does_backed_up_sentinel_file_exist) {
    restore_data.is_first_session_after_device_restore =
        signin::Tribool::kUnknown;
  } else if (!does_not_backed_up_sentinel_file_exist) {
    restore_data.is_first_session_after_device_restore = signin::Tribool::kTrue;
  } else {
    base::File::Info not_backed_up_sentinel_info;
    base::File::Info backed_up_sentinel_info;
    bool file_info_valid =
        GetFileInfo(not_backed_up_sentinel_path,
                    &not_backed_up_sentinel_info) &&
        GetFileInfo(backed_up_sentinel_path, &backed_up_sentinel_info);
    if (file_info_valid && (backed_up_sentinel_info.creation_time <
                            not_backed_up_sentinel_info.creation_time)) {
      // If the not backed up sentinel was created before the backed up sentinel
      // file, a device restore happened in a previous run.
      restore_data.last_restore_timestamp =
          not_backed_up_sentinel_info.creation_time;
    }
    restore_data.is_first_session_after_device_restore =
        signin::Tribool::kFalse;
  }
  return restore_data;
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/model/first_run.h"

#import <string_view>

#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/startup_metric_utils/browser/startup_metric_utils.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

namespace {

// The absence of kSentinelFile file will tell us it is a first run.
const base::FilePath::CharType kSentinelFile[] = FILE_PATH_LITERAL("First Run");

// RLZ ping delay pref name.
const char kPingDelayPrefName[] = "distribution.ping_delay";

// The info from first run sentinel file; if the file doesn't exist, the value
// would be `std::nullopt`. Only accessed through LoadSentinelInfo() and
// GetSentinelInfo();
std::optional<base::File::Info>& GetSentinelInfoGlobal() {
  static base::NoDestructor<std::optional<base::File::Info>> kInstance(
      std::nullopt);
  return *kInstance;
}

}  // namespace

FirstRun::FirstRunState FirstRun::first_run_ = FIRST_RUN_UNKNOWN;

// static
bool FirstRun::GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath first_run_sentinel;
  if (!base::PathService::Get(ios::DIR_USER_DATA, &first_run_sentinel)) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  *path = first_run_sentinel.Append(kSentinelFile);
  return true;
}

// static
bool FirstRun::IsChromeFirstRun() {
  if (first_run_ != FIRST_RUN_UNKNOWN)
    return first_run_ == FIRST_RUN_TRUE;

  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel) ||
      base::PathExists(first_run_sentinel)) {
    LoadSentinelInfo();
    first_run_ = FIRST_RUN_FALSE;
    return false;
  }
  first_run_ = FIRST_RUN_TRUE;
  return true;
}

// static
std::optional<base::File::Info> FirstRun::GetSentinelInfo() {
  return GetSentinelInfoGlobal();
}

// static
bool FirstRun::RemoveSentinel() {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return base::DeleteFile(first_run_sentinel);
}

// static
startup_metric_utils::FirstRunSentinelCreationResult FirstRun::CreateSentinel(
    base::File::Error* error) {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFailedToGetPath;
  if (base::PathExists(first_run_sentinel))
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFilePathExists;
  GetSentinelInfoGlobal() = std::nullopt;
  bool success = base::WriteFile(first_run_sentinel, std::string_view());
  if (error)
    *error = base::File::GetLastFileError();

  if (success) {
    LoadSentinelInfo();
  }
  return success
             ? startup_metric_utils::FirstRunSentinelCreationResult::kSuccess
             : startup_metric_utils::FirstRunSentinelCreationResult::
                   kFileSystemError;
}

// static
void FirstRun::LoadSentinelInfo() {
  std::optional<base::File::Info>& global_sentinel_info =
      GetSentinelInfoGlobal();
  if (global_sentinel_info.has_value()) {
    return;
  }
  base::FilePath first_run_sentinel;
  base::File::Info sentinel_info;
  if (GetFirstRunSentinelFilePath(&first_run_sentinel) &&
      base::PathExists(first_run_sentinel) &&
      base::GetFileInfo(first_run_sentinel, &sentinel_info)) {
    global_sentinel_info = sentinel_info;
  }
}

// static
const char* FirstRun::GetPingDelayPrefName() {
  return kPingDelayPrefName;
}

// static
void FirstRun::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(GetPingDelayPrefName(), 0);
}

// static
void FirstRun::ClearStateForTesting() {
  GetSentinelInfoGlobal() = std::nullopt;
  first_run_ = FIRST_RUN_UNKNOWN;
}

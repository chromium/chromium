// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/first_run.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/paths/paths.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The absence of kSentinelFile file will tell us it is a first run.
const base::FilePath::CharType kSentinelFile[] = FILE_PATH_LITERAL("First Run");

// RLZ ping delay pref name.
const char kPingDelayPrefName[] = "distribution.ping_delay";

}  // namespace

FirstRun::FirstRunState FirstRun::first_run_ = FIRST_RUN_UNKNOWN;

// static
bool FirstRun::GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath first_run_sentinel;
  if (!base::PathService::Get(ios::DIR_USER_DATA, &first_run_sentinel)) {
    NOTREACHED();
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
    first_run_ = FIRST_RUN_FALSE;
    return false;
  }
  first_run_ = FIRST_RUN_TRUE;
  return true;
}

// static
bool FirstRun::RemoveSentinel() {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return base::DeleteFile(first_run_sentinel);
}

// static
FirstRun::SentinelResult FirstRun::CreateSentinel(base::File::Error* error) {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return SENTINEL_RESULT_FAILED_TO_GET_PATH;
  if (base::PathExists(first_run_sentinel))
    return SENTINEL_RESULT_FILE_PATH_EXISTS;
  bool success = base::WriteFile(first_run_sentinel, "", 0) != -1;
  if (error)
    *error = base::File::GetLastFileError();
  return success ? SENTINEL_RESULT_SUCCESS : SENTINEL_RESULT_FILE_ERROR;
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

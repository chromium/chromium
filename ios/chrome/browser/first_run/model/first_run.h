// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the iOS equivalent of FirstRun.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_MODEL_FIRST_RUN_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_MODEL_FIRST_RUN_H_

#include <optional>

#include "base/files/file.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A collection of functions to manage the user experience when running
// this application for the first time. The iOS implementation is significantly
// simpler than other platforms because it is designed to be preemptive and
// stops user from doing anything else until the First Run UX is completed
// or explicitly skipped.
class FirstRun {
 public:
  FirstRun() = delete;
  FirstRun(const FirstRun&) = delete;
  FirstRun& operator=(const FirstRun&) = delete;

  // Clears the stored state so that a call to `IsChromeFirstRun()` will reload
  // the state. Any changes to the sentinel file need to be made prior to
  // calling this method, as this method does not remove or modify the sentinel.
  // To be used only for testing.
  static void ClearStateForTesting();

  // Returns true if this is the first time chrome is run for this user.
  static bool IsChromeFirstRun();

  // If the first run sentinel file exist, returns the info; otherwise, return
  // `std::nullopt`.
  static std::optional<base::File::Info> GetSentinelInfo();

  // Creates the sentinel file that signals that chrome has been configured if
  // the file does not exist yet.
  // Returns `startup_metric_utils::FirstRunSentinelCreationResult::kSuccess` if
  // the file was created. If
  // `startup_metric_utils::FirstRunSentinelCreationResult::kFileSystemError` is
  // returned, `error` is set to the file system error, if non-nil.
  static startup_metric_utils::FirstRunSentinelCreationResult CreateSentinel(
      base::File::Error* error);

  // Removes the sentinel file created in ConfigDone(). Returns false if the
  // sentinel file could not be removed.
  static bool RemoveSentinel();

  // Retrieve the first run sentinel file info to be accessed in the future;
  // note that this method should NOT be accessed from any non-blocking thread.
  static void LoadSentinelInfo();

  // Get RLZ ping delay pref name.
  static const char* GetPingDelayPrefName();

  // Register user preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Gives the full path to the sentinel file. The file might not exist.
  static bool GetFirstRunSentinelFilePath(base::FilePath* path);

  enum FirstRunState {
    FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
    FIRST_RUN_TRUE,
    FIRST_RUN_FALSE
  };

  // This variable should only be accessed through IsChromeFirstRun().
  static FirstRunState first_run_;
};

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_MODEL_FIRST_RUN_H_

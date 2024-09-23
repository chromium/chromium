// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"

// Name of the directory containing the legacy sessions.
extern const base::FilePath::CharType kLegacySessionsDirname[];

// Name of the directory containing the legacy web sessions.
extern const base::FilePath::CharType kLegacyWebSessionsDirname[];

// Name of the legacy session file.
extern const base::FilePath::CharType kLegacySessionFilename[];

// Name of the directory containing the sessions' storage.
extern const base::FilePath::CharType kSessionRestorationDirname[];

// Name of the session metadata file.
extern const base::FilePath::CharType kSessionMetadataFilename[];

// Name of the file storing the data for a single WebState.
extern const base::FilePath::CharType kWebStateStorageFilename[];

// Name of the file storing the session data for a single WebState.
extern const base::FilePath::CharType kWebStateSessionFilename[];

// Keys used to store information metadata about a WebState in a WebStateList.
extern NSString* const kLegacyWebStateListPinnedStateKey;
extern NSString* const kLegacyWebStateListOpenerIndexKey;
extern NSString* const kLegacyWebStateListOpenerNavigationIndexKey;

// Name of the preference storing the format of the session storage.
extern const char kSessionStorageFormatPref[];

// Name of the preference storing the status of the session storage migration.
extern const char kSessionStorageMigrationStatusPref[];

// Name of the preference storing the date of the last migration attempt.
extern const char kSessionStorageMigrationStartedTimePref[];

// Possible values for the preference storing the format of the session
// storage.
enum class SessionStorageFormat {
  kUnknown,
  kLegacy,
  kOptimized,
};

// Possible values for the preference storing the status of the session
// storage migration.
enum class SessionStorageMigrationStatus {
  kUnkown,
  kSuccess,
  kFailure,
  kInProgress,
};

// Name of the histogram used to record the time spent blocking the main
// thread to save/load the session from storage.
extern const char kSessionHistogramSavingTime[];
extern const char kSessionHistogramLoadingTime[];

// Name of the histograms used to record the status of the session storage
// migration, the format of the session storage and the duration of the
// migration.
extern const char kSessionHistogramStorageFormat[];
extern const char kSessionHistogramStorageMigrationStatus[];
extern const char kSessionHistogramStorageMigrationTiming[];

// Possible value of the kSessionHistogramStorageFormat histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SessionHistogramStorageFormat {
  kLegacy = 0,
  kOptimized = 1,
  kMaxValue = kOptimized,
};

// Possible value of the kSessionHistogramStorageMigrationStatus histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SessionHistogramStorageMigrationStatus {
  kSuccess = 0,
  kFailure = 1,
  kInterrupted = 2,
  kMaxValue = kInterrupted,
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_

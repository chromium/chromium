// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_CONSTANTS_H_

// UMA histogram name for the state of the Sync server during the initial setup
// flow.
inline constexpr char kThemeSyncInitialState[] =
    "IOS.ThemeSync.InitialSyncState";

// UMA histogram name for the action taken by the client when evaluating an
// incoming remote theme.
inline constexpr char kThemeSyncRemoteAction[] =
    "IOS.ThemeSync.RemoteThemeAction";

// UMA histogram name for the action taken when sync is stopped, disabled, or
// cleared.
inline constexpr char kThemeSyncStopAction[] = "IOS.ThemeSync.StopAction";

// State of the Sync server during the initial theme sync setup.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSThemeSyncInitialState)
enum class IOSThemeSyncInitialState {
  kEmptyServer = 0,
  kHasRemoteData = 1,
  kTooManySpecificsError = 2,
  kMaxValue = kTooManySpecificsError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSThemeSyncInitialState)

// The action taken when evaluating a remote theme.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSThemeSyncRemoteAction)
enum class IOSThemeSyncRemoteAction {
  kApplied = 0,
  kIgnoredManagedByPolicy = 1,
  kIgnoredAlreadyMatches = 2,
  kMissingSpecifics = 3,
  kTooManyChangesError = 4,
  kInvalidChangeTypeError = 5,
  kMaxValue = kInvalidChangeTypeError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSThemeSyncRemoteAction)

// The action taken when sync is stopped.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSThemeSyncStopAction)
enum class IOSThemeSyncStopAction {
  kRestoredLocalTheme = 0,
  kMaxValue = kRestoredLocalTheme,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSThemeSyncStopAction)

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_THEME_SYNCABLE_SERVICE_IOS_CONSTANTS_H_

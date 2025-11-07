// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_

#import <string_view>

// The event that triggered the Synced Set Up flow.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyncedSetUpTriggerSource)
enum class SyncedSetUpTriggerSource {
  // Triggered as part of the Post-First Run Experience.
  kPostFirstRun = 0,
  // Triggered by a remote pref change notification from Sync.
  kRemotePrefChange = 1,
  // Triggered by a scene becoming foreground active.
  kSceneActivation = 2,
  kMaxValue = kSceneActivation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:SyncedSetUpTriggerSource)

// Records the specific event that caused the Synced Set Up flow to start.
void LogSyncedSetUpTriggerSource(SyncedSetUpTriggerSource source);

// Tracks the user's interaction with the Synced Set Up Snackbar.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyncedSetUpSnackbarInteraction)
enum class SyncedSetUpSnackbarInteraction {
  // ---------------------------------------------------------------------------
  // Events for the initial snackbar that suggests applying remote changes.
  // ---------------------------------------------------------------------------

  // The initial suggestion snackbar (offering to apply changes) was shown.
  kShownSuggestion = 0,
  // User clicked "Apply" on the suggestion snackbar.
  kClickedApply = 1,
  // The suggestion snackbar timed out (user ignored).
  kDismissedSuggestion = 2,

  // ---------------------------------------------------------------------------
  // Events for the confirmation snackbar shown after changes are applied,
  // which offers an "Undo" action.
  // ---------------------------------------------------------------------------

  // The "Applied" confirmation (with "Undo" button) was shown.
  kShownAppliedConfirmation = 3,
  // User clicked "Undo" on the applied confirmation.
  kClickedUndo = 4,
  // The "Applied" confirmation timed out (user accepted the change).
  kDismissedAppliedConfirmation = 5,

  // ---------------------------------------------------------------------------
  // Events for the confirmation snackbar shown after "Undo" is clicked, which
  // offers a "Redo" (Apply) action.
  // ---------------------------------------------------------------------------

  // The "Undone" confirmation (with "Redo" button) was shown.
  kShownUndoneConfirmation = 6,
  // User clicked "Redo" on the undone confirmation.
  kClickedRedo = 7,
  // The "Undone" confirmation timed out (user accepted the reversion).
  kDismissedUndoneConfirmation = 8,

  kMaxValue = kDismissedUndoneConfirmation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:SyncedSetUpSnackbarInteraction)

// Records the user's interaction with the Synced Set Up snackbar.
void LogSyncedSetUpSnackbarInteraction(SyncedSetUpSnackbarInteraction event);

// Logs the number of remote prefs applied to the local device in a single
// batch.
void LogSyncedSetUpRemoteAppliedPrefCount(int count);

// Select prefs used by the Synced Set Up flow.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyncedSetUpAppliedPref)
enum class SyncedSetUpAppliedPref {
  kOmniboxPosition = 0,
  kMagicStackHomeModule = 1,
  kMostVisitedHomeModule = 2,
  kPriceTrackingHomeModule = 3,
  kSafetyCheckHomeModule = 4,
  kTabResumptionHomeModule = 5,
  kTipsHomeModule = 6,
  kUnknown = 7,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:SyncedSetUpAppliedPref)

// Logs that a specific pref was applied.
void LogSyncedSetUpPrefApplied(std::string_view pref_name);

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_

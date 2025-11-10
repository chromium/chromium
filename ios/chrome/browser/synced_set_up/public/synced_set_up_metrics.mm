// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"

#import "base/metrics/histogram_functions.h"

namespace {

// UMA histogram name for recording the `SyncedSetUpTriggerSource`.
static constexpr char kSyncedSetUpTriggerSource[] =
    "IOS.SyncedSetUp.TriggerSource";

// UMA histogram name for recording the `SyncedSetUpSnackbarInteraction`.
static constexpr char kSyncedSetUpSnackbarInteraction[] =
    "IOS.SyncedSetUp.Snackbar.Interaction";

// UMA histogram name for recording the number of applied remote prefs.
static constexpr char kSyncedSetUpRemoteAppliedPrefCount[] =
    "IOS.SyncedSetUp.RemoteAppliedPrefCount";

}  // namespace

void LogSyncedSetUpTriggerSource(SyncedSetUpTriggerSource source) {
  base::UmaHistogramEnumeration(kSyncedSetUpTriggerSource, source);
}

void LogSyncedSetUpSnackbarInteraction(SyncedSetUpSnackbarInteraction event) {
  base::UmaHistogramEnumeration(kSyncedSetUpSnackbarInteraction, event);
}

void LogSyncedSetUpRemoteAppliedPrefCount(int count) {
  // Uses `Counts100` as the number of applied prefs for the Synced Set Up
  // feature is small (< 10).
  base::UmaHistogramCounts100(kSyncedSetUpRemoteAppliedPrefCount, count);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_

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

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_PUBLIC_SYNCED_SET_UP_METRICS_H_

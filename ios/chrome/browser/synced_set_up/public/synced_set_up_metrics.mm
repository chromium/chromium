// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"

#import "base/metrics/histogram_functions.h"

namespace {

// UMA histogram name for recording the `SyncedSetUpTriggerSource`.
static constexpr char kSyncedSetUpTriggerSource[] =
    "IOS.SyncedSetUp.TriggerSource";

}  // namespace

void LogSyncedSetUpTriggerSource(SyncedSetUpTriggerSource source) {
  base::UmaHistogramEnumeration(kSyncedSetUpTriggerSource, source);
}

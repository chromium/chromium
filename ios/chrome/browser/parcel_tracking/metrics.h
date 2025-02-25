// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_METRICS_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_METRICS_H_

#import "ios/chrome/browser/parcel_tracking/tracking_source.h"

namespace parcel_tracking {

// Interactions with the Parcel Tracking Opt-In prompt. This is mapped to
// the IOSParcelTrackingOptInActionOnPrompt enum in enums.xml for metrics.
enum class OptInPromptActionType {
  kAskEveryTime = 0,
  kAlwaysTrack = 1,
  kNoThanks = 2,
  kSwipeToDismiss = 3,
  kMaxValue = kSwipeToDismiss,
};

// Logs number of parcels untracked to the histogram corresponding to
// `tracking_source`.
void RecordParcelsUntracked(TrackingSource tracking_source,
                            int number_of_parcels);

}  // namespace parcel_tracking

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_METRICS_H_

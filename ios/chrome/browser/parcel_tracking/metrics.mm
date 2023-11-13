// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"

namespace parcel_tracking {

const char kOptInPromptActionHistogramName[] =
    "IOS.ParcelTracking.OptIn.ActionOnPrompt";

const char kOptInPromptDisplayedHistogramName[] =
    "IOS.ParcelTracking.OptIn.Displayed";

// Name of the histogram that logs when packages are tracked from an infobar.
const char kTrackedFromInfobarHistogramName[] =
    "IOS.ParcelTracking.Tracked.Infobar";

// Name of the histogram that logs when packages are tracked from Auto Track.
const char kTrackedFromAutoTrackHistogramName[] =
    "IOS.ParcelTracking.Tracked.AutoTrack";

// Name of the histogram that logs when packages are tracked from long press
// menu.
const char kTrackedFromLongPressHistogramName[] =
    "IOS.ParcelTracking.Tracked.LongPress";

// Name of the histogram that logs when packages are untracked from an infobar
// modal.
const char kUntrackedFromInfobarHistogramName[] =
    "IOS.ParcelTracking.Untracked.Infobar";

// Name of the histogram that logs when packages are untracked from the magic
// stack module.
const char kUntrackedFromMagicStackHistogramName[] =
    "IOS.ParcelTracking.Untracked.MagicStack";

void RecordParcelsTracked(TrackingSource tracking_source,
                          int number_of_parcels) {
  switch (tracking_source) {
    case TrackingSource::kInfobar:
      base::UmaHistogramCounts100(kTrackedFromInfobarHistogramName,
                                  number_of_parcels);
      break;
    case TrackingSource::kLongPress:
      base::UmaHistogramCounts100(kTrackedFromLongPressHistogramName,
                                  number_of_parcels);
      break;
    case TrackingSource::kAutoTrack:
      base::UmaHistogramCounts100(kTrackedFromAutoTrackHistogramName,
                                  number_of_parcels);
      break;
    case TrackingSource::kMagicStackModule:
      // Package cannot be tracked this way.
      break;
  }
}

void RecordParcelsUntracked(TrackingSource tracking_source,
                            int number_of_parcels) {
  switch (tracking_source) {
    case TrackingSource::kInfobar:
      base::UmaHistogramCounts100(kUntrackedFromInfobarHistogramName,
                                  number_of_parcels);
      break;
    case TrackingSource::kMagicStackModule:
      base::UmaHistogramCounts100(kUntrackedFromMagicStackHistogramName,
                                  number_of_parcels);
      break;
    case TrackingSource::kLongPress:
    case TrackingSource::kAutoTrack:
      // Package cannot be un-tracked this way.
      break;
  }
}

}  // namespace parcel_tracking

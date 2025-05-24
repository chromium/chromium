// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_utils.h"

#import "base/logging.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"

FeedActivityBucket FeedActivityBucketForPrefs(PrefService* prefs) {
  const int activity_bucket = prefs->GetInteger(kActivityBucketKey);
  switch (activity_bucket) {
    case base::to_underlying(FeedActivityBucket::kNoActivity):
    case base::to_underlying(FeedActivityBucket::kLowActivity):
    case base::to_underlying(FeedActivityBucket::kMediumActivity):
    case base::to_underlying(FeedActivityBucket::kHighActivity):
      return static_cast<FeedActivityBucket>(activity_bucket);

    default:
      // Do not fail in case of invalid value (to avoid crashing if invalid
      // data is read from disk) but return a value in range.
      DLOG(ERROR) << "Invalid activity bucket value: " << activity_bucket;
      return FeedActivityBucket::kNoActivity;
  }
}

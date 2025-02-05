// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_VIEW_FINDER_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_VIEW_FINDER_METRICS_RECORDER_H_

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"

// Different types of image selection sources.
enum class LensViewFinderImageSource {
  kCamera = 0,
  kGallery = 1,
};

/// Utility for recording lens view finder related metrics.
@interface LensViewFinderMetricsRecorder : NSObject

// Records LVF opened.
- (void)recordLensViewFinderOpened;

// Records LVF closed.
- (void)recordLensViewFinderDismissTapped;

// Records LVF selected an image.
- (void)recordImageWithSource:(LensViewFinderImageSource)source;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_VIEW_FINDER_METRICS_RECORDER_H_

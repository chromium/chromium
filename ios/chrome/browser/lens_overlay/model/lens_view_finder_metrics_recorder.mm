// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_view_finder_metrics_recorder.h"

@implementation LensViewFinderMetricsRecorder

- (void)recordLensViewFinderOpened {
  RecordAction(base::UserMetricsAction("Mobile.LensViewFinder.Opened"));
}

- (void)recordLensViewFinderDismissTapped {
  RecordAction(base::UserMetricsAction("Mobile.LensViewFinder.DismissTapped"));
}

- (void)recordLensViewFinderCameraURLOpen {
  RecordAction(base::UserMetricsAction("Mobile.LensViewFinder.CameraURLOpen"));
}

- (void)recordImageWithSource:(LensViewFinderImageSource)source {
  switch (source) {
    case LensViewFinderImageSource::kCamera:
      RecordAction(
          base::UserMetricsAction("Mobile.LensViewFinder.CameraCapture"));
      break;
    case LensViewFinderImageSource::kGallery:
      RecordAction(
          base::UserMetricsAction("Mobile.LensViewFinder.GalleryImagePicked"));
      break;
  }
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_view_finder_metrics_recorder.h"

#import "components/lens/lens_metrics.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"

using lens::CameraOpenEntryPoint;

@implementation LensViewFinderMetricsRecorder

- (void)recordLensViewFinderOpenedFromEntrypoint:(LensEntrypoint)entrypoint {
  RecordAction(base::UserMetricsAction("Mobile.LensViewFinder.Opened"));

  switch (entrypoint) {
    case LensEntrypoint::HomeScreenWidget:
      RecordCameraOpen(CameraOpenEntryPoint::WIDGET);
      break;
    case LensEntrypoint::NewTabPage:
      RecordCameraOpen(CameraOpenEntryPoint::NEW_TAB_PAGE);
      break;
    case LensEntrypoint::Keyboard:
      RecordCameraOpen(CameraOpenEntryPoint::KEYBOARD);
      break;
    case LensEntrypoint::Composebox:
      RecordCameraOpen(CameraOpenEntryPoint::COMPOSE_BOX);
      break;
    case LensEntrypoint::Spotlight:
      RecordCameraOpen(CameraOpenEntryPoint::SPOTLIGHT);
      break;
    default:
      // Do not record the camera open histogram for other entry points.
      break;
  }
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

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include "base/apple/scoped_cftyperef.h"
#include "services/shape_detection/detection_utils_mac.h"

namespace shape_detection {

FaceDetectionImplMac::FaceDetectionImplMac(
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  NSString* const accuracy =
      options->fast_mode ? CIDetectorAccuracyLow : CIDetectorAccuracyHigh;
  // The CIDetectorMaxFeatureCount option introduced in Mac OS SDK 10.12 can
  // only be used with Rectangle Detectors.
  NSDictionary* const detector_options = @{CIDetectorAccuracy : accuracy};
  detector_ = [CIDetector detectorOfType:CIDetectorTypeFace
                                 context:nil
                                 options:detector_options];
}

FaceDetectionImplMac::~FaceDetectionImplMac() = default;

void FaceDetectionImplMac::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  CIImage* ci_image = CIImageFromSkBitmap(bitmap);
  if (!ci_image) {
    std::move(callback).Run({});
    return;
  }

  NSArray* const features = [detector_ featuresInImage:ci_image];
  const int height = bitmap.height();

  std::vector<mojom::FaceDetectionResultPtr> results;
  for (CIFaceFeature* const f in features) {
    auto face = shape_detection::mojom::FaceDetectionResult::New();
    face->bounding_box = ConvertCGToGfxCoordinates(f.bounds, height);

    if (f.hasLeftEyePosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::EYE;
      landmark->locations.emplace_back(f.leftEyePosition.x,
                                       height - f.leftEyePosition.y);
      face->landmarks.push_back(std::move(landmark));
    }
    if (f.hasRightEyePosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::EYE;
      landmark->locations.emplace_back(f.rightEyePosition.x,
                                       height - f.rightEyePosition.y);
      face->landmarks.push_back(std::move(landmark));
    }
    if (f.hasMouthPosition) {
      auto landmark = shape_detection::mojom::Landmark::New();
      landmark->type = shape_detection::mojom::LandmarkType::MOUTH;
      landmark->locations.emplace_back(f.mouthPosition.x,
                                       height - f.mouthPosition.y);
      face->landmarks.push_back(std::move(landmark));
    }

    results.push_back(std::move(face));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace shape_detection

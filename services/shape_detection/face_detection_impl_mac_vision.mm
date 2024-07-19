// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/shape_detection/face_detection_impl_mac_vision.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

namespace {

mojom::LandmarkPtr BuildLandmark(VNFaceLandmarkRegion2D* landmark_region,
                                 mojom::LandmarkType landmark_type,
                                 gfx::RectF bounding_box) {
  auto landmark = mojom::Landmark::New();
  landmark->type = landmark_type;
  landmark->locations.reserve(landmark_region.pointCount);
  for (NSUInteger i = 0; i < landmark_region.pointCount; ++i) {
    // The points are normalized to the bounding box of the detected face.
    landmark->locations.emplace_back(
        landmark_region.normalizedPoints[i].x * bounding_box.width() +
            bounding_box.x(),
        (1 - landmark_region.normalizedPoints[i].y) * bounding_box.height() +
            bounding_box.y());
  }
  return landmark;
}
}

FaceDetectionImplMacVision::FaceDetectionImplMacVision() : weak_factory_(this) {
  // The repeating callback will not be run if FaceDetectionImplMacVision object
  // has already been destroyed.
  landmarks_async_request_ = VisionAPIAsyncRequestMac::Create(
      [VNDetectFaceLandmarksRequest class],
      base::BindRepeating(&FaceDetectionImplMacVision::OnFacesDetected,
                          weak_factory_.GetWeakPtr()));
}

FaceDetectionImplMacVision::~FaceDetectionImplMacVision() = default;

void FaceDetectionImplMacVision::Detect(const SkBitmap& bitmap,
                                        DetectCallback callback) {
  DCHECK(landmarks_async_request_);

  if (!landmarks_async_request_->PerformRequest(bitmap)) {
    std::move(callback).Run({});
    return;
  }

  image_size_ = CGSizeMake(bitmap.width(), bitmap.height());
  // Hold on the callback until async request completes.
  detected_callback_ = std::move(callback);
  // This prevents the Detect function from being called before the
  // VisionAPIAsyncRequestMac completes.
  if (receiver_)  // Can be unbound in unit testing.
    receiver_->PauseIncomingMethodCallProcessing();
}

void FaceDetectionImplMacVision::OnFacesDetected(VNRequest* request,
                                                 NSError* error) {
  if (receiver_)  // Can be unbound in unit testing.
    receiver_->ResumeIncomingMethodCallProcessing();

  if (!request.results.count || error) {
    std::move(detected_callback_).Run({});
    return;
  }

  std::vector<mojom::FaceDetectionResultPtr> results;
  for (VNFaceObservation* const observation in request.results) {
    auto face = mojom::FaceDetectionResult::New();
    // The coordinate are normalized to the dimensions of the processed image.
    face->bounding_box = ConvertCGToGfxCoordinates(
        CGRectMake(observation.boundingBox.origin.x * image_size_.width,
                   observation.boundingBox.origin.y * image_size_.height,
                   observation.boundingBox.size.width * image_size_.width,
                   observation.boundingBox.size.height * image_size_.height),
        image_size_.height);

    if (VNFaceLandmarkRegion2D* leftEye = observation.landmarks.leftEye) {
      face->landmarks.push_back(
          BuildLandmark(leftEye, mojom::LandmarkType::EYE, face->bounding_box));
    }
    if (VNFaceLandmarkRegion2D* rightEye = observation.landmarks.rightEye) {
      face->landmarks.push_back(BuildLandmark(
          rightEye, mojom::LandmarkType::EYE, face->bounding_box));
    }
    if (VNFaceLandmarkRegion2D* outerLips = observation.landmarks.outerLips) {
      face->landmarks.push_back(BuildLandmark(
          outerLips, mojom::LandmarkType::MOUTH, face->bounding_box));
    }
    if (VNFaceLandmarkRegion2D* nose = observation.landmarks.nose) {
      face->landmarks.push_back(
          BuildLandmark(nose, mojom::LandmarkType::NOSE, face->bounding_box));
    }

    results.push_back(std::move(face));
  }
  std::move(detected_callback_).Run(std::move(results));

  return;
}

}  // namespace shape_detection

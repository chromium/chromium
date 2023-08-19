// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/detection_utils_mac.h"
#include "services/shape_detection/public/mojom/facedetection.mojom.h"

class SkBitmap;

namespace shape_detection {

// The FaceDetectionImplMacVision class is the implementation of Face Detection
// based on Mac OS Vision framework.
class FaceDetectionImplMacVision : public mojom::FaceDetection {
 public:
  FaceDetectionImplMacVision();

  FaceDetectionImplMacVision(const FaceDetectionImplMacVision&) = delete;
  FaceDetectionImplMacVision& operator=(const FaceDetectionImplMacVision&) =
      delete;

  ~FaceDetectionImplMacVision() override;

  void Detect(const SkBitmap& bitmap,
              mojom::FaceDetection::DetectCallback callback) override;

  void SetReceiver(mojo::SelfOwnedReceiverRef<mojom::FaceDetection> receiver) {
    receiver_ = std::move(receiver);
  }

 private:
  void OnFacesDetected(VNRequest* request, NSError* error);

  CGSize image_size_;
  std::unique_ptr<VisionAPIAsyncRequestMac> landmarks_async_request_;
  DetectCallback detected_callback_;
  mojo::SelfOwnedReceiverRef<mojom::FaceDetection> receiver_;
  base::WeakPtrFactory<FaceDetectionImplMacVision> weak_factory_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_

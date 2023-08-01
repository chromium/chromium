// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_

#include "services/shape_detection/public/mojom/facedetection.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class CIDetector;

namespace shape_detection {

class FaceDetectionImplMac : public shape_detection::mojom::FaceDetection {
 public:
  explicit FaceDetectionImplMac(
      shape_detection::mojom::FaceDetectorOptionsPtr options);

  FaceDetectionImplMac(const FaceDetectionImplMac&) = delete;
  FaceDetectionImplMac& operator=(const FaceDetectionImplMac&) = delete;

  ~FaceDetectionImplMac() override;

  void Detect(
      const SkBitmap& bitmap,
      shape_detection::mojom::FaceDetection::DetectCallback callback) override;

 private:
  CIDetector* __strong detector_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_impl.h"

#include "services/shape_detection/public/mojom/facedetection.mojom.h"

namespace shape_detection {

void FaceDetectionProviderImpl::CreateFaceDetection(
    mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  DLOG(ERROR) << "Platform not supported for Face Detection Service.";
}

}  // namespace shape_detection

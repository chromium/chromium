// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"

namespace shape_detection {

class FaceDetectionProviderImpl
    : public shape_detection::mojom::FaceDetectionProvider {
 public:
  ~FaceDetectionProviderImpl() override = default;

  static void Create(
      mojo::PendingReceiver<shape_detection::mojom::FaceDetectionProvider>
          receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<FaceDetectionProviderImpl>(),
                                std::move(receiver));
  }

  void CreateFaceDetection(
      mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
      shape_detection::mojom::FaceDetectorOptionsPtr options) override;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_

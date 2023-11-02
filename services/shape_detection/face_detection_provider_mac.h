// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_MAC_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_MAC_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"

namespace shape_detection {

// The FaceDetectionProviderMac class is a provider that binds an implementation
// of mojom::FaceDetection with Core Image or Vision Framework.
class FaceDetectionProviderMac
    : public shape_detection::mojom::FaceDetectionProvider {
 public:
  FaceDetectionProviderMac();

  FaceDetectionProviderMac(const FaceDetectionProviderMac&) = delete;
  FaceDetectionProviderMac& operator=(const FaceDetectionProviderMac&) = delete;

  ~FaceDetectionProviderMac() override;

  // Binds FaceDetection provider receiver to the implementation of
  // mojom::FaceDetectionProvider.
  static void Create(
      mojo::PendingReceiver<mojom::FaceDetectionProvider> receiver);

  void CreateFaceDetection(mojo::PendingReceiver<mojom::FaceDetection> receiver,
                           mojom::FaceDetectorOptionsPtr options) override;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_MAC_H_

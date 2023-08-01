// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_mac.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/face_detection_impl_mac.h"
#include "services/shape_detection/face_detection_impl_mac_vision.h"

namespace shape_detection {

FaceDetectionProviderMac::FaceDetectionProviderMac() = default;

FaceDetectionProviderMac::~FaceDetectionProviderMac() = default;

// static
void FaceDetectionProviderMac::Create(
    mojo::PendingReceiver<mojom::FaceDetectionProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FaceDetectionProviderMac>(),
                              std::move(receiver));
}

void FaceDetectionProviderMac::CreateFaceDetection(
    mojo::PendingReceiver<mojom::FaceDetection> receiver,
    mojom::FaceDetectorOptionsPtr options) {
  // Vision is more accurate than Core Image Framework, but it also needs more
  // processing time.
  if (options->fast_mode) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FaceDetectionImplMac>(std::move(options)),
        std::move(receiver));
    return;
  }

  auto impl = std::make_unique<FaceDetectionImplMacVision>();
  auto* impl_ptr = impl.get();
  impl_ptr->SetReceiver(
      mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver)));
}

}  // namespace shape_detection

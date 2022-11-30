// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_WIN_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_WIN_H_

#include <windows.foundation.h>
#include <windows.graphics.imaging.h>
#include <wrl/client.h>
#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/face_detection_impl_win.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"

namespace shape_detection {

class FaceDetectionProviderWin
    : public shape_detection::mojom::FaceDetectionProvider {
 public:
  FaceDetectionProviderWin();

  FaceDetectionProviderWin(const FaceDetectionProviderWin&) = delete;
  FaceDetectionProviderWin& operator=(const FaceDetectionProviderWin&) = delete;

  ~FaceDetectionProviderWin() override;

  static void Create(
      mojo::PendingReceiver<shape_detection::mojom::FaceDetectionProvider>
          receiver) {
    auto provider = std::make_unique<FaceDetectionProviderWin>();
    auto* provider_ptr = provider.get();
    provider_ptr->receiver_ =
        mojo::MakeSelfOwnedReceiver(std::move(provider), std::move(receiver));
  }

  void CreateFaceDetection(
      mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
      shape_detection::mojom::FaceDetectorOptionsPtr options) override;

 private:
  void OnFaceDetectorCreated(
      mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat pixel_format,
      Microsoft::WRL::ComPtr<ABI::Windows::Media::FaceAnalysis::IFaceDetector>
          face_detector);

  mojo::SelfOwnedReceiverRef<mojom::FaceDetectionProvider> receiver_;
  base::WeakPtrFactory<FaceDetectionProviderWin> weak_factory_{this};
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_WIN_H_

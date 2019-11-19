// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_

#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#include <windows.graphics.imaging.h>
#include <windows.media.faceanalysis.h>
#include <wrl/client.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/public/mojom/facedetection.mojom.h"

class SkBitmap;

namespace shape_detection {

class FaceDetectionImplWin : public mojom::FaceDetection {
 public:
  FaceDetectionImplWin(
      Microsoft::WRL::ComPtr<ABI::Windows::Media::FaceAnalysis::IFaceDetector>
          face_detector,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics>
          bitmap_factory,
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat pixel_format);
  ~FaceDetectionImplWin() override;

  void SetReceiver(mojo::SelfOwnedReceiverRef<mojom::FaceDetection> receiver) {
    receiver_ = std::move(receiver);
  }

  // mojom::FaceDetection implementation.
  void Detect(const SkBitmap& bitmap,
              mojom::FaceDetection::DetectCallback callback) override;

 private:
  HRESULT BeginDetect(const SkBitmap& bitmap);
  std::vector<mojom::FaceDetectionResultPtr> BuildFaceDetectionResult(
      Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVector<
          ABI::Windows::Media::FaceAnalysis::DetectedFace*>> result);
  void OnFaceDetected(
      Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::ISoftwareBitmap>
          win_bitmap,
      Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVector<
          ABI::Windows::Media::FaceAnalysis::DetectedFace*>> result);

  Microsoft::WRL::ComPtr<ABI::Windows::Media::FaceAnalysis::IFaceDetector>
      face_detector_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics>
      bitmap_factory_;
  ABI::Windows::Graphics::Imaging::BitmapPixelFormat pixel_format_;

  DetectCallback detected_face_callback_;
  mojo::SelfOwnedReceiverRef<mojom::FaceDetection> receiver_;

  base::WeakPtrFactory<FaceDetectionImplWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplWin);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_

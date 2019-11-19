// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_
#define SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_

#include <windows.graphics.imaging.h>
#include <windows.media.ocr.h>
#include <wrl/client.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"

class SkBitmap;

namespace shape_detection {

class TextDetectionImplWin : public mojom::TextDetection {
 public:
  TextDetectionImplWin(
      Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrEngine> ocr_engine,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics>
          bitmap_factory);
  ~TextDetectionImplWin() override;

  // mojom::TextDetection implementation.
  void Detect(const SkBitmap& bitmap,
              mojom::TextDetection::DetectCallback callback) override;

  void SetReceiver(mojo::SelfOwnedReceiverRef<mojom::TextDetection> receiver) {
    receiver_ = std::move(receiver);
  }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrEngine> ocr_engine_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics>
      bitmap_factory_;
  DetectCallback recognize_text_callback_;
  mojo::SelfOwnedReceiverRef<mojom::TextDetection> receiver_;

  HRESULT BeginDetect(const SkBitmap& bitmap);
  std::vector<mojom::TextDetectionResultPtr> BuildTextDetectionResult(
      Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrResult> ocr_result);
  void OnTextDetected(
      Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Imaging::ISoftwareBitmap>
          win_bitmap,
      Microsoft::WRL::ComPtr<ABI::Windows::Media::Ocr::IOcrResult> ocr_result);

  base::WeakPtrFactory<TextDetectionImplWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TextDetectionImplWin);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_

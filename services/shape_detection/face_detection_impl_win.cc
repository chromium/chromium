// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/win/post_async_results.h"
#include "services/shape_detection/detection_utils_win.h"

namespace shape_detection {

namespace {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Graphics::Imaging::BitmapPixelFormat;
using ABI::Windows::Media::FaceAnalysis::DetectedFace;
using ABI::Windows::Media::FaceAnalysis::IDetectedFace;
using ABI::Windows::Media::FaceAnalysis::IFaceDetector;

using Microsoft::WRL::ComPtr;

}  // namespace

FaceDetectionImplWin::FaceDetectionImplWin(
    ComPtr<IFaceDetector> face_detector,
    ComPtr<ISoftwareBitmapStatics> bitmap_factory,
    BitmapPixelFormat pixel_format)
    : face_detector_(std::move(face_detector)),
      bitmap_factory_(std::move(bitmap_factory)),
      pixel_format_(pixel_format) {
  DCHECK(face_detector_);
  DCHECK(bitmap_factory_);
}
FaceDetectionImplWin::~FaceDetectionImplWin() = default;

void FaceDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  if (FAILED(BeginDetect(bitmap))) {
    // No detection taking place; run |callback| with an empty array of results.
    std::move(callback).Run(std::vector<mojom::FaceDetectionResultPtr>());
    return;
  }
  // Hold on the callback until AsyncOperation completes.
  detected_face_callback_ = std::move(callback);
  // This prevents the Detect function from being called before the
  // AsyncOperation completes.
  receiver_->PauseIncomingMethodCallProcessing();
}

HRESULT FaceDetectionImplWin::BeginDetect(const SkBitmap& bitmap) {
  ComPtr<ISoftwareBitmap> win_bitmap = CreateWinBitmapWithPixelFormat(
      bitmap, bitmap_factory_.Get(), pixel_format_);
  if (!win_bitmap)
    return E_FAIL;

  // Detect faces asynchronously.
  ComPtr<IAsyncOperation<IVector<DetectedFace*>*>> async_op;
  HRESULT hr = face_detector_->DetectFacesAsync(win_bitmap.Get(), &async_op);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Detect faces asynchronously failed: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  // Use WeakPtr to bind the callback so that the once callback will not be run
  // if this object has been already destroyed. |win_bitmap| needs to be kept
  // alive until OnFaceDetected().
  hr = base::win::PostAsyncResults(
      std::move(async_op),
      base::BindOnce(&FaceDetectionImplWin::OnFaceDetected,
                     weak_factory_.GetWeakPtr(), std::move(win_bitmap)));
  if (FAILED(hr)) {
    DLOG(ERROR) << "PostAsyncResults failed: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  return hr;
}

std::vector<mojom::FaceDetectionResultPtr>
FaceDetectionImplWin::BuildFaceDetectionResult(
    ComPtr<IVector<DetectedFace*>> detected_face) {
  std::vector<mojom::FaceDetectionResultPtr> results;
  if (!detected_face)
    return results;

  uint32_t count;
  HRESULT hr = detected_face->get_Size(&count);
  if (FAILED(hr)) {
    DLOG(ERROR) << "get_Size failed: " << logging::SystemErrorCodeToString(hr);
    return results;
  }

  results.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    ComPtr<IDetectedFace> face;
    hr = detected_face->GetAt(i, &face);
    if (FAILED(hr))
      break;

    ABI::Windows::Graphics::Imaging::BitmapBounds bounds;
    hr = face->get_FaceBox(&bounds);
    if (FAILED(hr))
      break;

    auto result = shape_detection::mojom::FaceDetectionResult::New();
    result->bounding_box =
        gfx::RectF(bounds.X, bounds.Y, bounds.Width, bounds.Height);
    results.push_back(std::move(result));
  }
  return results;
}

// |win_bitmap| is passed here so that it is kept alive until the AsyncOperation
// completes because DetectFacesAsync does not hold a reference.
void FaceDetectionImplWin::OnFaceDetected(
    ComPtr<ISoftwareBitmap> /* win_bitmap */,
    ComPtr<IVector<DetectedFace*>> result) {
  std::move(detected_face_callback_)
      .Run(BuildFaceDetectionResult(std::move(result)));
  receiver_->ResumeIncomingMethodCallProcessing();
}

}  // namespace shape_detection

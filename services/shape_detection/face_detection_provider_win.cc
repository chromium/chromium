// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_win.h"

#include <windows.media.faceanalysis.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace shape_detection {

namespace {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Graphics::Imaging::BitmapPixelFormat;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics;
using ABI::Windows::Media::FaceAnalysis::FaceDetector;
using ABI::Windows::Media::FaceAnalysis::IFaceDetector;
using ABI::Windows::Media::FaceAnalysis::IFaceDetectorStatics;

using base::win::GetActivationFactory;
using base::win::ScopedHString;
using Microsoft::WRL::ComPtr;

BitmapPixelFormat GetPreferredPixelFormat(IFaceDetectorStatics* factory) {
  static constexpr BitmapPixelFormat kFormats[] = {
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Gray8,
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Nv12};

  for (const auto& format : kFormats) {
    boolean is_supported = false;
    factory->IsBitmapPixelFormatSupported(format, &is_supported);
    if (is_supported)
      return format;
  }
  return ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Unknown;
}

}  // namespace

void FaceDetectionProviderWin::CreateFaceDetection(
    mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
    shape_detection::mojom::FaceDetectorOptionsPtr options) {
  ComPtr<IFaceDetectorStatics> factory;
  HRESULT hr = GetActivationFactory<
      IFaceDetectorStatics,
      RuntimeClass_Windows_Media_FaceAnalysis_FaceDetector>(&factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IFaceDetectorStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  boolean is_supported = false;
  factory->get_IsSupported(&is_supported);
  if (!is_supported)
    return;

  // In the current version, the FaceDetector class only supports images in
  // Gray8 or Nv12. Gray8 should be a good type but verify it against
  // FaceDetector’s supported formats.
  BitmapPixelFormat pixel_format = GetPreferredPixelFormat(factory.Get());
  if (pixel_format ==
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Unknown) {
    return;
  }

  // Create an instance of FaceDetector asynchronously.
  ComPtr<IAsyncOperation<FaceDetector*>> async_op;
  hr = factory->CreateAsync(&async_op);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create FaceDetector failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  // Use WeakPtr to bind the callback so that the once callback will not be run
  // if this object has been already destroyed.
  hr = base::win::PostAsyncResults(
      std::move(async_op),
      base::BindOnce(&FaceDetectionProviderWin::OnFaceDetectorCreated,
                     weak_factory_.GetWeakPtr(), std::move(receiver),
                     pixel_format));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Begin async operation failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  // When |provider| goes out of scope it will immediately close its end of
  // the message pipe, then the callback OnFaceDetectorCreated will be not
  // called. This prevents this object from being destroyed before the
  // AsyncOperation completes.
  receiver_->PauseIncomingMethodCallProcessing();
}

FaceDetectionProviderWin::FaceDetectionProviderWin() {}

FaceDetectionProviderWin::~FaceDetectionProviderWin() = default;

void FaceDetectionProviderWin::OnFaceDetectorCreated(
    mojo::PendingReceiver<shape_detection::mojom::FaceDetection> receiver,
    BitmapPixelFormat pixel_format,
    ComPtr<IFaceDetector> face_detector) {
  receiver_->ResumeIncomingMethodCallProcessing();

  if (!face_detector)
    return;

  ComPtr<ISoftwareBitmapStatics> bitmap_factory;
  const HRESULT hr = GetActivationFactory<
      ISoftwareBitmapStatics,
      RuntimeClass_Windows_Graphics_Imaging_SoftwareBitmap>(&bitmap_factory);
  if (FAILED(hr)) {
    DLOG(ERROR) << "ISoftwareBitmapStatics factory failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  auto impl = std::make_unique<FaceDetectionImplWin>(
      std::move(face_detector), std::move(bitmap_factory), pixel_format);
  auto* impl_ptr = impl.get();
  impl_ptr->SetReceiver(
      mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver)));
}

}  // namespace shape_detection

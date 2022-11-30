// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/detection_utils_win.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/win/winrt_storage_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

using Microsoft::WRL::ComPtr;

ComPtr<ISoftwareBitmap> CreateWinBitmapFromSkBitmap(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory) {
  DCHECK(bitmap_factory);
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);
  if (!base::CheckedNumeric<uint32_t>(bitmap.computeByteSize()).IsValid()) {
    DLOG(ERROR) << "Data overflow.";
    return nullptr;
  }

  // Create IBuffer from bitmap data.
  ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
  HRESULT hr = base::win::CreateIBufferFromData(
      static_cast<uint8_t*>(bitmap.getPixels()),
      static_cast<UINT32>(bitmap.computeByteSize()), &buffer);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create IBuffer from bitmap data failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<ISoftwareBitmap> win_bitmap;
#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
  const BitmapPixelFormat pixel_format =
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Rgba8;
#else
  const BitmapPixelFormat pixel_format =
      ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Bgra8;
#endif
  // Create ISoftwareBitmap from SKBitmap that is kN32_SkColorType and copy the
  // IBuffer into it.
  hr = bitmap_factory->CreateCopyFromBuffer(
      buffer.Get(), pixel_format, bitmap.width(), bitmap.height(), &win_bitmap);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create ISoftwareBitmap from buffer failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return win_bitmap;
}

ComPtr<ISoftwareBitmap> CreateWinBitmapWithPixelFormat(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory,
    BitmapPixelFormat pixel_format) {
  ComPtr<ISoftwareBitmap> win_bitmap =
      CreateWinBitmapFromSkBitmap(bitmap, bitmap_factory);

  // Convert Rgba8/Bgra8 to Gray8/Nv12 SoftwareBitmap.
  ComPtr<ISoftwareBitmap> converted_bitmap;
  const HRESULT hr = bitmap_factory->Convert(win_bitmap.Get(), pixel_format,
                                             &converted_bitmap);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Convert Rgba8/Bgra8 to Gray8/Nv12 failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return converted_bitmap;
}

}  // namespace shape_detection

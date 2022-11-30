// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_

#include <windows.storage.streams.h>
#include <wrl/client.h>

class SkBitmap;

namespace shape_detection {

using ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmap;
using ABI::Windows::Graphics::Imaging::BitmapPixelFormat;

// Creates a Windows ISoftwareBitmap from a kN32_SkColorType |bitmap|, or
// returns nullptr.
Microsoft::WRL::ComPtr<ISoftwareBitmap> CreateWinBitmapFromSkBitmap(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory);

// Creates a Gray8/Nv12 ISoftwareBitmap from a kN32_SkColorType |bitmap|, or
// returns nullptr.
Microsoft::WRL::ComPtr<ISoftwareBitmap> CreateWinBitmapWithPixelFormat(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory,
    BitmapPixelFormat pixel_format);

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_

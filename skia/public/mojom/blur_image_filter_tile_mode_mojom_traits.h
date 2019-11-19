// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_BLUR_IMAGE_FILTER_TILE_MODE_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_BLUR_IMAGE_FILTER_TILE_MODE_MOJOM_TRAITS_H_

#include "skia/public/mojom/blur_image_filter_tile_mode.mojom-shared.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

namespace mojo {

template <>
struct EnumTraits<skia::mojom::BlurTileMode, SkBlurImageFilter::TileMode> {
  static skia::mojom::BlurTileMode ToMojom(
      SkBlurImageFilter::TileMode tile_mode) {
    switch (tile_mode) {
      case SkBlurImageFilter::kClamp_TileMode:
        return skia::mojom::BlurTileMode::CLAMP;
      case SkBlurImageFilter::kRepeat_TileMode:
        return skia::mojom::BlurTileMode::REPEAT;
      case SkBlurImageFilter::kClampToBlack_TileMode:
        return skia::mojom::BlurTileMode::CLAMP_TO_BLACK;
    }
    NOTREACHED();
    return skia::mojom::BlurTileMode::CLAMP_TO_BLACK;
  }

  static bool FromMojom(skia::mojom::BlurTileMode input,
                        SkBlurImageFilter::TileMode* out) {
    switch (input) {
      case skia::mojom::BlurTileMode::CLAMP:
        *out = SkBlurImageFilter::kClamp_TileMode;
        return true;
      case skia::mojom::BlurTileMode::REPEAT:
        *out = SkBlurImageFilter::kRepeat_TileMode;
        return true;
      case skia::mojom::BlurTileMode::CLAMP_TO_BLACK:
        *out = SkBlurImageFilter::kClampToBlack_TileMode;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_BLUR_IMAGE_FILTER_TILE_MODE_MOJOM_TRAITS_H_

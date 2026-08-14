// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_BLEND_MODE_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_BLEND_MODE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "skia/public/mojom/blend_mode.mojom-shared.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace mojo {

template <>
struct EnumTraits<skia::mojom::BlendMode, SkBlendMode> {
  static skia::mojom::BlendMode ToMojom(SkBlendMode input) {
    switch (input) {
      case SkBlendMode::kClear:
        return skia::mojom::BlendMode::kClear;
      case SkBlendMode::kSrc:
        return skia::mojom::BlendMode::kSrc;
      case SkBlendMode::kDst:
        return skia::mojom::BlendMode::kDst;
      case SkBlendMode::kSrcOver:
        return skia::mojom::BlendMode::kSrcOver;
      case SkBlendMode::kDstOver:
        return skia::mojom::BlendMode::kDstOver;
      case SkBlendMode::kSrcIn:
        return skia::mojom::BlendMode::kSrcIn;
      case SkBlendMode::kDstIn:
        return skia::mojom::BlendMode::kDstIn;
      case SkBlendMode::kSrcOut:
        return skia::mojom::BlendMode::kSrcOut;
      case SkBlendMode::kDstOut:
        return skia::mojom::BlendMode::kDstOut;
      case SkBlendMode::kSrcATop:
        return skia::mojom::BlendMode::kSrcATop;
      case SkBlendMode::kDstATop:
        return skia::mojom::BlendMode::kDstATop;
      case SkBlendMode::kXor:
        return skia::mojom::BlendMode::kXor;
      case SkBlendMode::kPlus:
        return skia::mojom::BlendMode::kPlus;
      case SkBlendMode::kModulate:
        return skia::mojom::BlendMode::kModulate;
      case SkBlendMode::kScreen:
        return skia::mojom::BlendMode::kScreen;
      case SkBlendMode::kOverlay:
        return skia::mojom::BlendMode::kOverlay;
      case SkBlendMode::kDarken:
        return skia::mojom::BlendMode::kDarken;
      case SkBlendMode::kLighten:
        return skia::mojom::BlendMode::kLighten;
      case SkBlendMode::kColorDodge:
        return skia::mojom::BlendMode::kColorDodge;
      case SkBlendMode::kColorBurn:
        return skia::mojom::BlendMode::kColorBurn;
      case SkBlendMode::kHardLight:
        return skia::mojom::BlendMode::kHardLight;
      case SkBlendMode::kSoftLight:
        return skia::mojom::BlendMode::kSoftLight;
      case SkBlendMode::kDifference:
        return skia::mojom::BlendMode::kDifference;
      case SkBlendMode::kExclusion:
        return skia::mojom::BlendMode::kExclusion;
      case SkBlendMode::kMultiply:
        return skia::mojom::BlendMode::kMultiply;
      case SkBlendMode::kHue:
        return skia::mojom::BlendMode::kHue;
      case SkBlendMode::kSaturation:
        return skia::mojom::BlendMode::kSaturation;
      case SkBlendMode::kColor:
        return skia::mojom::BlendMode::kColor;
      case SkBlendMode::kLuminosity:
        return skia::mojom::BlendMode::kLuminosity;
    }
    NOTREACHED();
  }

  static SkBlendMode FromMojom(skia::mojom::BlendMode input) {
    switch (input) {
      case skia::mojom::BlendMode::kClear:
        return SkBlendMode::kClear;
      case skia::mojom::BlendMode::kSrc:
        return SkBlendMode::kSrc;
      case skia::mojom::BlendMode::kDst:
        return SkBlendMode::kDst;
      case skia::mojom::BlendMode::kSrcOver:
        return SkBlendMode::kSrcOver;
      case skia::mojom::BlendMode::kDstOver:
        return SkBlendMode::kDstOver;
      case skia::mojom::BlendMode::kSrcIn:
        return SkBlendMode::kSrcIn;
      case skia::mojom::BlendMode::kDstIn:
        return SkBlendMode::kDstIn;
      case skia::mojom::BlendMode::kSrcOut:
        return SkBlendMode::kSrcOut;
      case skia::mojom::BlendMode::kDstOut:
        return SkBlendMode::kDstOut;
      case skia::mojom::BlendMode::kSrcATop:
        return SkBlendMode::kSrcATop;
      case skia::mojom::BlendMode::kDstATop:
        return SkBlendMode::kDstATop;
      case skia::mojom::BlendMode::kXor:
        return SkBlendMode::kXor;
      case skia::mojom::BlendMode::kPlus:
        return SkBlendMode::kPlus;
      case skia::mojom::BlendMode::kModulate:
        return SkBlendMode::kModulate;
      case skia::mojom::BlendMode::kScreen:
        return SkBlendMode::kScreen;
      case skia::mojom::BlendMode::kOverlay:
        return SkBlendMode::kOverlay;
      case skia::mojom::BlendMode::kDarken:
        return SkBlendMode::kDarken;
      case skia::mojom::BlendMode::kLighten:
        return SkBlendMode::kLighten;
      case skia::mojom::BlendMode::kColorDodge:
        return SkBlendMode::kColorDodge;
      case skia::mojom::BlendMode::kColorBurn:
        return SkBlendMode::kColorBurn;
      case skia::mojom::BlendMode::kHardLight:
        return SkBlendMode::kHardLight;
      case skia::mojom::BlendMode::kSoftLight:
        return SkBlendMode::kSoftLight;
      case skia::mojom::BlendMode::kDifference:
        return SkBlendMode::kDifference;
      case skia::mojom::BlendMode::kExclusion:
        return SkBlendMode::kExclusion;
      case skia::mojom::BlendMode::kMultiply:
        return SkBlendMode::kMultiply;
      case skia::mojom::BlendMode::kHue:
        return SkBlendMode::kHue;
      case skia::mojom::BlendMode::kSaturation:
        return SkBlendMode::kSaturation;
      case skia::mojom::BlendMode::kColor:
        return SkBlendMode::kColor;
      case skia::mojom::BlendMode::kLuminosity:
        return SkBlendMode::kLuminosity;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_BLEND_MODE_MOJOM_TRAITS_H_

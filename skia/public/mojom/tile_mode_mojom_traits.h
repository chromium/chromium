// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_TILE_MODE_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_TILE_MODE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "skia/public/mojom/tile_mode.mojom-shared.h"
#include "third_party/skia/include/core/SkTileMode.h"

namespace mojo {

template <>
struct EnumTraits<skia::mojom::TileMode, SkTileMode> {
  static skia::mojom::TileMode ToMojom(SkTileMode tile_mode) {
    switch (tile_mode) {
      case SkTileMode::kClamp:
        return skia::mojom::TileMode::CLAMP;
      case SkTileMode::kRepeat:
        return skia::mojom::TileMode::REPEAT;
      case SkTileMode::kMirror:
        return skia::mojom::TileMode::MIRROR;
      case SkTileMode::kDecal:
        return skia::mojom::TileMode::DECAL;
    }
    NOTREACHED();
  }

  static bool FromMojom(skia::mojom::TileMode input, SkTileMode* out) {
    switch (input) {
      case skia::mojom::TileMode::CLAMP:
        *out = SkTileMode::kClamp;
        return true;
      case skia::mojom::TileMode::REPEAT:
        *out = SkTileMode::kRepeat;
        return true;
      case skia::mojom::TileMode::MIRROR:
        *out = SkTileMode::kMirror;
        return true;
      case skia::mojom::TileMode::DECAL:
        *out = SkTileMode::kDecal;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_TILE_MODE_MOJOM_TRAITS_H_

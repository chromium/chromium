// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_SURFACE_ORIGIN_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SURFACE_ORIGIN_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "skia/public/mojom/surface_origin.mojom-shared.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace mojo {

template <>
struct EnumTraits<skia::mojom::SurfaceOrigin, GrSurfaceOrigin> {
  static skia::mojom::SurfaceOrigin ToMojom(GrSurfaceOrigin origin) {
    switch (origin) {
      case kTopLeft_GrSurfaceOrigin:
        return skia::mojom::SurfaceOrigin::kTopLeft;
      case kBottomLeft_GrSurfaceOrigin:
        return skia::mojom::SurfaceOrigin::kBottomLeft;
    }
    NOTREACHED();
  }

  static bool FromMojom(skia::mojom::SurfaceOrigin origin,
                        GrSurfaceOrigin* out_origin) {
    switch (origin) {
      case skia::mojom::SurfaceOrigin::kTopLeft:
        *out_origin = kTopLeft_GrSurfaceOrigin;
        return true;
      case skia::mojom::SurfaceOrigin::kBottomLeft:
        *out_origin = kBottomLeft_GrSurfaceOrigin;
        return true;
    }

    // Mojo has already validated that `origin` is a valid value, so it must be
    // covered by one of the cases above.
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SURFACE_ORIGIN_MOJOM_TRAITS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_SKCOLORSPACE_PRIMARIES_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SKCOLORSPACE_PRIMARIES_MOJOM_TRAITS_H_

#include "skia/ext/skcolorspace_primaries.h"
#include "skia/public/mojom/skcolorspace_primaries.mojom.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkColorSpacePrimariesDataView,
                    ::SkColorSpacePrimaries> {
  static float rX(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fRX;
  }
  static float rY(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fRY;
  }
  static float gX(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fGX;
  }
  static float gY(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fGY;
  }
  static float bX(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fBX;
  }
  static float bY(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fBY;
  }
  static float wX(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fWX;
  }
  static float wY(const ::SkColorSpacePrimaries& primaries) {
    return primaries.fWY;
  }

  static bool Read(skia::mojom::SkColorSpacePrimariesDataView data,
                   ::SkColorSpacePrimaries* color) {
    color->fRX = data.rX();
    color->fRY = data.rY();
    color->fGX = data.gX();
    color->fGY = data.gY();
    color->fBX = data.bX();
    color->fBY = data.bY();
    color->fWX = data.wX();
    color->fWY = data.wY();
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKCOLORSPACE_PRIMARIES_MOJOM_TRAITS_H_

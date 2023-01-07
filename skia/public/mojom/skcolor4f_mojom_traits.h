// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_SKCOLOR4F_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SKCOLOR4F_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolor4f.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkColor4fDataView, ::SkColor4f> {
  static float r(::SkColor4f color) { return color.fR; }
  static float g(::SkColor4f color) { return color.fG; }
  static float b(::SkColor4f color) { return color.fB; }
  static float a(::SkColor4f color) { return color.fA; }
  static bool Read(skia::mojom::SkColor4fDataView data, ::SkColor4f* color) {
    color->fR = data.r();
    color->fG = data.g();
    color->fB = data.b();
    color->fA = data.a();
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKCOLOR4F_MOJOM_TRAITS_H_

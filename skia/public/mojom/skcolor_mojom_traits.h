// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_SKCOLOR_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_SKCOLOR_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolor.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkColorDataView, ::SkColor> {
  static uint32_t value(::SkColor color) { return color; }
  static bool Read(skia::mojom::SkColorDataView data, ::SkColor* color) {
    *color = data.value();
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_SKCOLOR_MOJOM_TRAITS_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_PROPERTIES_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_PROPERTIES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/css/color_scheme.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::ColorScheme, ::blink::ColorScheme> {
  static blink::mojom::ColorScheme ToMojom(::blink::ColorScheme color_scheme) {
    switch (color_scheme) {
      case ::blink::ColorScheme::kLight:
        return blink::mojom::ColorScheme::kLight;
      case ::blink::ColorScheme::kDark:
        return blink::mojom::ColorScheme::kDark;
    }
    NOTREACHED();
    return blink::mojom::ColorScheme::kLight;
  }
  static bool FromMojom(blink::mojom::ColorScheme input,
                        ::blink::ColorScheme* out) {
    switch (input) {
      case blink::mojom::ColorScheme::kLight:
        *out = ::blink::ColorScheme::kLight;
        return true;
      case blink::mojom::ColorScheme::kDark:
        *out = ::blink::ColorScheme::kDark;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_PROPERTIES_MOJOM_TRAITS_H_

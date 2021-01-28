// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFO_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFO_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/widget/screen_info.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ScreenInfoDataView, blink::ScreenInfo> {
  static float device_scale_factor(const blink::ScreenInfo& r) {
    return r.device_scale_factor;
  }

  static const gfx::DisplayColorSpaces& display_color_spaces(
      const blink::ScreenInfo& r) {
    return r.display_color_spaces;
  }

  static int depth(const blink::ScreenInfo& r) { return r.depth; }

  static int depth_per_component(const blink::ScreenInfo& r) {
    return r.depth_per_component;
  }

  static bool is_monochrome(const blink::ScreenInfo& r) {
    return r.is_monochrome;
  }

  static int display_frequency(const blink::ScreenInfo& r) {
    return r.display_frequency;
  }

  static const gfx::Rect& rect(const blink::ScreenInfo& r) { return r.rect; }

  static const gfx::Rect& available_rect(const blink::ScreenInfo& r) {
    return r.available_rect;
  }

  static blink::mojom::ScreenOrientation orientation_type(
      const blink::ScreenInfo& r) {
    return r.orientation_type;
  }

  static uint16_t orientation_angle(const blink::ScreenInfo& r) {
    return r.orientation_angle;
  }

  static bool is_extended(const blink::ScreenInfo& r) { return r.is_extended; }

  static bool Read(blink::mojom::ScreenInfoDataView r, blink::ScreenInfo* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFO_MOJOM_TRAITS_H_

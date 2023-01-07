// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face_from_typeface.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkStream.h"

// clang-format off
#include <hb.h>
#include <hb-cplusplus.hh>
#include <hb-ot.h>
// clang-format on

namespace {
SkFontTableTag kCpalTag = SkSetFourByteTag('C', 'P', 'A', 'L');

}  // namespace

namespace blink {

/* static */
absl::optional<uint16_t> OpenTypeCpalLookup::FirstThemedPalette(
    sk_sp<SkTypeface> typeface,
    PaletteUse palette_use) {
  if (!typeface || !typeface->getTableSize(kCpalTag))
    return absl::nullopt;

  hb::unique_ptr<hb_face_t> face(HbFaceFromSkTypeface(typeface));

  if (!face || !hb_ot_color_has_palettes(face.get()))
    return absl::nullopt;

  int num_palettes = hb_ot_color_palette_get_count(face.get());

  const hb_ot_color_palette_flags_t desired_flag =
      palette_use == kUsableWithLightBackground
          ? HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_LIGHT_BACKGROUND
          : HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_DARK_BACKGROUND;
  for (int i = 0; i < num_palettes; ++i) {
    if (hb_ot_color_palette_get_flags(face.get(), i) == desired_flag)
      return i;
  }
  return absl::nullopt;
}

}  // namespace blink

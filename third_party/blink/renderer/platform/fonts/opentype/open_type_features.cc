// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_features.h"

#include <hb-ot.h>

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

OpenTypeFeatures::OpenTypeFeatures(const SimpleFontData& font)
    : features_(kInitialSize) {
  const FontPlatformData& platform_data = font.PlatformData();
  HarfBuzzFace* const face = platform_data.GetHarfBuzzFace();
  DCHECK(face);
  hb_font_t* const hb_font = face->GetScaledFont();
  DCHECK(hb_font);
  hb_face_t* const hb_face = hb_font_get_face(hb_font);
  DCHECK(hb_face);

  unsigned get_size = kInitialSize;
  unsigned size = hb_ot_layout_table_get_feature_tags(
      hb_face, HB_OT_TAG_GPOS, 0, &get_size, features_.data());
  features_.resize(size);
  if (size > get_size) {
    hb_ot_layout_table_get_feature_tags(hb_face, HB_OT_TAG_GPOS, 0, &size,
                                        features_.data());
    DCHECK_EQ(size, features_.size());
  }
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_test_fonts.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

void RetrieveGlyphForStretchyOperators(const Font* operators_woff,
                                       Vector<UChar32>& vertical_glyphs,
                                       Vector<UChar32>& horizontal_glyphs) {
  DCHECK(vertical_glyphs.empty());
  DCHECK(horizontal_glyphs.empty());
  // For details, see createSizeVariants() and createStretchy() from
  // third_party/blink/web_tests/external/wpt/mathml/tools/operator-dictionary.py
  for (wtf_size_t i = 0; i < 4; ++i) {
    vertical_glyphs.push_back(operators_woff->PrimaryFont()->GlyphForCharacter(
        uchar::kPrivateUseFirst + 2 * i));
    horizontal_glyphs.push_back(
        operators_woff->PrimaryFont()->GlyphForCharacter(
            uchar::kPrivateUseFirst + 2 * i + 1));
  }
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_test_fonts.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

void retrieveGlyphForStretchyOperators(const blink::Font operatorsWoff,
                                       Vector<UChar32>& verticalGlyphs,
                                       Vector<UChar32>& horizontalGlyphs) {
  DCHECK(verticalGlyphs.empty());
  DCHECK(horizontalGlyphs.empty());
  // For details, see createSizeVariants() and createStretchy() from
  // third_party/blink/web_tests/external/wpt/mathml/tools/operator-dictionary.py
  for (unsigned i = 0; i < 4; i++) {
    verticalGlyphs.push_back(operatorsWoff.PrimaryFont()->GlyphForCharacter(
        kPrivateUseFirstCharacter + 2 * i));
    horizontalGlyphs.push_back(operatorsWoff.PrimaryFont()->GlyphForCharacter(
        kPrivateUseFirstCharacter + 2 * i + 1));
  }
}

}  // namespace blink

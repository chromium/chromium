// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_test_fonts.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription::VariantLigatures ligatures;
  Font math = test::CreateTestFont(AtomicString("MathTestFont"), data, size,
                                   1000, &ligatures);

  // HasMathData should be used by other API functions below for early return.
  // Explicitly call it here for exhaustivity, since it is fast anyway.
  OpenTypeMathSupport::HasMathData(
      math.PrimaryFont()->PlatformData().GetHarfBuzzFace());

  // There is only a small amount of math constants and each call is fast, so
  // all of these values are queried.
  for (int constant = OpenTypeMathSupport::kScriptPercentScaleDown;
       constant <= OpenTypeMathSupport::kRadicalDegreeBottomRaisePercent;
       constant++) {
    OpenTypeMathSupport::MathConstant(
        math.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
        static_cast<OpenTypeMathSupport::MathConstants>(constant));
  }

  // TODO(crbug.com/1340884): This is only testing API for three characters.
  // TODO(crbug.com/1340884): Use FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION to
  // limit the size of the vector returned by GetGlyphVariantRecords and
  // GetGlyphPartRecords?
  for (auto character : {kNAryWhiteVerticalBarCodePoint, kLeftBraceCodePoint,
                         kOverBraceCodePoint}) {
    if (auto glyph = math.PrimaryFont()->GlyphForCharacter(character)) {
      for (auto stretch_direction :
           {OpenTypeMathStretchData::StretchAxis::Horizontal,
            OpenTypeMathStretchData::StretchAxis::Vertical}) {
        Vector<OpenTypeMathStretchData::GlyphVariantRecord> variants =
            OpenTypeMathSupport::GetGlyphVariantRecords(
                math.PrimaryFont()->PlatformData().GetHarfBuzzFace(), glyph,
                stretch_direction);
        for (auto variant : variants) {
          OpenTypeMathSupport::MathItalicCorrection(
              math.PrimaryFont()->PlatformData().GetHarfBuzzFace(), variant);
        }
        float italic_correction = 0;
        OpenTypeMathSupport::GetGlyphPartRecords(
            math.PrimaryFont()->PlatformData().GetHarfBuzzFace(), glyph,
            stretch_direction, &italic_correction);
      }
    }
  }

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_test_fonts.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

constexpr float kFontSize = 1000;
constexpr float kSizeCount = 20;

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription::VariantLigatures ligatures;
  Font math = test::CreateTestFont(AtomicString("MathTestFont"), data, size,
                                   kFontSize, &ligatures);

  // TODO(crbug.com/1340884): This is only testing API for three characters.
  // TODO(crbug.com/1340884): Use FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION to
  // limit the size of the vector returned by GetGlyphVariantRecords and
  // GetGlyphPartRecords?
  for (auto character : {kNAryWhiteVerticalBarCodePoint, kLeftBraceCodePoint,
                         kOverBraceCodePoint}) {
    if (!math.PrimaryFont()->GlyphForCharacter(character))
      continue;
    StretchyOperatorShaper vertical_shaper(
        character, OpenTypeMathStretchData::StretchAxis::Vertical);
    StretchyOperatorShaper horizontal_shaper(
        character, OpenTypeMathStretchData::StretchAxis::Horizontal);
    for (unsigned i = 0; i < kSizeCount; i++) {
      StretchyOperatorShaper::Metrics metrics;
      float target_size = (i + 1) * (kFontSize / 2);
      vertical_shaper.Shape(&math, target_size, &metrics);
      horizontal_shaper.Shape(&math, target_size, &metrics);
    }
  }

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

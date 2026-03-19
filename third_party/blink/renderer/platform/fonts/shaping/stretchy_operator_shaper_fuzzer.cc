// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_test_fonts.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

constexpr float kFontSize = 1000;
constexpr float kSizeCount = 20;

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  FontDescription::VariantLigatures ligatures;
  Font* math = test::CreateTestFont(AtomicString("MathTestFont"), data,
                                    kFontSize, &ligatures);

  // TODO(crbug.com/1340884): This is only testing API for three characters.
  // TODO(crbug.com/1340884): Use FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION to
  // limit the size of the vector returned by GetGlyphVariantRecords and
  // GetGlyphPartRecords?
  for (auto character : {kNAryWhiteVerticalBarCodePoint, kLeftBraceCodePoint,
                         kOverBraceCodePoint}) {
    if (!math->PrimaryFont()->GlyphForCharacter(character)) {
      continue;
    }
    // TODO(crbug.com/432143094): Test different text directions.
    StretchyOperatorShaper vertical_shaper(
        character, OpenTypeMathStretchData::StretchAxis::Vertical,
        TextDirection::kLtr);
    StretchyOperatorShaper horizontal_shaper(
        character, OpenTypeMathStretchData::StretchAxis::Horizontal,
        TextDirection::kLtr);
    for (unsigned i = 0; i < kSizeCount; i++) {
      StretchyOperatorShaper::Metrics metrics;
      float target_size = (i + 1) * (kFontSize / 2);
      vertical_shaper.Shape(math, target_size, &metrics);
      horizontal_shaper.Shape(math, target_size, &metrics);
    }
  }

  return 0;
}

}  // namespace blink


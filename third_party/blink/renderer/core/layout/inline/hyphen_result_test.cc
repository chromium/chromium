// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/hyphen_result.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HyphenResultTest : public RenderingTest {
 protected:
  void ExpectGlyphRotation(CanvasRotationInVertical expected) {
    HyphenResult hyphen;
    hyphen.Shape(GetLayoutObjectByElementId("target")->StyleRef());

    Vector<CanvasRotationInVertical> rotations;
    hyphen.GetShapeResult().ForEachGlyph(
        0,
        [](void* context, unsigned, Glyph, gfx::Vector2dF, float, bool,
           CanvasRotationInVertical rotation, const SimpleFontData*) {
          static_cast<Vector<CanvasRotationInVertical>*>(context)->push_back(
              rotation);
        },
        &rotations);

    EXPECT_THAT(rotations, testing::ElementsAre(expected, expected));
  }
};

TEST_F(HyphenResultTest, Horizontal) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="font: 16px Ahem; writing-mode: horizontal-tb;
                            hyphenate-character: '+='"></div>
  )HTML");

  for (bool fix_enabled : {false, true}) {
    SCOPED_TRACE(testing::Message() << "fix_enabled=" << fix_enabled);
    ScopedHyphenVerticalOrientationFixForTest scoped_fix(fix_enabled);

    // In horizontal writing, "+=" is rendered without rotation.
    ExpectGlyphRotation(CanvasRotationInVertical::kRegular);
  }
}

TEST_F(HyphenResultTest, VerticalUpright) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="font: 16px Ahem; writing-mode: vertical-rl;
                            text-orientation: upright;
                            hyphenate-character: '+='"></div>
  )HTML");

  for (bool fix_enabled : {false, true}) {
    SCOPED_TRACE(testing::Message() << "fix_enabled=" << fix_enabled);
    ScopedHyphenVerticalOrientationFixForTest scoped_fix(fix_enabled);

    // With text-orientation: upright, "+=" is rendered upright.
    ExpectGlyphRotation(CanvasRotationInVertical::kRotateCanvasUpright);
  }
}

TEST_F(HyphenResultTest, VerticalMixed) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="font: 16px Ahem; writing-mode: vertical-rl;
                            text-orientation: mixed;
                            hyphenate-character: '+='"></div>
  )HTML");

  for (bool fix_enabled : {false, true}) {
    SCOPED_TRACE(testing::Message() << "fix_enabled=" << fix_enabled);
    ScopedHyphenVerticalOrientationFixForTest scoped_fix(fix_enabled);

    // With the fix enabled, "+=" is rendered sideways in vertical writing
    // with text-orientation: mixed.
    ExpectGlyphRotation(fix_enabled
                            ? CanvasRotationInVertical::kRegular
                            : CanvasRotationInVertical::kRotateCanvasUpright);
  }
}

}  // namespace blink

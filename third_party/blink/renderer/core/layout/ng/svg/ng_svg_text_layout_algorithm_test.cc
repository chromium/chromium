// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGSVGTextLayoutAlgorithmTest : public NGLayoutTest {
 public:
  NGSVGTextLayoutAlgorithmTest() : svg_text_ng_(true) {}

 private:
  ScopedSVGTextNGForTest svg_text_ng_;
};

// We had a crash in a case where connected characters are hidden.
TEST_F(NGSVGTextLayoutAlgorithmTest, PositionOnPathCrash) {
  SetBodyInnerHTML(R"HTML(
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400">
  <path fill="transparent" id="f" d="m100 200 L 300 200"/>
  <text font-size="28" textLength="400">
    <textPath xlink:href="#f">&#x633;&#x644;&#x627;&#x645;
&#xE0A;&#xE38;&#xE15;&#xE34;&#xE19;&#xE31;&#xE19;&#xE17;&#xE4C;</textPath>
  </text>
</svg>
)HTML");

  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(NGSVGTextLayoutAlgorithmTest, EmptyTextLengthCrash) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
C AxBxC
<textPath textLength="100"></textPath></text>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(NGSVGTextLayoutAlgorithmTest, EmptyTextLengthSpacingAndGlyphsCrash) {
  SetBodyInnerHTML(R"HTML(
  <svg xmlns="http://www.w3.org/2000/svg">
  <text textLength="5" lengthAdjust="spacingAndGlyphs">&zwj;<!---->&zwj;</text>
  </svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink

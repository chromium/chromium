// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SvgTextLayoutAlgorithmTest : public RenderingTest {};

// We had a crash in a case where connected characters are hidden.
TEST_F(SvgTextLayoutAlgorithmTest, PositionOnPathCrash) {
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

TEST_F(SvgTextLayoutAlgorithmTest, EmptyTextLengthCrash) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
C AxBxC
<textPath textLength="100"></textPath></text>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(SvgTextLayoutAlgorithmTest, EmptyTextLengthSpacingAndGlyphsCrash) {
  SetBodyInnerHTML(R"HTML(
  <svg xmlns="http://www.w3.org/2000/svg">
  <text textLength="5" lengthAdjust="spacingAndGlyphs">&zwj;<!---->&zwj;</text>
  </svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(SvgTextLayoutAlgorithmTest, HugeScaleCrash) {
  SetBodyInnerHTML(R"HTML(
  <svg xmlns="http://www.w3.org/2000/svg" width="450" height="450">
  <style>
  #test-body-content {
    scale: 16420065941240262705269076410170673060945878020586681613052798923953430637521913631296811416;
  }
  </style>
  <text id="test-body-content" x="-10" y="14">A</text>
  </svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

// crbug.com/1470433
TEST_F(SvgTextLayoutAlgorithmTest, ControlCharCrash) {
  SetBodyInnerHTML(R"HTML(
<style>text { white-space: pre; }</style>
<svg xmlns="http://www.w3.org/2000/svg"><text>a&#xC;d</text>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink

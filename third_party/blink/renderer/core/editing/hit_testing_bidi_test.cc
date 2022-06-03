// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HitTestingBidiTest : public EditingTestBase {};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedHitTestingBidiTest
    : public testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public HitTestingBidiTest {
 public:
  ParameterizedHitTestingBidiTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }
};

INSTANTIATE_TEST_SUITE_P(All, ParameterizedHitTestingBidiTest, testing::Bool());

// This file contains script-generated tests for PositionForPoint()
// that are related to bidirectional text. The test cases are only for
// behavior recording purposes, and do not necessarily reflect the
// correct/desired behavior.

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |C B A d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |C B A d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f C B A|
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f C B A|
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRun) {
  // Visual: |C B A
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRun) {
  // Visual: |C B A
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def|<bdo dir=\"rtl\">ABC</bdo>ghi</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">|ABC</bdo>ghi</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>ghi<bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">ghi<bdo dir=\"rtl\">ABC|</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>ghi<bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">ghi<bdo dir=\"rtl\">ABC</bdo>|def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def|<bdo dir=\"rtl\">ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC</bdo>|def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRun) {
  // Visual: |a b c
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRun) {
  // Visual: |a b c
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo "
      "dir=\"ltr\">abc</bdo>|DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF|<bdo "
      "dir=\"ltr\">abc</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>|DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">DEF|<bdo "
      "dir=\"ltr\">abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i
  // Bidi:    2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i
  // Bidi:    2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    0 0 0 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi|<bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    0 0 0 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi<bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    0 0 0 1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo>ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    0 0 0 1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo>|ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    0 0 0 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi|<bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    0 0 0 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">ghi<bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo>ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo>|ghi</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>|def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def|<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>|def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def|<bdo "
      "dir=\"rtl\">ABC</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |C B A d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |C B A d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    2 2 2 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">DEF|<bdo dir=\"ltr\">abc</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    2 2 2 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    2 2 2 3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    2 2 2 3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>|DEF</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    2 2 2 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">DEF|<bdo dir=\"ltr\">abc</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    2 2 2 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>|DEF</bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo>|GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI|<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo>|GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">GHI|<bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl|<bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo>|jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl|<bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">jkl<bdo dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo>jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo>|jkl</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>|ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi|<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>DEF</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>|ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi|<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |a b c F E D g h i
  // Bidi:    4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |a b c F E D g h i
  // Bidi:    4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|
  // Bidi:    2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|
  // Bidi:    2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\">GHI|<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>|GHI</bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\">GHI|<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>|GHI</bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>ghi</bdo>|JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo>ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL|<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>DEF</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>ghi</bdo>|JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo>ghi</bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 87;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">|abc</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 93;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">JKL|<bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc</bdo>DEF</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J m n o
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J m n o
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o L K J g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o L K J g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno|<bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc</bdo></bdo>ghi</bdo></bdo>pqr</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo>pqr</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>pqr<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">pqr<bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>pqr<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">pqr<bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo></bdo>JKL</bdo>|mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno|<bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">abc</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\">mno<bdo dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">DEF<bdo dir=\"ltr\">|abc</bdo></bdo>ghi</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc|</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo "
      "dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo></bdo>JKL</bdo>|mno</div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">PQR<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>|jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">PQR<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl|<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>|jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">|ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC|</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int x = div->OffsetLeft() + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"ltr\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl|<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l O N M
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l O N M
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M j k l I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(
    ParameterizedHitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M j k l I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\">JKL|<bdo dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>pqr<bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">pqr<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>pqr<bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">pqr<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo></bdo>|JKL</bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\">JKL|<bdo dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\">mno<bdo "
      "dir=\"rtl\">JKL<bdo dir=\"ltr\"><bdo dir=\"rtl\">DEF<bdo "
      "dir=\"ltr\">abc|</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">|abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">ghi<bdo dir=\"rtl\"><bdo "
      "dir=\"ltr\">abc</bdo>DEF</bdo></bdo>|JKL</bdo>mno</bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">PQR<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>jkl</bdo>|MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">PQR<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 147;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 153;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO|<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC</bdo>def</bdo></bdo>jkl</bdo>|MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\"><bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">GHI<bdo dir=\"ltr\"><bdo "
      "dir=\"rtl\">ABC|</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 117;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">|ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

TEST_P(ParameterizedHitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector("div");
  int text_left = div->OffsetLeft() + 300 - div->textContent().length() * 10;
  int x = text_left + 123;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ(
      "<div dir=\"rtl\"><bdo dir=\"rtl\">MNO|<bdo dir=\"ltr\">jkl<bdo "
      "dir=\"rtl\"><bdo dir=\"ltr\">def<bdo "
      "dir=\"rtl\">ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>",
      GetCaretTextFromBody(result.StartPosition()));
}

}  // namespace blink

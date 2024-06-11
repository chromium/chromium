// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"

namespace blink {

class HitTestingBidiTest : public EditingTestBase {};

// This file contains script-generated tests for PositionForPoint()
// that are related to bidirectional text. The test cases are only for
// behavior recording purposes, and do not necessarily reflect the
// correct/desired behavior.

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |C B A d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |C B A d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f C B A|
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f C B A|
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest, InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRun) {
  // Visual: |C B A
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() - 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRun) {
  // Visual: |C B A
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 3;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|
  // Bidi:    1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def|<bdo dir=\"rtl\">ABC</bdo>ghi</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">|ABC</bdo>ghi</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>ghi<bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 57;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">ghi<bdo dir=\"rtl\">ABC|</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    0 0 0 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>ghi<bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 63;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">ghi<bdo dir=\"rtl\">ABC</bdo>|def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest, InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def|<bdo dir=\"rtl\">ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest, InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    0 0 0 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr>def<bdo dir=rtl>ABC</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\">def<bdo dir=\"rtl\">|ABC</bdo></div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest, InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 27;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC|</bdo>def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest, InLtrBlockLtrBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent("<div dir=ltr><bdo dir=rtl>ABC</bdo>def</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  int x = div->OffsetLeft() + 33;
  int y = div->OffsetTop() + 5;
  const EphemeralRange result(GetDocument().caretRangeFromPoint(x, y));
  EXPECT_TRUE(result.IsNotNull());
  EXPECT_TRUE(result.IsCollapsed());
  EXPECT_EQ("<div dir=\"ltr\"><bdo dir=\"rtl\">ABC</bdo>|def</div>",
            GetCaretTextFromBody(result.StartPosition()));
}

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InLtrBlockRtlBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfOneRun) {
  // Visual: |a b c
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfOneRun) {
  // Visual: |a b c
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|
  // Bidi:    2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  d e f|C B A g h i
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  g h i C B A|d e f
  // Bidi:    2 2 2 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  d e f|C B A
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockLtrBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  C B A|d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  F E D|a b c I H G
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOfOneRunWithBaseRunEnd) {
  // Visual:  I H G a b c|F E D
  // Bidi:    1 1 1 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfOneRun) {
  // Visual:  F E D|a b c
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest, InRtlBlockRtlBaseRunRightSideOfRightEdgeOfOneRun) {
  // Visual:  a b c|F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i
  // Bidi:    2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i
  // Bidi:    2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |a b c F E D
  // Bidi:    2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|
  // Bidi:    1 1 1 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    0 0 0 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    0 0 0 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    0 0 0 1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    0 0 0 1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    0 0 0 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    0 0 0 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>ghi<bdo dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    1 1 1 2 2 2 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |C B A d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual: |C B A d e f
  // Bidi:    3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|
  // Bidi:    2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    2 2 2 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  g h i|a b c F E D j k l
  // Bidi:    2 2 2 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    2 2 2 3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  j k l F E D a b c|g h i
  // Bidi:    2 2 2 3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    2 2 2 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  g h i|a b c F E D
  // Bidi:    2 2 2 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  F E D a b c|g h i
  // Bidi:    3 3 3 4 4 4 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  I H G|C B A d e f L K J
  // Bidi:    1 1 1 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRunsWithBaseRunEnd) {
  // Visual:  L K J d e f C B A|I H G
  // Bidi:    1 1 1 2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOftwoNestedRuns) {
  // Visual:  I H G|C B A d e f
  // Bidi:    1 1 1 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOftwoNestedRuns) {
  // Visual:  d e f C B A|I H G
  // Bidi:    2 2 2 3 3 3 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |C B A d e f I H G
  // Bidi:    3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    0 0 0 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    1 1 1 2 2 2 3 3 3 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |a b c F E D g h i
  // Bidi:    4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual: |a b c F E D g h i
  // Bidi:    4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|
  // Bidi:    2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|
  // Bidi:    2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  j k l|C B A d e f I H G m n o
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  m n o I H G d e f C B A|j k l
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  j k l|C B A d e f I H G
  // Bidi:    2 2 2 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  I H G d e f C B A|j k l
  // Bidi:    3 3 3 4 4 4 5 5 5 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  L K J|a b c F E D g h i O N M
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRunsWithBaseRunEnd) {
  // Visual:  O N M g h i F E D a b c|L K J
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOfthreeNestedRuns) {
  // Visual:  L K J|a b c F E D g h i
  // Bidi:    1 1 1 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOfthreeNestedRuns) {
  // Visual:  g h i F E D a b c|L K J
  // Bidi:    2 2 2 3 3 3 4 4 4 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J m n o
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |a b c F E D g h i L K J m n o
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o L K J g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InLtrBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o L K J g h i F E D a b c|
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |a b c F E D g h i L K J
  // Bidi:    4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>pqr<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    0 0 0 1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>pqr<bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr>mno<bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InLtrBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l O N M
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual: |C B A d e f I H G j k l O N M
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M j k l I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(
    HitTestingBidiTest,
    InRtlBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M j k l I H G d e f C B A|
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual: |C B A d e f I H G j k l
  // Bidi:    5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockAtLineBoundaryRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  m n o|a b c F E D g h i L K J p q r
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo>pqr</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>pqr<bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  p q r L K J g h i F E D a b c|m n o
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>pqr<bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  m n o|a b c F E D g h i L K J
  // Bidi:    2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>mno<bdo dir=rtl>JKL<bdo "
      "dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>ghi</bdo></bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockLtrBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  L K J g h i F E D a b c|m n o
  // Bidi:    3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo>JKL</bdo>mno</bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  O N M|C B A d e f I H G j k l R Q P
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>PQR<bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRunsWithBaseRunEnd) {
  // Visual:  R Q P j k l I H G d e f C B A|O N M
  // Bidi:    1 1 1 2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo>PQR</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfLeftEdgeOffourNestedRuns) {
  // Visual:  O N M|C B A d e f I H G j k l
  // Bidi:    1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo></bdo>jkl</bdo>MNO</bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunLeftSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

TEST_F(HitTestingBidiTest,
       InRtlBlockRtlBaseRunRightSideOfRightEdgeOffourNestedRuns) {
  // Visual:  j k l I H G d e f C B A|O N M
  // Bidi:    2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  SetBodyContent(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr>jkl<bdo dir=rtl><bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo>GHI</bdo></bdo></bdo></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
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

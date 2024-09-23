// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LocalCaretRectBidiTest : public EditingTestBase {};

// This file contains script-generated tests for LocalCaretRectOfPosition()
// that are related to Bidirectional text. The test cases are only for
// behavior recording purposes, and do not necessarily reflect the
// correct/desired behavior.

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterRtlRunTouchingLineBoundary) {
  // Sample: A B C|d e f
  // Bidi:   1 1 1 0 0 0
  // Visual: C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterRtlRun) {
  // Sample: g h i A B C|d e f
  // Bidi:   0 0 0 1 1 1 0 0 0
  // Visual: g h i C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: A B C|d e f
  // Bidi:   1 1 1 0 0 0
  // Visual: C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
  // Sample: g h i A B C|d e f
  // Bidi:   0 0 0 1 1 1 0 0 0
  // Visual: g h i C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterTwoNestedRuns) {
  // Sample: D E F a b c|g h i
  // Bidi:   1 1 1 2 2 2 0 0 0
  // Visual: a b c F E D|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>|ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: D E F a b c|g h i
  // Bidi:   1 1 1 2 2 2 0 0 0
  // Visual: a b c F E D|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterThreeNestedRuns) {
  // Sample: G H I d e f A B C|j k l
  // Bidi:   1 1 1 2 2 2 3 3 3 0 0 0
  // Visual: d e f C B A I H G|j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo>|jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: G H I d e f A B C|j k l
  // Bidi:   1 1 1 2 2 2 3 3 3 0 0 0
  // Visual: d e f C B A I H G|j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|m n o
  // Bidi:   1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  // Visual: g h i a b c F E D L K J|m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></bdo></bdo>|mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: J K L g h i D E F a b c|m n o
  // Bidi:   1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  // Visual: g h i a b c F E D L K J|m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></bdo></bdo>mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeRtlRunTouchingLineBoundary) {
  // Sample: d e f|A B C
  // Bidi:   0 0 0 1 1 1
  // Visual: d e f|C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeRtlRun) {
  // Sample: d e f|A B C g h i
  // Bidi:   0 0 0 1 1 1 0 0 0
  // Visual: d e f|C B A g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: d e f|A B C
  // Bidi:   0 0 0 1 1 1
  // Visual: d e f|C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
  // Sample: d e f|A B C g h i
  // Bidi:   0 0 0 1 1 1 0 0 0
  // Visual: d e f|C B A g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeTwoNestedRuns) {
  // Sample: g h i|a b c D E F
  // Bidi:   0 0 0 2 2 2 1 1 1
  // Visual: g h i|F E D a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi|<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: g h i|a b c D E F
  // Bidi:   0 0 0 2 2 2 1 1 1
  // Visual: g h i|F E D a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeThreeNestedRuns) {
  // Sample: j k l|A B C d e f G H I
  // Bidi:   0 0 0 3 3 3 2 2 2 1 1 1
  // Visual: j k l I H G|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>jkl|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: j k l|A B C d e f G H I
  // Bidi:   0 0 0 3 3 3 2 2 2 1 1 1
  // Visual: j k l I H G|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeFourNestedRuns) {
  // Sample: m n o|a b c D E F g h i J K L
  // Bidi:   0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: m n o L K J|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>mno|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: m n o|a b c D E F g h i J K L
  // Bidi:   0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: m n o L K J|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>mno<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterLtrRunTouchingLineBoundary) {
  // Sample: a b c|D E F
  // Bidi:   2 2 2 1 1 1
  // Visual: F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterLtrRun) {
  // Sample: G H I a b c|D E F
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D a b c|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: a b c|D E F
  // Bidi:   2 2 2 1 1 1
  // Visual: F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
  // Sample: G H I a b c|D E F
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D a b c|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterTwoNestedRuns) {
  // Sample: d e f A B C|G H I
  // Bidi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>|GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: d e f A B C|G H I
  // Bidi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterThreeNestedRuns) {
  // Sample: g h i D E F a b c|J K L
  // Bidi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J g h i a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo>|JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: g h i D E F a b c|J K L
  // Bidi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J g h i a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|M N O
  // Bidi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M j k l d e f C B A I H G|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo></bdo></bdo>|MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: j k l G H I d e f A B C|M N O
  // Bidi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M j k l d e f C B A I H G|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></bdo></bdo>MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeLtrRunTouchingLineBoundary) {
  // Sample: D E F|a b c
  // Bidi:   1 1 1 2 2 2
  // Visual:|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeLtrRun) {
  // Sample: D E F|a b c G H I
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: D E F|a b c
  // Bidi:   1 1 1 2 2 2
  // Visual:|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
  // Sample: D E F|a b c G H I
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeTwoNestedRuns) {
  // Sample: G H I|A B C d e f
  // Bidi:   1 1 1 3 3 3 2 2 2
  // Visual:|C B A d e f I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI|<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: G H I|A B C d e f
  // Bidi:   1 1 1 3 3 3 2 2 2
  // Visual:|C B A d e f I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeThreeNestedRuns) {
  // Sample: J K L|a b c D E F g h i
  // Bidi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual:|F E D a b c g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>JKL|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: J K L|a b c D E F g h i
  // Bidi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual:|F E D a b c g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeFourNestedRuns) {
  // Sample: M N O|A B C d e f G H I j k l
  // Bidi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G|C B A d e f j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>MNO|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: M N O|A B C d e f G H I j k l
  // Bidi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G|C B A d e f j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterRtlRunTouchingLineBoundary) {
  // Sample: A B C|d e f
  // Bidi:   3 3 3 2 2 2
  // Visual:|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterRtlRun) {
  // Sample: g h i A B C|d e f
  // Bidi:   2 2 2 3 3 3 2 2 2
  // Visual: g h i|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: A B C|d e f
  // Bidi:   3 3 3 2 2 2
  // Visual:|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
  // Sample: g h i A B C|d e f
  // Bidi:   2 2 2 3 3 3 2 2 2
  // Visual: g h i|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterTwoNestedRuns) {
  // Sample: D E F a b c|g h i
  // Bidi:   3 3 3 4 4 4 2 2 2
  // Visual:|a b c F E D g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>|ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: D E F a b c|g h i
  // Bidi:   3 3 3 4 4 4 2 2 2
  // Visual:|a b c F E D g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterThreeNestedRuns) {
  // Sample: G H I d e f A B C|j k l
  // Bidi:   3 3 3 4 4 4 5 5 5 2 2 2
  // Visual:|d e f C B A I H G j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo>|jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: G H I d e f A B C|j k l
  // Bidi:   3 3 3 4 4 4 5 5 5 2 2 2
  // Visual:|d e f C B A I H G j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|m n o
  // Bidi:   3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  // Visual:|g h i a b c F E D L K J m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></bdo></bdo>|mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: J K L g h i D E F a b c|m n o
  // Bidi:   3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  // Visual:|g h i a b c F E D L K J m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></bdo></bdo>mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeRtlRunTouchingLineBoundary) {
  // Sample: d e f|A B C
  // Bidi:   2 2 2 3 3 3
  // Visual: d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeRtlRun) {
  // Sample: d e f|A B C g h i
  // Bidi:   2 2 2 3 3 3 2 2 2
  // Visual: d e f C B A|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: d e f|A B C
  // Bidi:   2 2 2 3 3 3
  // Visual: d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
  // Sample: d e f|A B C g h i
  // Bidi:   2 2 2 3 3 3 2 2 2
  // Visual: d e f C B A|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeTwoNestedRuns) {
  // Sample: g h i|a b c D E F
  // Bidi:   2 2 2 4 4 4 3 3 3
  // Visual: g h i F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi|<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: g h i|a b c D E F
  // Bidi:   2 2 2 4 4 4 3 3 3
  // Visual: g h i F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeThreeNestedRuns) {
  // Sample: j k l|A B C d e f G H I
  // Bidi:   2 2 2 5 5 5 4 4 4 3 3 3
  // Visual: j k l I H G C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>jkl|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: j k l|A B C d e f G H I
  // Bidi:   2 2 2 5 5 5 4 4 4 3 3 3
  // Visual: j k l I H G C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeFourNestedRuns) {
  // Sample: m n o|a b c D E F g h i J K L
  // Bidi:   2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  // Visual: m n o L K J F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>mno|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: m n o|a b c D E F g h i J K L
  // Bidi:   2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  // Visual: m n o L K J F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterLtrRunTouchingLineBoundary) {
  // Sample: a b c|D E F
  // Bidi:   2 2 2 1 1 1
  // Visual: F E D|a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterLtrRun) {
  // Sample: G H I a b c|D E F
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D|a b c I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: a b c|D E F
  // Bidi:   2 2 2 1 1 1
  // Visual: F E D|a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
  // Sample: G H I a b c|D E F
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D|a b c I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterTwoNestedRuns) {
  // Sample: d e f A B C|G H I
  // Bidi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G|d e f C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>|GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: d e f A B C|G H I
  // Bidi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G|d e f C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterThreeNestedRuns) {
  // Sample: g h i D E F a b c|J K L
  // Bidi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J|g h i a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo>|JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: g h i D E F a b c|J K L
  // Bidi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J|g h i a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|M N O
  // Bidi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M|j k l d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo></bdo></bdo>|MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: j k l G H I d e f A B C|M N O
  // Bidi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M|j k l d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></bdo></bdo>MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeLtrRunTouchingLineBoundary) {
  // Sample: D E F|a b c
  // Bidi:   1 1 1 2 2 2
  // Visual: a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeLtrRun) {
  // Sample: D E F|a b c G H I
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: D E F|a b c
  // Bidi:   1 1 1 2 2 2
  // Visual: a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
  // Sample: D E F|a b c G H I
  // Bidi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeTwoNestedRuns) {
  // Sample: G H I|A B C d e f
  // Bidi:   1 1 1 3 3 3 2 2 2
  // Visual: C B A d e f|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI|<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: G H I|A B C d e f
  // Bidi:   1 1 1 3 3 3 2 2 2
  // Visual: C B A d e f|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeThreeNestedRuns) {
  // Sample: J K L|a b c D E F g h i
  // Bidi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual: F E D a b c|g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>JKL|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: J K L|a b c D E F g h i
  // Bidi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual: F E D a b c|g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeFourNestedRuns) {
  // Sample: M N O|A B C d e f G H I j k l
  // Bidi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G C B A d e f|j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>MNO|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: M N O|A B C d e f G H I j k l
  // Bidi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G C B A d e f|j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineBeginLtrBaseRunWithTwoNestedRuns) {
  // Sample:|A B C d e f
  // Bidi:   1 1 1 0 0 0
  // Visual:|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLineBeginLtrBaseRunWithThreeNestedRuns) {
  // Sample:|a b c D E F g h i
  // Bidi:   2 2 2 1 1 1 0 0 0
  // Visual:|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLineBeginLtrBaseRunWithFourNestedRuns) {
  // Sample:|A B C d e f G H I j k l
  // Bidi:   3 3 3 2 2 2 1 1 1 0 0 0
  // Visual: I H G|C B A d e f j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndLtrBaseRunWithTwoNestedRuns) {
  // Sample: d e f A B C|
  // Bidi:   0 0 0 1 1 1
  // Visual: d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndLtrBaseRunWithThreeNestedRuns) {
  // Sample: g h i D E F a b c|
  // Bidi:   0 0 0 1 1 1 2 2 2
  // Visual: g h i a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndLtrBaseRunWithFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|
  // Bidi:   0 0 0 1 1 1 2 2 2 3 3 3
  // Visual: j k l d e f C B A|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineBeginWithRtlRunOnly) {
  // Sample:|A B C
  // Bidi:   1 1 1
  // Visual:|C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position =
      SetCaretTextToBody("<div dir=ltr><bdo dir=rtl>|ABC</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineBeginRtlBaseRunWithTwoNestedRuns) {
  // Sample:|a b c D E F
  // Bidi:   2 2 2 1 1 1
  // Visual:|F E D a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLineBeginRtlBaseRunWithThreeNestedRuns) {
  // Sample:|A B C d e f G H I
  // Bidi:   3 3 3 2 2 2 1 1 1
  // Visual: I H G|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InLtrBlockLineBeginRtlBaseRunWithFourNestedRuns) {
  // Sample:|a b c D E F g h i J K L
  // Bidi:   4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: L K J|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndWithRtlRunOnly) {
  // Sample: A B C|
  // Bidi:   1 1 1
  // Visual: C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position =
      SetCaretTextToBody("<div dir=ltr><bdo dir=rtl>ABC|</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndRtlBaseRunWithTwoNestedRuns) {
  // Sample: D E F a b c|
  // Bidi:   1 1 1 2 2 2
  // Visual: a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndRtlBaseRunWithThreeNestedRuns) {
  // Sample: G H I d e f A B C|
  // Bidi:   1 1 1 2 2 2 3 3 3
  // Visual: d e f C B A|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InLtrBlockLineEndRtlBaseRunWithFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|
  // Bidi:   1 1 1 2 2 2 3 3 3 4 4 4
  // Visual: g h i a b c F E D|L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineBeginWithLtrRunOnly) {
  // Sample:|a b c
  // Bidi:   2 2 2
  // Visual: a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position =
      SetCaretTextToBody("<div dir=rtl><bdo dir=ltr>|abc</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineBeginLtrBaseRunWithTwoNestedRuns) {
  // Sample:|A B C d e f
  // Bidi:   3 3 3 2 2 2
  // Visual: C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLineBeginLtrBaseRunWithThreeNestedRuns) {
  // Sample:|a b c D E F g h i
  // Bidi:   4 4 4 3 3 3 2 2 2
  // Visual: F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLineBeginLtrBaseRunWithFourNestedRuns) {
  // Sample:|A B C d e f G H I j k l
  // Bidi:   5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G C B A d e f|j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndWithLtrRunOnly) {
  // Sample: a b c|
  // Bidi:   2 2 2
  // Visual:|a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position =
      SetCaretTextToBody("<div dir=rtl><bdo dir=ltr>abc|</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndLtrBaseRunWithTwoNestedRuns) {
  // Sample: d e f A B C|
  // Bidi:   2 2 2 3 3 3
  // Visual:|d e f C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndLtrBaseRunWithThreeNestedRuns) {
  // Sample: g h i D E F a b c|
  // Bidi:   2 2 2 3 3 3 4 4 4
  // Visual: g h i|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndLtrBaseRunWithFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|
  // Bidi:   2 2 2 3 3 3 4 4 4 5 5 5
  // Visual: j k l|d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineBeginRtlBaseRunWithTwoNestedRuns) {
  // Sample:|a b c D E F
  // Bidi:   2 2 2 1 1 1
  // Visual: F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLineBeginRtlBaseRunWithThreeNestedRuns) {
  // Sample:|A B C d e f G H I
  // Bidi:   3 3 3 2 2 2 1 1 1
  // Visual: I H G C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest,
       InRtlBlockLineBeginRtlBaseRunWithFourNestedRuns) {
  // Sample:|a b c D E F g h i J K L
  // Bidi:   4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: L K J F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndRtlBaseRunWithTwoNestedRuns) {
  // Sample: D E F a b c|
  // Bidi:   1 1 1 2 2 2
  // Visual:|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndRtlBaseRunWithThreeNestedRuns) {
  // Sample: G H I d e f A B C|
  // Bidi:   1 1 1 2 2 2 3 3 3
  // Visual:|d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBidiTest, InRtlBlockLineEndRtlBaseRunWithFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|
  // Bidi:   1 1 1 2 2 2 3 3 3 4 4 4
  // Visual: g h i|a b c F E D L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

}  // namespace blink

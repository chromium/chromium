// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LocalCaretRectBidiTest : public EditingTestBase {};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLocalCaretRectBidiTest
    : public testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public LocalCaretRectBidiTest {
 public:
  ParameterizedLocalCaretRectBidiTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedLocalCaretRectBidiTest,
                         testing::Bool());

// This file contains script-generated tests for LocalCaretRectOfPosition()
// that are related to Bidirectional text. The test cases are only for
// behavior recording purposes, and do not necessarily reflect the
// correct/desired behavior.

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockLtrBaseRunAfterRtlRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterThreeNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(60, 0, 1, 10)
                              : PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(60, 0, 1, 10)
                              : PhysicalRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunAfterFourNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(90, 0, 1, 10)
                              : PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(90, 0, 1, 10)
                              : PhysicalRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockLtrBaseRunBeforeRtlRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeThreeNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(30, 0, 1, 10)
                              : PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(30, 0, 1, 10)
                              : PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLtrBaseRunBeforeFourNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(30, 0, 1, 10)
                              : PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(30, 0, 1, 10)
                              : PhysicalRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockRtlBaseRunAfterLtrRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunAfterFourNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  EXPECT_EQ(PhysicalRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockRtlBaseRunBeforeLtrRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockRtlBaseRunBeforeFourNestedRuns) {
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
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockLtrBaseRunAfterRtlRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunAfterFourNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(180, 0, 1, 10)
                              : PhysicalRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(180, 0, 1, 10)
                              : PhysicalRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockLtrBaseRunBeforeRtlRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLtrBaseRunBeforeFourNestedRuns) {
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(299, 0, 1, 10)
                              : PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  // TODO(xiaochengh): Decide if the behavior difference is worth to fix.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(299, 0, 1, 10)
                              : PhysicalRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockRtlBaseRunAfterLtrRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  EXPECT_EQ(PhysicalRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunAfterFourNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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
  EXPECT_EQ(PhysicalRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockRtlBaseRunBeforeLtrRun) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeThreeNestedRuns) {
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
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockRtlBaseRunBeforeFourNestedRuns) {
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
  EXPECT_EQ(PhysicalRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineBeginLtrBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndLtrBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndLtrBaseRunWithThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndLtrBaseRunWithFourNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockLineBeginWithRtlRunOnly) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineBeginRtlBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InLtrBlockLineEndWithRtlRunOnly) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndRtlBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndRtlBaseRunWithThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InLtrBlockLineEndRtlBaseRunWithFourNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockLineBeginWithLtrRunOnly) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineBeginLtrBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest, InRtlBlockLineEndWithLtrRunOnly) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndLtrBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndLtrBaseRunWithThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndLtrBaseRunWithFourNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineBeginRtlBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndRtlBaseRunWithTwoNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndRtlBaseRunWithThreeNestedRuns) {
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

TEST_P(ParameterizedLocalCaretRectBidiTest,
       InRtlBlockLineEndRtlBaseRunWithFourNestedRuns) {
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

// Smoke tests for Position-based word navigation overloads. Comprehensive word
// navigation tests are in visible_units_word_test.cc.
class PositionUnitsWordTest : public EditingTestBase {};

TEST_F(PositionUnitsWordTest, StartOfWordPositionBasic) {
  SetBodyContent("<div id=sample>hello world</div>");
  Node* text = GetElementById("sample")->firstChild();
  EXPECT_EQ(Position(*text, 6), StartOfWordPosition(Position(*text, 8)));
  EXPECT_EQ(Position(*text, 0), StartOfWordPosition(Position(*text, 3)));
}

TEST_F(PositionUnitsWordTest, EndOfWordPositionBasic) {
  SetBodyContent("<div id=sample>hello world</div>");
  Node* text = GetElementById("sample")->firstChild();
  EXPECT_EQ(Position(*text, 5), EndOfWordPosition(Position(*text, 0)));
  EXPECT_EQ(Position(*text, 11), EndOfWordPosition(Position(*text, 8)));
}

TEST_F(PositionUnitsWordTest, NextWordPositionBasic) {
  SetBodyContent("<div id=sample>hello world</div>");
  Node* text = GetElementById("sample")->firstChild();
  PositionWithAffinity result = NextWordPosition(Position(*text, 0));
  EXPECT_EQ(Position(*text, 5), result.GetPosition());
}

TEST_F(PositionUnitsWordTest, PreviousWordPositionBasic) {
  SetBodyContent("<div id=sample>hello world</div>");
  Node* text = GetElementById("sample")->firstChild();
  PositionWithAffinity result = PreviousWordPosition(Position(*text, 11));
  EXPECT_EQ(Position(*text, 6), result.GetPosition());
}

}  // namespace blink

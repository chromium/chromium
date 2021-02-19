// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class InlineTextBoxTest : public RenderingTest {};

class TestInlineTextBox : public InlineTextBox {
 public:
  TestInlineTextBox(LineLayoutItem item) : InlineTextBox(item, 0, 0) {
    SetHasVirtualLogicalHeight();
  }

  static TestInlineTextBox* Create(Document& document, const String& string) {
    Text* node = document.createTextNode(string);
    LayoutText* text = new LayoutText(node, string.Impl());
    text->SetStyle(ComputedStyle::Create());
    return new TestInlineTextBox(LineLayoutItem(text));
  }

  LayoutUnit VirtualLogicalHeight() const override { return logical_height_; }

  void SetLogicalFrameRect(const LayoutRect& rect) {
    SetX(rect.X());
    SetY(rect.Y());
    SetLogicalWidth(rect.Width());
    logical_height_ = rect.Height();
  }

 private:
  LayoutUnit logical_height_;
};

static void MoveAndTest(InlineTextBox* box,
                        const LayoutSize& move,
                        LayoutRect& frame,
                        LayoutRect& overflow) {
  box->Move(move);
  frame.Move(move);
  overflow.Move(move);
  ASSERT_EQ(frame, box->LogicalFrameRect());
  ASSERT_EQ(overflow, box->LogicalOverflowRect());
}

TEST_F(InlineTextBoxTest, LogicalOverflowRect) {
  // Setup a TestInlineTextBox.
  TestInlineTextBox* box = TestInlineTextBox::Create(GetDocument(), "");

  // Initially, logicalOverflowRect() should be the same as logicalFrameRect().
  LayoutRect frame(5, 20, 100, 200);
  LayoutRect overflow = frame;
  box->SetLogicalFrameRect(frame);
  ASSERT_EQ(frame, box->LogicalFrameRect());
  ASSERT_EQ(overflow, box->LogicalOverflowRect());

  // Ensure it's movable and the rects are correct.
  LayoutSize move(10, 10);
  MoveAndTest(box, move, frame, overflow);

  // Ensure clearKnownToHaveNoOverflow() doesn't change either rects.
  box->ClearKnownToHaveNoOverflow();
  ASSERT_EQ(frame, box->LogicalFrameRect());
  ASSERT_EQ(overflow, box->LogicalOverflowRect());

  // Ensure it's still movable correctly when !knownToHaveNoOverflow().
  MoveAndTest(box, move, frame, overflow);

  // Let it have different logicalOverflowRect() than logicalFrameRect().
  overflow.Expand(LayoutSize(10, 10));
  box->SetLogicalOverflowRect(overflow);
  ASSERT_EQ(frame, box->LogicalFrameRect());
  ASSERT_EQ(overflow, box->LogicalOverflowRect());

  // Ensure it's still movable correctly.
  MoveAndTest(box, move, frame, overflow);
}

}  // namespace blink

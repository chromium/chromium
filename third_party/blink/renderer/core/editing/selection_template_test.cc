// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_template.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class SelectionTest : public EditingTestBase {};

TEST_F(SelectionTest, defaultConstructor) {
  SelectionInDOMTree selection;

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsAnchorFirst());
  EXPECT_TRUE(selection.IsNone());
  EXPECT_EQ(Position(), selection.Anchor());
  EXPECT_EQ(Position(), selection.Focus());
  EXPECT_EQ(EphemeralRange(), selection.ComputeRange());
}

TEST_F(SelectionTest, IsAnchorFirst) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  Position base(Position(sample->firstChild(), 4));
  Position extent(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(base);
  builder.Extend(extent);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_FALSE(selection.IsAnchorFirst());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(base, selection.Anchor());
  EXPECT_EQ(extent, selection.Focus());
}

TEST_F(SelectionTest, caret) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  Position position(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(position);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsAnchorFirst());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(position, selection.Anchor());
  EXPECT_EQ(position, selection.Focus());
}

TEST_F(SelectionTest, range) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  Position base(Position(sample->firstChild(), 2));
  Position extent(Position(sample->firstChild(), 4));
  SelectionInDOMTree::Builder builder;
  builder.Collapse(base);
  builder.Extend(extent);
  const SelectionInDOMTree& selection = builder.Build();

  EXPECT_EQ(TextAffinity::kDownstream, selection.Affinity());
  EXPECT_TRUE(selection.IsAnchorFirst());
  EXPECT_FALSE(selection.IsNone());
  EXPECT_EQ(base, selection.Anchor());
  EXPECT_EQ(extent, selection.Focus());
}

TEST_F(SelectionTest, SetAsBacwardAndForward) {
  SetBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  Position start(Position(sample->firstChild(), 2));
  Position end(Position(sample->firstChild(), 4));
  EphemeralRange range(start, end);
  const SelectionInDOMTree& backward_selection =
      SelectionInDOMTree::Builder().SetAsBackwardSelection(range).Build();
  const SelectionInDOMTree& forward_selection =
      SelectionInDOMTree::Builder().SetAsForwardSelection(range).Build();
  const SelectionInDOMTree& collapsed_selection =
      SelectionInDOMTree::Builder()
          .SetAsForwardSelection(EphemeralRange(start))
          .Build();

  EXPECT_EQ(TextAffinity::kDownstream, backward_selection.Affinity());
  EXPECT_FALSE(backward_selection.IsAnchorFirst());
  EXPECT_FALSE(backward_selection.IsNone());
  EXPECT_EQ(end, backward_selection.Anchor());
  EXPECT_EQ(start, backward_selection.Focus());
  EXPECT_EQ(start, backward_selection.ComputeStartPosition());
  EXPECT_EQ(end, backward_selection.ComputeEndPosition());
  EXPECT_EQ(range, backward_selection.ComputeRange());

  EXPECT_EQ(TextAffinity::kDownstream, forward_selection.Affinity());
  EXPECT_TRUE(forward_selection.IsAnchorFirst());
  EXPECT_FALSE(forward_selection.IsNone());
  EXPECT_EQ(start, forward_selection.Anchor());
  EXPECT_EQ(end, forward_selection.Focus());
  EXPECT_EQ(start, forward_selection.ComputeStartPosition());
  EXPECT_EQ(end, forward_selection.ComputeEndPosition());
  EXPECT_EQ(range, forward_selection.ComputeRange());

  EXPECT_EQ(TextAffinity::kDownstream, collapsed_selection.Affinity());
  EXPECT_TRUE(collapsed_selection.IsAnchorFirst());
  EXPECT_FALSE(collapsed_selection.IsNone());
  EXPECT_EQ(start, collapsed_selection.Anchor());
  EXPECT_EQ(start, collapsed_selection.Focus());
  EXPECT_EQ(start, collapsed_selection.ComputeStartPosition());
  EXPECT_EQ(start, collapsed_selection.ComputeEndPosition());
  EXPECT_EQ(EphemeralRange(start, start), collapsed_selection.ComputeRange());
}

TEST_F(SelectionTest, EquivalentPositions) {
  SetBodyContent(
      "<div id='first'></div>"
      "<div id='last'></div>");
  Element* first = GetDocument().getElementById(AtomicString("first"));
  Element* last = GetDocument().getElementById(AtomicString("last"));
  Position after_first = Position::AfterNode(*first);
  Position before_last = Position::BeforeNode(*last);

  // Test selections created with different but equivalent positions.
  EXPECT_NE(after_first, before_last);
  EXPECT_TRUE(after_first.IsEquivalent(before_last));

  for (bool reversed : {false, true}) {
    const Position& start = reversed ? before_last : after_first;
    const Position& end = reversed ? after_first : before_last;
    EphemeralRange range(start, end);

    const SelectionInDOMTree& selection =
        SelectionInDOMTree::Builder().Collapse(start).Extend(end).Build();
    EXPECT_EQ(
        selection,
        SelectionInDOMTree::Builder().SetAsForwardSelection(range).Build());

    EXPECT_TRUE(selection.IsCaret());
    EXPECT_EQ(range, selection.ComputeRange());
    EXPECT_EQ(start, selection.Anchor());
    EXPECT_EQ(start, selection.Focus());
  }
}

}  // namespace blink

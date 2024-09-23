// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/relocatable_position.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class RelocatablePositionTest : public EditingTestBase {};

TEST_F(RelocatablePositionTest, position) {
  SetBodyContent("<b>foo</b><textarea>bar</textarea>");
  Node* boldface = GetDocument().QuerySelector(AtomicString("b"));
  Node* textarea = GetDocument().QuerySelector(AtomicString("textarea"));

  Position position(textarea, PositionAnchorType::kBeforeAnchor);
  RelocatablePosition* relocatable_position =
      MakeGarbageCollected<RelocatablePosition>(position);
  EXPECT_EQ(position, relocatable_position->GetPosition());

  textarea->remove();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // RelocatablePosition should track the given Position even if its original
  // anchor node is moved away from the document.
  Position expected_position(boldface, PositionAnchorType::kAfterAnchor);
  Position tracked_position = relocatable_position->GetPosition();
  EXPECT_TRUE(tracked_position.AnchorNode()->isConnected());
  EXPECT_EQ(CreateVisiblePosition(expected_position).DeepEquivalent(),
            CreateVisiblePosition(tracked_position).DeepEquivalent());
}

TEST_F(RelocatablePositionTest, positionAnchorTypes) {
  SetBodyContent("<div>text</div>");
  Node* node = GetDocument().QuerySelector(AtomicString("div"));

  Position before(node, PositionAnchorType::kBeforeAnchor);
  Position offset0(node, 0);
  Position offset1(node, 1);
  Position after_children(node, PositionAnchorType::kAfterChildren);
  Position after(node, PositionAnchorType::kAfterAnchor);

  RelocatablePosition* relocatable_before =
      MakeGarbageCollected<RelocatablePosition>(before);
  RelocatablePosition* relocatable_offset0 =
      MakeGarbageCollected<RelocatablePosition>(offset0);
  RelocatablePosition* relocatable_offset1 =
      MakeGarbageCollected<RelocatablePosition>(offset1);
  RelocatablePosition* relocatable_after_children =
      MakeGarbageCollected<RelocatablePosition>(after_children);
  RelocatablePosition* relocatable_after =
      MakeGarbageCollected<RelocatablePosition>(after);

  EXPECT_EQ(before, relocatable_before->GetPosition());
  EXPECT_EQ(offset0, relocatable_offset0->GetPosition());
  EXPECT_EQ(offset1, relocatable_offset1->GetPosition());
  EXPECT_EQ(after_children, relocatable_after_children->GetPosition());
  EXPECT_EQ(after, relocatable_after->GetPosition());

  node->insertBefore(Text::Create(GetDocument(), "["), node->firstChild());
  Position offset2(node, 2);
  RelocatablePosition* relocatable_offset2 =
      MakeGarbageCollected<RelocatablePosition>(offset2);

  EXPECT_EQ(before, relocatable_before->GetPosition());
  EXPECT_EQ(offset0, relocatable_offset0->GetPosition());
  EXPECT_EQ(offset2, relocatable_offset1->GetPosition());
  EXPECT_EQ(offset2, relocatable_offset2->GetPosition());
  EXPECT_EQ(after_children, relocatable_after_children->GetPosition());
  EXPECT_EQ(after, relocatable_after->GetPosition());

  node->appendChild(Text::Create(GetDocument(), "]"));
  Position offset3(node, 3);
  RelocatablePosition* relocatable_offset3 =
      MakeGarbageCollected<RelocatablePosition>(offset3);

  EXPECT_EQ(before, relocatable_before->GetPosition());
  EXPECT_EQ(offset0, relocatable_offset0->GetPosition());
  EXPECT_EQ(offset2, relocatable_offset1->GetPosition());
  EXPECT_EQ(offset2, relocatable_offset2->GetPosition());
  EXPECT_EQ(offset3, relocatable_offset3->GetPosition());
  EXPECT_EQ(offset2, relocatable_after_children->GetPosition());
  EXPECT_EQ(after, relocatable_after->GetPosition());
}

}  // namespace blink

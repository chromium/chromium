// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

using PaintInvalidation = LocalFrameView::ObjectPaintInvalidation;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class CaretDisplayItemClientTest : public PaintAndRasterInvalidationTest {
 protected:
  void SetUp() override {
    PaintAndRasterInvalidationTest::SetUp();
    Selection().SetCaretBlinkingSuspended(true);
  }

  FrameSelection& Selection() const {
    return GetDocument().View()->GetFrame().Selection();
  }

  const DisplayItemClient& GetCaretDisplayItemClient() const {
    return Selection().CaretDisplayItemClientForTesting();
  }

  const LayoutBlock* CaretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(
               GetCaretDisplayItemClient())
        .layout_block_;
  }

  const LayoutBlock* PreviousCaretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(
               GetCaretDisplayItemClient())
        .previous_layout_block_;
  }

  Text* AppendTextNode(const String& data) {
    Text* text = GetDocument().createTextNode(data);
    GetDocument().body()->AppendChild(text);
    return text;
  }

  Element* AppendBlock(const String& data) {
    Element* block = GetDocument().CreateRawElement(html_names::kDivTag);
    Text* text = GetDocument().createTextNode(data);
    block->AppendChild(text);
    GetDocument().body()->AppendChild(block);
    return block;
  }

  void UpdateAllLifecyclePhasesForCaretTest() {
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean();
    UpdateAllLifecyclePhasesForTest();
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CaretDisplayItemClientTest);

TEST_P(CaretDisplayItemClientTest, CaretPaintInvalidation) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhasesForCaretTest();
  const auto* block = To<LayoutBlock>(GetDocument().body()->GetLayoutObject());

  // Focus the body. Should invalidate the new caret.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().body()->focus();
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(block->ShouldPaintCursorCaret());

  auto caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect.Width());
  EXPECT_EQ(block->Location(), LayoutPoint(caret_visual_rect.Location()));

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &GetCaretDisplayItemClient(), "Caret", caret_visual_rect,
                  PaintInvalidationReason::kAppeared}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Move the caret to the end of the text. Should invalidate both the old and
  // new carets.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(block->ShouldPaintCursorCaret());

  auto new_caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(caret_visual_rect.Size(), new_caret_visual_rect.Size());
  EXPECT_EQ(caret_visual_rect.Y(), new_caret_visual_rect.Y());
  EXPECT_LT(caret_visual_rect.X(), new_caret_visual_rect.X());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect,
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         new_caret_visual_rect,
                                         PaintInvalidationReason::kCaret}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Remove selection. Should invalidate the old caret.
  auto old_caret_visual_rect = new_caret_visual_rect;
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_FALSE(block->ShouldPaintCursorCaret());
  EXPECT_EQ(IntRect(), GetCaretDisplayItemClient().VisualRect());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &GetCaretDisplayItemClient(), "Caret", old_caret_visual_rect,
                  PaintInvalidationReason::kDisappeared}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, CaretMovesBetweenBlocks) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhasesForTest();
  auto* block1 = To<LayoutBlockFlow>(block_element1->GetLayoutObject());
  auto* block2 = To<LayoutBlockFlow>(block_element2->GetLayoutObject());

  // Focus the body.
  GetDocument().body()->focus();
  UpdateAllLifecyclePhasesForCaretTest();
  auto caret_visual_rect1 = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect1.Width());
  EXPECT_EQ(block1->FirstFragment().VisualRect().Location(),
            LayoutPoint(caret_visual_rect1.Location()));
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());

  // Move the caret into block2. Should invalidate both the old and new carets.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  UpdateAllLifecyclePhasesForTest();

  auto caret_visual_rect2 = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect2.Width());
  EXPECT_EQ(block2->FirstFragment().VisualRect().Location(),
            caret_visual_rect2.Location());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect1,
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect2,
                                         PaintInvalidationReason::kCaret}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret},
                  PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Move the caret back into block1.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  UpdateAllLifecyclePhasesForCaretTest();

  EXPECT_EQ(caret_visual_rect1, GetCaretDisplayItemClient().VisualRect());
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect1,
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect2,
                                         PaintInvalidationReason::kCaret}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret},
                  PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, UpdatePreviousLayoutBlock) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhasesForCaretTest();
  auto* block1 = To<LayoutBlock>(block_element1->GetLayoutObject());
  auto* block2 = To<LayoutBlock>(block_element2->GetLayoutObject());

  // Set caret into block2.
  GetDocument().body()->focus();
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Move caret into block1. Should set previousCaretLayoutBlock to block2.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_EQ(block1, CaretLayoutBlock());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Move caret into block2. Partial update should not change
  // previousCaretLayoutBlock.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Remove block2. Should clear caretLayoutBlock and previousCaretLayoutBlock.
  block_element2->parentNode()->RemoveChild(block_element2);
  EXPECT_FALSE(CaretLayoutBlock());
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Set caret into block1.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  UpdateAllLifecyclePhasesForCaretTest();
  // Remove selection.
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_EQ(block1, PreviousCaretLayoutBlock());
}

TEST_P(CaretDisplayItemClientTest, CaretHideMoveAndShow) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  GetDocument().body()->focus();
  UpdateAllLifecyclePhasesForCaretTest();
  const auto* block = To<LayoutBlock>(GetDocument().body()->GetLayoutObject());

  auto caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect.Width());
  EXPECT_EQ(block->Location(), caret_visual_rect.Location());

  GetDocument().View()->SetTracksPaintInvalidations(true);
  // Simulate that the blinking cursor becomes invisible.
  Selection().SetCaretVisible(false);
  // Move the caret to the end of the text.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());
  // Simulate that the cursor blinking is restarted.
  Selection().SetCaretVisible(true);
  UpdateAllLifecyclePhasesForCaretTest();

  auto new_caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(caret_visual_rect.Size(), new_caret_visual_rect.Size());
  EXPECT_EQ(caret_visual_rect.Y(), new_caret_visual_rect.Y());
  EXPECT_LT(caret_visual_rect.X(), new_caret_visual_rect.X());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         caret_visual_rect,
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{&GetCaretDisplayItemClient(), "Caret",
                                         new_caret_visual_rect,
                                         PaintInvalidationReason::kCaret}));
  EXPECT_THAT(
      *GetDocument().View()->TrackedObjectPaintInvalidations(),
      ElementsAre(PaintInvalidation{"Caret", PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, CompositingChange) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #container { position: absolute; top: 55px; left: 66px; }"
      "</style>"
      "<div id='container'>"
      "  <div id='editor' contenteditable style='padding: 50px'>ABCDE</div>"
      "</div>");

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* container = GetDocument().getElementById("container");
  auto* editor = GetDocument().getElementById("editor");
  auto* editor_block = To<LayoutBlock>(editor->GetLayoutObject());
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(editor, 0)).Build());
  UpdateAllLifecyclePhasesForCaretTest();

  EXPECT_TRUE(editor_block->ShouldPaintCursorCaret());
  EXPECT_EQ(editor_block, CaretLayoutBlock());
  EXPECT_EQ(IntRect(116, 105, 1, 1), GetCaretDisplayItemClient().VisualRect());

  // Composite container.
  container->setAttribute(html_names::kStyleAttr, "will-change: transform");
  UpdateAllLifecyclePhasesForCaretTest();
  // TODO(wangxianzhu): Why will-change:transform doens't trigger compositing
  // in CAP?
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(IntRect(50, 50, 1, 1), GetCaretDisplayItemClient().VisualRect());
  }

  // Uncomposite container.
  container->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(IntRect(116, 105, 1, 1), GetCaretDisplayItemClient().VisualRect());
}

class ParameterizedComputeCaretRectTest
    : public EditingTestBase,
      private ScopedLayoutNGForTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedComputeCaretRectTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  PhysicalRect ComputeCaretRect(const PositionWithAffinity& position) const {
    return CaretDisplayItemClient::ComputeCaretRectAndPainterBlock(position)
        .caret_rect;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedComputeCaretRectTest,
                         testing::Bool());

TEST_P(ParameterizedComputeCaretRectTest, CaretRectAfterEllipsisNoCrash) {
  SetBodyInnerHTML(
      "<style>pre{width:30px; overflow:hidden; text-overflow:ellipsis}</style>"
      "<pre id=target>long long long long long long text</pre>");
  const Node* text = GetElementById("target")->firstChild();
  const Position position = Position::LastPositionInNode(*text);
  // Shouldn't crash inside. The actual result doesn't matter and may change.
  ComputeCaretRect(PositionWithAffinity(position));
}

}  // namespace blink

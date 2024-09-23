// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/picture_matchers.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

#define ASSERT_CARET_LAYER()                                       \
  do {                                                             \
    ASSERT_TRUE(CaretLayer());                                     \
    EXPECT_EQ(SkColors::kBlack, CaretLayer()->background_color()); \
    EXPECT_TRUE(CaretLayer()->IsSolidColorLayerForTesting());      \
  } while (false)

class CaretDisplayItemClientTest : public PaintAndRasterInvalidationTest {
 protected:
  void SetUp() override {
    PaintAndRasterInvalidationTest::SetUp();
    Selection().SetCaretBlinkingSuspended(true);
  }

  FrameSelection& Selection() const {
    return GetDocument().View()->GetFrame().Selection();
  }

  FrameCaret& GetFrameCaret() { return Selection().FrameCaretForTesting(); }

  bool IsVisibleIfActive() { return GetFrameCaret().IsVisibleIfActive(); }
  void SetVisibleIfActive(bool v) { GetFrameCaret().SetVisibleIfActive(v); }

  CaretDisplayItemClient& GetCaretDisplayItemClient() {
    return *GetFrameCaret().display_item_client_;
  }

  const PhysicalRect& CaretLocalRect() {
    return GetCaretDisplayItemClient().local_rect_;
  }

  PhysicalRect ComputeCaretRect(const PositionWithAffinity& position) const {
    return CaretDisplayItemClient::ComputeCaretRectAndPainterBlock(position)
        .caret_rect;
  }

  const LayoutBlock* CaretLayoutBlock() {
    return GetCaretDisplayItemClient().layout_block_.Get();
  }

  const LayoutBlock* PreviousCaretLayoutBlock() {
    return GetCaretDisplayItemClient().previous_layout_block_.Get();
  }

  const PhysicalBoxFragment* CaretBoxFragment() {
    return GetCaretDisplayItemClient().box_fragment_.Get();
  }

  bool ShouldPaintCursorCaret(const LayoutBlock& block) {
    return Selection().ShouldPaintCaret(block);
  }

  bool ShouldPaintCursorCaret(const PhysicalBoxFragment& fragment) {
    return Selection().ShouldPaintCaret(fragment);
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

  const cc::Layer* CaretLayer() const {
    Vector<cc::Layer*> layers = CcLayersByName(
        GetDocument().View()->GetPaintArtifactCompositor()->RootLayer(),
        "Caret");
    if (layers.empty()) {
      return nullptr;
    }
    DCHECK_EQ(layers.size(), 1u);
    return layers.front();
  }

  void UpdateAllLifecyclePhasesForCaretTest() {
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);
    UpdateAllLifecyclePhasesForTest();
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);
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
  GetDocument().body()->Focus();

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*block));
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());

  ASSERT_CARET_LAYER();

  // Move the caret to the end of the text. Should invalidate both the old and
  // new carets.
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build(),
      SetSelectionOptions());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*block));
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  int delta = CaretLocalRect().X().ToInt();
  EXPECT_GT(delta, 0);
  EXPECT_EQ(PhysicalRect(delta, 0, 1, 1), CaretLocalRect());

  ASSERT_CARET_LAYER();

  // Remove selection. Should invalidate the old caret.
  Selection().SetSelection(SelectionInDOMTree(), SetSelectionOptions());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_FALSE(ShouldPaintCursorCaret(*block));
  // The caret display item client painted nothing, so is not validated.
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  EXPECT_EQ(PhysicalRect(), CaretLocalRect());
  EXPECT_FALSE(CaretLayer());
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
  GetDocument().body()->Focus();

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));

  // Move the caret into block2. Should invalidate both the old and new carets.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element2, 0))
                               .Build(),
                           SetSelectionOptions());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));

  ASSERT_CARET_LAYER();

  // Move the caret back into block1.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element1, 0))
                               .Build(),
                           SetSelectionOptions());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));

  ASSERT_CARET_LAYER();
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
  GetDocument().body()->Focus();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element2, 0))
                               .Build(),
                           SetSelectionOptions());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Move caret into block1. Should set PreviousCaretLayoutBlock to block2.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element1, 0))
                               .Build(),
                           SetSelectionOptions());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_EQ(block1, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Move caret into block2. Partial update should not change
  // PreviousCaretLayoutBlock.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element2, 0))
                               .Build(),
                           SetSelectionOptions());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Remove block2. Should clear caretLayoutBlock and PreviousCaretLayoutBlock.
  block_element2->parentNode()->RemoveChild(block_element2);
  EXPECT_FALSE(CaretLayoutBlock());
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Set caret into block1.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(block_element1, 0))
                               .Build(),
                           SetSelectionOptions());
  UpdateAllLifecyclePhasesForCaretTest();
  // Remove selection.
  Selection().SetSelection(SelectionInDOMTree(), SetSelectionOptions());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(block1, PreviousCaretLayoutBlock());
}

TEST_P(CaretDisplayItemClientTest, EnsureInvalidatePreviousLayoutBlock) {
  SetBodyInnerHTML(
      "<style>"
      "  .canvas {"
      "    width: 600px;"
      "  }"
      "  .page {"
      "    content-visibility: auto;"
      "    contain-intrinsic-size: auto 300px; "
      "  }"
      "  .paragraph {"
      "    position: relative;"
      "    left: 20px;"
      "  }"
      "  .high {"
      "    height: 10000px;"
      "  }"
      "</style>"

      "<div class='canvas' contenteditable='true'>"
      "  <div id='div1' class='page'>"
      "    <p id='p1' class='paragraph'>some text</p>"
      "  </div>"
      "  <div id='div2' class='high'></div>"
      "  <div id='div3' class='page'>"
      "    <p id='p3' class='paragraph'>some text</p>"
      "  </div>"
      "</div>");

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  UpdateAllLifecyclePhasesForCaretTest();
  auto* p1 = GetDocument().getElementById(AtomicString("p1"));
  auto* p1_block = To<LayoutBlock>(p1->GetLayoutObject());

  // Set caret into p1.
  GetDocument().body()->Focus();
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(p1, 0)).Build(),
      SetSelectionOptions());

  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(p1_block, PreviousCaretLayoutBlock());

  // Scroll the page all the way to bottom. p1 will be display locked.
  GetDocument().documentElement()->setScrollTop(1000000000);
  UpdateAllLifecyclePhasesForCaretTest();

  auto* p3 = GetDocument().getElementById(AtomicString("p3"));
  // Set caret into p3. Should set PreviousCaretLayoutBlock to p1.
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(p3, 0)).Build(),
      SetSelectionOptions());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(p1_block, PreviousCaretLayoutBlock());

  // PreviousCaretLayoutBlock should be invalidated and cleared after paint
  // invalidation.
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_NE(p1_block, PreviousCaretLayoutBlock());
}

TEST_P(CaretDisplayItemClientTest, CaretHideMoveAndShow) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  GetDocument().body()->Focus();
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());

  // Simulate that the blinking cursor becomes invisible.
  Selection().SetCaretEnabled(false);
  // Move the caret to the end of the text.
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build(),
      SetSelectionOptions());
  // Simulate that the cursor blinking is restarted.
  Selection().SetCaretEnabled(true);

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  int delta = CaretLocalRect().X().ToInt();
  EXPECT_GT(delta, 0);
  EXPECT_EQ(PhysicalRect(delta, 0, 1, 1), CaretLocalRect());

  ASSERT_CARET_LAYER();
}

TEST_P(CaretDisplayItemClientTest, BlinkingCaretNoInvalidation) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  GetDocument().body()->Focus();
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());

  // No paint or raster invalidation when caret is blinking.
  EXPECT_TRUE(IsVisibleIfActive());
  SetVisibleIfActive(false);
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_TRUE(IsVisibleIfActive());
  SetVisibleIfActive(true);
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
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
  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* editor = GetDocument().getElementById(AtomicString("editor"));
  auto* editor_block = To<LayoutBlock>(editor->GetLayoutObject());
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(editor, 0)).Build(),
      SetSelectionOptions());

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*editor_block));
  EXPECT_EQ(editor_block, CaretLayoutBlock());
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());

  // Composite container.
  container->setAttribute(html_names::kStyleAttr,
                          AtomicString("will-change: transform"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());

  // Uncomposite container.
  container->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());
}

TEST_P(CaretDisplayItemClientTest, PlainTextRTLCaretPosition) {
  LoadNoto();
  SetBodyInnerHTML(
      "<style>"
      "  div { width: 100px; padding: 5px; font: 20px NotoArabic }"
      "  #plaintext { unicode-bidi: plaintext }"
      "</style>"
      "<div id='regular' dir='rtl'>&#1575;&#1582;&#1578;&#1576;&#1585;</div>"
      "<div id='plaintext'>&#1575;&#1582;&#1578;&#1576;&#1585;</div>");

  auto* regular = GetDocument().getElementById(AtomicString("regular"));
  auto* regular_text_node = regular->firstChild();
  const Position& regular_position =
      Position::FirstPositionInNode(*regular_text_node);
  const PhysicalRect regular_caret_rect =
      ComputeCaretRect(PositionWithAffinity(regular_position));

  auto* plaintext = GetDocument().getElementById(AtomicString("plaintext"));
  auto* plaintext_text_node = plaintext->firstChild();
  const Position& plaintext_position =
      Position::FirstPositionInNode(*plaintext_text_node);
  const PhysicalRect plaintext_caret_rect =
      ComputeCaretRect(PositionWithAffinity(plaintext_position));

  EXPECT_EQ(regular_caret_rect, plaintext_caret_rect);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
// TODO(crbug.com/1457081): Previously, this test passed on the Mac bots even
// though `LoadNoto()` always failed. Now that `LoadNoto()` actually succeeds,
// this test fails on Mac and iOS though...
#define MAYBE_InsertSpaceToWhiteSpacePreWrapRTL \
  DISABLED_InsertSpaceToWhiteSpacePreWrapRTL
#else
#define MAYBE_InsertSpaceToWhiteSpacePreWrapRTL \
  InsertSpaceToWhiteSpacePreWrapRTL
#endif
// http://crbug.com/1278559
TEST_P(CaretDisplayItemClientTest, MAYBE_InsertSpaceToWhiteSpacePreWrapRTL) {
  LoadNoto();
  SetBodyInnerHTML(
      "<style>"
      "  div { white-space: pre-wrap; unicode-bidi: plaintext; width: 100px; "
      "  font: 20px NotoArabic }"
      "</style>"
      "<div id='editor' contentEditable='true' "
      "dir='rtl'>&#1575;&#1582;&#1578;&#1576;&#1585;</div>");

  auto* editor = GetDocument().getElementById(AtomicString("editor"));
  auto* editor_block = To<LayoutBlock>(editor->GetLayoutObject());
  auto* text_node = editor->firstChild();
  const Position& position = Position::LastPositionInNode(*text_node);
  const PhysicalRect& caret_from_position =
      ComputeCaretRect(PositionWithAffinity(position));

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(position).Build(),
      SetSelectionOptions());

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*editor_block));
  EXPECT_EQ(editor_block, CaretLayoutBlock());

  const PhysicalRect& caret_from_selection = CaretLocalRect();
  EXPECT_EQ(caret_from_position, caret_from_selection);

  // Compute the width of a white-space, give the NotoNaskhArabic font's
  // metrics.
  auto* text_layout_object = To<LayoutText>(text_node->GetLayoutObject());
  LayoutUnit width = text_layout_object->PhysicalLinesBoundingBox().Width();
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForCaretTest();
  text_layout_object = To<LayoutText>(text_node->GetLayoutObject());
  int space_width =
      (text_layout_object->PhysicalLinesBoundingBox().Width() - width).ToInt();
  EXPECT_GT(space_width, 0);

  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForCaretTest();
  const PhysicalSize& delta1 =
      caret_from_position.DistanceAsSize(caret_from_selection.MinXMinYCorner());
  EXPECT_EQ(PhysicalSize(2 * space_width, 0), delta1);

  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForCaretTest();
  const PhysicalSize& delta2 =
      caret_from_position.DistanceAsSize(caret_from_selection.MinXMinYCorner());
  EXPECT_EQ(PhysicalSize(3 * space_width, 0), delta2);
}

// http://crbug.com/1278559
TEST_P(CaretDisplayItemClientTest, InsertSpaceToWhiteSpacePreWrap) {
  LoadAhem();
  SetBodyInnerHTML(
      "<style>"
      "  div { white-space: pre-wrap; unicode-bidi: plaintext; width: 100px; "
      "font: 10px/1 Ahem}"
      "</style>"
      "<div id='editor' contentEditable='true'>XXXXX</div>");

  auto* editor = GetDocument().getElementById(AtomicString("editor"));
  auto* editor_block = To<LayoutBlock>(editor->GetLayoutObject());
  auto* text_node = editor->firstChild();
  const Position position = Position::LastPositionInNode(*text_node);
  const PhysicalRect& rect = ComputeCaretRect(PositionWithAffinity(position));
  // The 5 characters of arabic text rendered using the NotoArabic font has a
  // width of 20px and a height of 17px
  EXPECT_EQ(PhysicalRect(50, 0, 1, 10), rect);

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(position).Build(),
      SetSelectionOptions());

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*editor_block));
  EXPECT_EQ(editor_block, CaretLayoutBlock());
  EXPECT_EQ(rect, CaretLocalRect());

  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(60, 0, 1, 10), CaretLocalRect());

  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(70, 0, 1, 10), CaretLocalRect());
}

// http://crbug.com/1330093
TEST_P(CaretDisplayItemClientTest, CaretAtStartInWhiteSpacePreWrapRTL) {
  LoadNoto();
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0; padding: 0; }"
      "  div { white-space: pre-wrap; width: 90px; margin: 0; padding: 5px; "
      "  font: 20px NotoArabic }"
      "</style>"
      "<div dir=rtl contenteditable>&#1575;&#1582;&#1578;&#1576;&#1585; "
      "</div>");

  const Element& div = *GetDocument().QuerySelector(AtomicString("div"));
  const Position& position = Position::FirstPositionInNode(div);
  const PhysicalRect& rect = ComputeCaretRect(PositionWithAffinity(position));
  EXPECT_EQ(94, rect.X());
}

// https://crbug.com/1499405
TEST_P(CaretDisplayItemClientTest, CaretAtEdgeOfInlineBlock) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0; padding: 0; font: 10px/10px Ahem; }"
      "div#editable { width: 80px; padding: 0px 10px; text-align: center; }"
      "span { padding: 0px 15px; display: inline-block }");
  SetBodyContent(
      "<div id=editable contenteditable>"
      "<span contenteditable=false>foo</span>"
      "</div>");

  const Element& editable =
      *GetDocument().QuerySelector(AtomicString("#editable"));
  const LayoutBlock* editable_block =
      To<LayoutBlock>(editable.GetLayoutObject());

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  auto test = [this, editable_block](const Position& position,
                                     const PhysicalRect& expected_rect) {
    Selection().SetSelection(
        SelectionInDOMTree::Builder().Collapse(position).Build(),
        SetSelectionOptions());

    UpdateAllLifecyclePhasesExceptPaint();
    EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
    UpdateAllLifecyclePhasesForCaretTest();

    EXPECT_TRUE(ShouldPaintCursorCaret(*editable_block));
    EXPECT_EQ(editable_block, CaretLayoutBlock());
    EXPECT_EQ(expected_rect, CaretLocalRect());

    DCHECK_EQ(editable_block->PhysicalFragmentCount(), 1u);
    auto* editable_fragment = editable_block->GetPhysicalFragment(0);
    EXPECT_TRUE(ShouldPaintCursorCaret(*editable_fragment));
    EXPECT_EQ(editable_fragment, CaretBoxFragment());
  };

  test(Position::FirstPositionInNode(editable), PhysicalRect(20, 0, 1, 10));
  test(Position::LastPositionInNode(editable), PhysicalRect(79, 0, 1, 10));
}

class ComputeCaretRectTest : public EditingTestBase {
 public:
  ComputeCaretRectTest() = default;

 protected:
  PhysicalRect ComputeCaretRect(const PositionWithAffinity& position) const {
    return CaretDisplayItemClient::ComputeCaretRectAndPainterBlock(position)
        .caret_rect;
  }
  HitTestResult HitTestResultAtLocation(const HitTestLocation& location) {
    return GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  }
  HitTestResult HitTestResultAtLocation(int x, int y) {
    HitTestLocation location(gfx::Point(x, y));
    return HitTestResultAtLocation(location);
  }
};

TEST_P(CaretDisplayItemClientTest, FullDocumentPaintingWithCaret) {
  SetBodyInnerHTML(
      "<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto& div = *To<Element>(GetDocument().body()->firstChild());
  auto& layout_text = *To<Text>(div.firstChild())->GetLayoutObject();
  DCHECK(layout_text.IsInLayoutNGInlineFormattingContext());
  InlineCursor cursor;
  cursor.MoveTo(layout_text);
  const DisplayItemClient* text_inline_box =
      cursor.Current().GetDisplayItemClient();
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_inline_box->Id(), kForegroundType)));
  EXPECT_FALSE(CaretLayer());

  div.Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_inline_box->Id(), kForegroundType),
                          // New!
                          IsSameId(GetCaretDisplayItemClient().Id(),
                                   DisplayItem::kCaret)));
  ASSERT_CARET_LAYER();
}

TEST_F(ComputeCaretRectTest, CaretRectAfterEllipsisNoCrash) {
  SetBodyInnerHTML(
      "<style>pre{width:30px; overflow:hidden; text-overflow:ellipsis}</style>"
      "<pre id=target>long long long long long long text</pre>");
  const Node* text = GetElementById("target")->firstChild();
  const Position position = Position::LastPositionInNode(*text);
  // Shouldn't crash inside. The actual result doesn't matter and may change.
  ComputeCaretRect(PositionWithAffinity(position));
}

TEST_F(ComputeCaretRectTest, CaretRectAvoidNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0; padding: 0; font: 10px/10px Ahem; }"
      "div { width: 70px; padding: 0px 10px; }"
      "span { padding: 0px 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const PositionWithAffinity& caret_position1 =
      HitTestResultAtLocation(20, 5).GetPosition();
  const PhysicalRect& rect1 = ComputeCaretRect(caret_position1);
  EXPECT_EQ(PhysicalRect(10, 0, 1, 10), rect1);

  const PositionWithAffinity& caret_position2 =
      HitTestResultAtLocation(60, 5).GetPosition();
  const PhysicalRect& rect2 = ComputeCaretRect(caret_position2);
  EXPECT_EQ(PhysicalRect(69, 0, 1, 10), rect2);
}

}  // namespace blink

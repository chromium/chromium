// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/abstract_inline_text_box.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"

namespace blink {

class AbstractInlineTextBoxTest : public RenderingTest {};

TEST_F(AbstractInlineTextBoxTest, GetTextWithCollapsedWhiteSpace) {
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div id="target">abc </div>)HTML");

  const Element& target = *GetElementById("target");
  auto& layout_text = *To<LayoutText>(target.firstChild()->GetLayoutObject());
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc", inline_text_box->GetText());
  EXPECT_EQ(3u, inline_text_box->Len());
  EXPECT_FALSE(inline_text_box->NeedsTrailingSpace());
}

// For DumpAccessibilityTreeTest.AccessibilityInputTextValue/blink
TEST_F(AbstractInlineTextBoxTest, GetTextWithLineBreakAtCollapsedWhiteSpace) {
  // Line break at space between <label> and <input>.
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div style="width: 10ch"><label id=label>abc:</label> <input></div>)HTML");

  const Element& label = *GetElementById("label");
  auto& layout_text = *To<LayoutText>(label.firstChild()->GetLayoutObject());
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc:", inline_text_box->GetText());
  EXPECT_EQ(4u, inline_text_box->Len());
  EXPECT_FALSE(inline_text_box->NeedsTrailingSpace());
}

// For "web_tests/accessibility/inline-text-change-style.html"
TEST_F(AbstractInlineTextBoxTest,
       GetTextWithLineBreakAtMiddleCollapsedWhiteSpace) {
  // There should be a line break at the space after "012".
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div id="target" style="width: 0ch">012 345</div>)HTML");

  const Element& target = *GetElementById("target");
  auto& layout_text = *To<LayoutText>(target.firstChild()->GetLayoutObject());
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("012 ", inline_text_box->GetText());
  EXPECT_EQ(4u, inline_text_box->Len());
  EXPECT_TRUE(inline_text_box->NeedsTrailingSpace());
}

// DumpAccessibilityTreeTest.AccessibilitySpanLineBreak/blink
TEST_F(AbstractInlineTextBoxTest,
       GetTextWithLineBreakAtSpanCollapsedWhiteSpace) {
  // There should be a line break at the space in <span>.
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <p id="t1" style="width: 0ch">012<span id="t2"> </span>345</p>)HTML");

  const Element& target1 = *GetElementById("t1");
  auto& layout_text1 = *To<LayoutText>(target1.firstChild()->GetLayoutObject());
  auto* inline_text_box1 = layout_text1.FirstAbstractInlineTextBox();

  EXPECT_EQ("012", inline_text_box1->GetText());
  EXPECT_EQ(3u, inline_text_box1->Len());
  EXPECT_FALSE(inline_text_box1->NeedsTrailingSpace());

  const Element& target2 = *GetElementById("t2");
  auto& layout_text2 = *To<LayoutText>(target2.firstChild()->GetLayoutObject());
  auto* inline_text_box2 = layout_text2.FirstAbstractInlineTextBox();

  EXPECT_EQ(nullptr, inline_text_box2)
      << "We don't have inline box when <span> "
         "contains only collapsed white spaces.";
}

// For DumpAccessibilityTreeTest.AccessibilityInputTypes/blink
TEST_F(AbstractInlineTextBoxTest, GetTextWithLineBreakAtTrailingWhiteSpace) {
  // There should be a line break at the space of "abc: ".
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div style="width: 10ch"><label id=label>abc: <input></label></div>)HTML");

  const Element& label = *GetElementById("label");
  auto& layout_text = *To<LayoutText>(label.firstChild()->GetLayoutObject());
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();

  EXPECT_EQ("abc: ", inline_text_box->GetText());
  EXPECT_EQ(5u, inline_text_box->Len());
  EXPECT_TRUE(inline_text_box->NeedsTrailingSpace());
}

TEST_F(AbstractInlineTextBoxTest, GetTextOffsetInFormattingContext) {
  // The span should not affect the offset in container of the following inline
  // text boxes in the paragraph.
  //
  // Note that "&#10" is a Line Feed, ("\n").
  SetBodyInnerHTML(R"HTML(
    <style>p { white-space: pre-line; }</style>
    <p id="paragraph"><span>Offset</span>First sentence &#10; of the paragraph. Second sentence of &#10; the paragraph.</p>
    <br id="br">)HTML");

  const Element& paragraph = *GetElementById("paragraph");
  const Node& text_node = *paragraph.firstChild()->nextSibling();
  auto& layout_text = *To<LayoutText>(text_node.GetLayoutObject());

  // The above "layout_text" should create five AbstractInlineTextBoxes:
  // 1. "First sentence "
  // 2. "\n"
  // 3. "of the paragraph. Second sentence of "
  // 4." \n"
  // 5. "the paragraph."
  //
  // The AbstractInlineTextBoxes are all children of the same text node and an
  // an offset calculated in the container node should always be the same for
  // both LayoutNG and Legacy, even though Legacy doesn't collapse the
  // white spaces at the end of an AbstractInlineTextBox. White spaces at the
  // beginning of the third and fifth inline text box should be collapsed.
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();
  String text = "First sentence";
  EXPECT_EQ(text, inline_text_box->GetText());
  EXPECT_EQ(6u, inline_text_box->TextOffsetInFormattingContext(0));

  // We need to jump over the AbstractInlineTextBox with the line break.
  inline_text_box = inline_text_box->NextInlineTextBox()->NextInlineTextBox();
  text = "of the paragraph. Second sentence of";
  EXPECT_EQ(text, inline_text_box->GetText());
  EXPECT_EQ(21u, inline_text_box->TextOffsetInFormattingContext(0u));

  // See comment above.
  inline_text_box = inline_text_box->NextInlineTextBox()->NextInlineTextBox();
  EXPECT_EQ("the paragraph.", inline_text_box->GetText());
  EXPECT_EQ(58u, inline_text_box->TextOffsetInFormattingContext(0u));

  // Ensure that calling TextOffsetInFormattingContext on a br gives the correct
  // result.
  const Element& br_element = *GetElementById("br");
  auto& br_text = *To<LayoutText>(br_element.GetLayoutObject());
  inline_text_box = br_text.FirstAbstractInlineTextBox();
  EXPECT_EQ("\n", inline_text_box->GetText());
  EXPECT_EQ(0u, inline_text_box->TextOffsetInFormattingContext(0));
}

TEST_F(AbstractInlineTextBoxTest, CharacterWidths) {
  // There should be a line break at the space after "012".
  SetBodyInnerHTML(R"HTML(
    <style>* { font-size: 10px; }</style>
    <div id="div" style="width: 0ch">012 345</div>)HTML");

  const Element& div = *GetElementById("div");
  auto& layout_text = *To<LayoutText>(div.firstChild()->GetLayoutObject());
  auto* inline_text_box = layout_text.FirstAbstractInlineTextBox();

  Vector<float> widths;
  inline_text_box->CharacterWidths(widths);
  // There should be four elements in the "widths" vector, not three, because
  // the width of the trailing space should be included.
  EXPECT_EQ(4u, widths.size());
  EXPECT_TRUE(inline_text_box->NeedsTrailingSpace());
}

TEST_F(AbstractInlineTextBoxTest, HeapCompactionNoCrash) {
  // Regression test: https://crbug.com/1360006

  using TestVector = HeapVector<Member<LinkedObject>>;
  Persistent<TestVector> vector(MakeGarbageCollected<TestVector>(100));
  SetBodyInnerHTML(R"HTML(<div id="div">012 345</div>)HTML");

  const Element& div = *GetElementById("div");
  auto* inline_text_box = To<LayoutText>(div.firstChild()->GetLayoutObject())
                              ->FirstAbstractInlineTextBox();
  const auto* items = div.GetLayoutBox()->GetPhysicalFragment(0)->Items();

  const auto* vector_buffer_before_gc = items->Items().data();
  vector.Clear();
  CompactionTestDriver compaction_driver(ThreadState::Current());
  compaction_driver.ForceCompactionForNextGC();
  TestSupportingGC::PreciselyCollectGarbage();
  // We assume the above code caused heap compaction, and moved the buffer
  // of HeapVector<FragmentItem>.
  ASSERT_NE(vector_buffer_before_gc, items->Items().data());

  // LocalBounds() calls GetCursor(), which crashed in this scenario.
  inline_text_box->LocalBounds();
  // Pass if no crashes.
}

}  // namespace blink

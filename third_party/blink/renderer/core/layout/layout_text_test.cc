// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_text.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

class LayoutTextTest : public RenderingTest {
 public:
  void SetBasicBody(const char* message) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font-size: 10px;'>%s</div>", message));
  }

  void SetAhemBody(const char* message, const unsigned width) {
    SetBodyInnerHTML(String::Format(
        "<div id='target' style='font: 10px Ahem; width: %uem'>%s</div>", width,
        message));
  }

  LayoutText* GetLayoutTextById(const char* id) {
    return ToLayoutText(GetLayoutObjectByElementId(id)->SlowFirstChild());
  }

  LayoutText* GetBasicText() { return GetLayoutTextById("target"); }

  void SetSelectionAndUpdateLayoutSelection(const std::string& selection_text) {
    const SelectionInDOMTree selection =
        SelectionSample::SetSelectionText(GetDocument().body(), selection_text);
    UpdateAllLifecyclePhasesForTest();
    Selection().SetSelectionAndEndTyping(selection);
    Selection().CommitAppearanceIfNeeded();
  }

  const LayoutText* FindFirstLayoutText() {
    for (const Node& node :
         NodeTraversal::DescendantsOf(*GetDocument().body())) {
      if (node.GetLayoutObject() && node.GetLayoutObject()->IsText())
        return ToLayoutTextOrDie(node.GetLayoutObject());
    }
    NOTREACHED();
    return nullptr;
  }

  PhysicalRect GetSelectionRectFor(const std::string& selection_text) {
    std::stringstream stream;
    stream << "<div style='font: 10px/10px Ahem;'>" << selection_text
           << "</div>";
    SetSelectionAndUpdateLayoutSelection(stream.str());
    const Node* target = GetDocument().getElementById("target");
    const LayoutObject* layout_object =
        target ? target->GetLayoutObject() : FindFirstLayoutText();
    return layout_object->LocalSelectionVisualRect();
  }
};

const char kTacoText[] = "Los Compadres Taco Truck";

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLayoutTextTest : public testing::WithParamInterface<bool>,
                                    private ScopedLayoutNGForTest,
                                    public LayoutTextTest {
 public:
  ParameterizedLayoutTextTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, ParameterizedLayoutTextTest, testing::Bool());

}  // namespace

TEST_F(LayoutTextTest, WidthZeroFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(0u, 0u, LayoutUnit(), TextDirection::kLtr,
                                     false));
}

TEST_F(LayoutTextTest, WidthMaxFromZeroLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(), 0u,
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthZeroFromMaxLength) {
  SetBasicBody(kTacoText);
  float width = GetBasicText()->Width(0u, std::numeric_limits<unsigned>::max(),
                                      LayoutUnit(), TextDirection::kLtr, false);
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 160.f);
}

TEST_F(LayoutTextTest, WidthMaxFromMaxLength) {
  SetBasicBody(kTacoText);
  ASSERT_EQ(0, GetBasicText()->Width(std::numeric_limits<unsigned>::max(),
                                     std::numeric_limits<unsigned>::max(),
                                     LayoutUnit(), TextDirection::kLtr, false));
}

TEST_F(LayoutTextTest, WidthWithHugeLengthAvoidsOverflow) {
  // The test case from http://crbug.com/647820 uses a 288-length string, so for
  // posterity we follow that closely.
  SetBodyInnerHTML(R"HTML(
    <div
    id='target'>
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    </div>
  )HTML");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width = GetBasicText()->Width(
      23u, 4294967282u, LayoutUnit(2.59375), TextDirection::kRtl, false);
  ASSERT_GE(width, 100.f);
  ASSERT_LE(width, 300.f);
}

TEST_F(LayoutTextTest, WidthFromBeyondLength) {
  SetBasicBody("x");
  ASSERT_EQ(0u, GetBasicText()->Width(1u, 1u, LayoutUnit(), TextDirection::kLtr,
                                      false));
}

TEST_F(LayoutTextTest, WidthLengthBeyondLength) {
  SetBasicBody("x");
  // Width may vary by platform and we just want to make sure it's something
  // roughly reasonable.
  const float width =
      GetBasicText()->Width(0u, 2u, LayoutUnit(), TextDirection::kLtr, false);
  ASSERT_GE(width, 4.f);
  ASSERT_LE(width, 20.f);
}

TEST_F(LayoutTextTest, ContainsOnlyWhitespaceOrNbsp) {
  // Note that '&nbsp' is needed when only including whitespace to force
  // LayoutText creation from the div.
  SetBasicBody("&nbsp");
  // GetWidth will also compute |contains_only_whitespace_|.
  GetBasicText()->Width(0u, 1u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kYes,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("   \t\t\n \n\t &nbsp \n\t&nbsp\n \t");
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kUnknown,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());
  GetBasicText()->Width(0u, 18u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kYes,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("abc");
  GetBasicText()->Width(0u, 3u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kNo,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());

  SetBasicBody("  \t&nbsp\nx \n");
  GetBasicText()->Width(0u, 8u, LayoutUnit(), TextDirection::kLtr, false);
  EXPECT_EQ(OnlyWhitespaceOrNbsp::kNo,
            GetBasicText()->ContainsOnlyWhitespaceOrNbsp());
}

struct NGOffsetMappingTestData {
  const char* text;
  unsigned dom_start;
  unsigned dom_end;
  bool success;
  unsigned text_start;
  unsigned text_end;
} offset_mapping_test_data[] = {
    {"<div id=target> a  b  </div>", 0, 1, true, 0, 0},
    {"<div id=target> a  b  </div>", 1, 2, true, 0, 1},
    {"<div id=target> a  b  </div>", 2, 3, true, 1, 2},
    {"<div id=target> a  b  </div>", 2, 4, true, 1, 2},
    {"<div id=target> a  b  </div>", 2, 5, true, 1, 3},
    {"<div id=target> a  b  </div>", 3, 4, true, 2, 2},
    {"<div id=target> a  b  </div>", 3, 5, true, 2, 3},
    {"<div id=target> a  b  </div>", 5, 6, true, 3, 3},
    {"<div id=target> a  b  </div>", 5, 7, true, 3, 3},
    {"<div id=target> a  b  </div>", 6, 7, true, 3, 3},
    {"<div>a <span id=target> </span>b</div>", 0, 1, false, 0, 1}};

std::ostream& operator<<(std::ostream& out, NGOffsetMappingTestData data) {
  return out << "\"" << data.text << "\" " << data.dom_start << ","
             << data.dom_end << " => " << (data.success ? "true " : "false ")
             << data.text_start << "," << data.text_end;
}

class MapDOMOffsetToTextContentOffset
    : public LayoutTextTest,
      private ScopedLayoutNGForTest,
      public testing::WithParamInterface<NGOffsetMappingTestData> {
 public:
  MapDOMOffsetToTextContentOffset() : ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_TEST_SUITE_P(LayoutTextTest,
                         MapDOMOffsetToTextContentOffset,
                         testing::ValuesIn(offset_mapping_test_data));

TEST_P(MapDOMOffsetToTextContentOffset, Basic) {
  const auto data = GetParam();
  SetBodyInnerHTML(data.text);
  LayoutText* layout_text = GetBasicText();
  const NGOffsetMapping* mapping = layout_text->GetNGOffsetMapping();
  ASSERT_TRUE(mapping);
  unsigned start = data.dom_start;
  unsigned end = data.dom_end;
  bool success =
      layout_text->MapDOMOffsetToTextContentOffset(*mapping, &start, &end);
  EXPECT_EQ(data.success, success);
  if (success) {
    EXPECT_EQ(data.text_start, start);
    EXPECT_EQ(data.text_end, end);
  }
}

TEST_P(ParameterizedLayoutTextTest, CharacterAfterWhitespaceCollapsing) {
  SetBodyInnerHTML("a<span id=target> b </span>");
  LayoutText* layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ('b', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a <span id=target> b </span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ('b', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ('b', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a<span id=target> </span>b");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("a <span id=target> </span>b");
  layout_text = GetLayoutTextById("target");
  DCHECK(!layout_text->HasTextBoxes());
  EXPECT_EQ(0, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0, layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML(
      "<span style='white-space: pre'>a <span id=target> </span>b</span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(' ', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("<span id=target>Hello </span> <span>world</span>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ('H', layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(' ', layout_text->LastCharacterAfterWhitespaceCollapsing());
  layout_text =
      ToLayoutText(GetLayoutObjectByElementId("target")->NextSibling());
  DCHECK(!layout_text->HasTextBoxes());
  EXPECT_EQ(0, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0, layout_text->LastCharacterAfterWhitespaceCollapsing());

  SetBodyInnerHTML("<b id=target>&#x1F34C;_&#x1F34D;</b>");
  layout_text = GetLayoutTextById("target");
  EXPECT_EQ(0x1F34C, layout_text->FirstCharacterAfterWhitespaceCollapsing());
  EXPECT_EQ(0x1F34D, layout_text->LastCharacterAfterWhitespaceCollapsing());
}

TEST_P(ParameterizedLayoutTextTest, CaretMinMaxOffset) {
  SetBasicBody("foo");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody("  foo");
  EXPECT_EQ(2, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(5, GetBasicText()->CaretMaxOffset());

  SetBasicBody("foo  ");
  EXPECT_EQ(0, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(3, GetBasicText()->CaretMaxOffset());

  SetBasicBody(" foo  ");
  EXPECT_EQ(1, GetBasicText()->CaretMinOffset());
  EXPECT_EQ(4, GetBasicText()->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextTest, ResolvedTextLength) {
  SetBasicBody("foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("  foo");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody("foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());

  SetBasicBody(" foo  ");
  EXPECT_EQ(3u, GetBasicText()->ResolvedTextLength());
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffset) {
  // This test records the behavior introduced in crrev.com/e3eb4e
  SetBasicBody(" foo   bar ");
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(0));   // "| foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));    // " |foo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));    // " f|oo   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));    // " fo|o   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));    // " foo|   bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));    // " foo |  bar "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(6));   // " foo  | bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));    // " foo   |bar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));    // " foo   b|ar "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));    // " foo   ba|r "
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(10));   // " foo   bar| "
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(11));  // " foo   bar |"
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(12));  // out of range
}

TEST_P(ParameterizedLayoutTextTest, ContainsCaretOffsetInPre) {
  // These tests record the behavior introduced in crrev.com/e3eb4e

  SetBodyInnerHTML("<pre id='target'>foo   bar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|   bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo |  bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo  | bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo   |bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo   b|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(8));  // "foo   ba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(9));  // "foo   bar|"

  SetBodyInnerHTML("<pre id='target'>foo\n</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));   // "|foo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));   // "f|oo\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));   // "fo|o\n"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));   // "foo|\n"
  EXPECT_FALSE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|"

  SetBodyInnerHTML("<pre id='target'>foo\nbar</pre>");
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(0));  // "|foo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(1));  // "f|oo\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(2));  // "fo|o\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(3));  // "foo|\nbar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(4));  // "foo\n|bar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(5));  // "foo\nb|ar"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(6));  // "foo\nba|r"
  EXPECT_TRUE(GetBasicText()->ContainsCaretOffset(7));  // "foo\nbar|"
}

TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithCollapsedWhiteSpace) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>pre { font: 10px/1 Ahem; white-space: pre-line; }</style>
    <pre id=target> abc  def
    xyz   </pre>)HTML");
  const LayoutText& layout_text = *GetLayoutTextById("target");

  const auto& results = layout_text.GetTextBoxInfo();

  ASSERT_EQ(4u, results.size());

  EXPECT_EQ(1u, results[0].dom_start_offset);
  EXPECT_EQ(4u, results[0].dom_length);
  EXPECT_EQ(LayoutRect(0, 0, 40, 10), results[0].local_rect);

  EXPECT_EQ(6u, results[1].dom_start_offset);
  EXPECT_EQ(3u, results[1].dom_length);
  EXPECT_EQ(LayoutRect(40, 0, 30, 10), results[1].local_rect);

  EXPECT_EQ(9u, results[2].dom_start_offset);
  EXPECT_EQ(1u, results[2].dom_length);
  EXPECT_EQ(LayoutRect(70, 0, 0, 10), results[2].local_rect);

  EXPECT_EQ(14u, results[3].dom_start_offset);
  EXPECT_EQ(3u, results[3].dom_length);
  EXPECT_EQ(LayoutRect(0, 10, 30, 10), results[3].local_rect);
}

TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithGeneratedContent) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      div::before { content: '  a   bc'; }
      div::first-letter { font-weight: bold; }
      div { font: 10px/1 Ahem; }
    </style>
    <div id="target">XYZ</div>)HTML");
  const Element& target = *GetElementById("target");
  const Element& before =
      *GetElementById("target")->GetPseudoElement(kPseudoIdBefore);
  const LayoutText& layout_text_xyz =
      *ToLayoutText(target.firstChild()->GetLayoutObject());
  const LayoutText& layout_text_remaining =
      ToLayoutText(*before.GetLayoutObject()->SlowLastChild());
  const LayoutText& layout_text_first_letter =
      *layout_text_remaining.GetFirstLetterPart();

  auto boxes_xyz = layout_text_xyz.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes_xyz.size());
  EXPECT_EQ(0u, boxes_xyz[0].dom_start_offset);
  EXPECT_EQ(3u, boxes_xyz[0].dom_length);
  EXPECT_EQ(LayoutRect(40, 0, 30, 10), boxes_xyz[0].local_rect);

  auto boxes_first_letter = layout_text_first_letter.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes_first_letter.size());
  EXPECT_EQ(2u, boxes_first_letter[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_first_letter[0].dom_length);
  EXPECT_EQ(LayoutRect(0, 0, 10, 10), boxes_first_letter[0].local_rect);

  auto boxes_remaining = layout_text_remaining.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes_remaining.size());
  EXPECT_EQ(0u, boxes_remaining[0].dom_start_offset);
  EXPECT_EQ(1u, boxes_remaining[0].dom_length) << "two spaces to one space";
  EXPECT_EQ(LayoutRect(10, 0, 10, 10), boxes_remaining[0].local_rect);
  EXPECT_EQ(3u, boxes_remaining[1].dom_start_offset);
  EXPECT_EQ(2u, boxes_remaining[1].dom_length);
  EXPECT_EQ(LayoutRect(20, 0, 20, 10), boxes_remaining[1].local_rect);
}

// For http://crbug.com/985488
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithHidden) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        font: 10px/1 Ahem;
        overflow-x: hidden;
        white-space: nowrap;
        width: 9ch;
      }
    </style>
    <div id="target">  abcde  fghij  </div>
  )HTML");
  const Element& target = *GetElementById("target");
  const LayoutText& layout_text =
      *To<Text>(target.firstChild())->GetLayoutObject();

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes.size());

  EXPECT_EQ(2u, boxes[0].dom_start_offset);
  EXPECT_EQ(6u, boxes[0].dom_length);
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(LayoutRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/985488
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithEllipsis) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        font: 10px/1 Ahem;
        overflow-x: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        width: 9ch;
      }
    </style>
    <div id="target">  abcde  fghij  </div>
  )HTML");
  const Element& target = *GetElementById("target");
  const LayoutText& layout_text =
      *To<Text>(target.firstChild())->GetLayoutObject();

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(2u, boxes.size());

  EXPECT_EQ(2u, boxes[0].dom_start_offset);
  EXPECT_EQ(6u, boxes[0].dom_length);
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), boxes[0].local_rect);

  EXPECT_EQ(9u, boxes[1].dom_start_offset);
  EXPECT_EQ(5u, boxes[1].dom_length);
  EXPECT_EQ(LayoutRect(60, 0, 50, 10), boxes[1].local_rect);
}

// For http://crbug.com/1003413
TEST_P(ParameterizedLayoutTextTest, GetTextBoxInfoWithEllipsisForPseudoAfter) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #sample {
        box-sizing: border-box;
        font: 10px/1 Ahem;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        width: 5ch;
      }
      b::after { content: ","; }
    </style>
    <div id=sample><b id=target>abc</b><b>xyz</b></div>
  )HTML");
  const Element& target = *GetElementById("target");
  const Element& after = *target.GetPseudoElement(kPseudoIdAfter);
  // Set |layout_text| to "," in <pseudo::after>,</pseudo::after>
  const LayoutText& layout_text =
      *ToLayoutText(after.GetLayoutObject()->SlowFirstChild());

  auto boxes = layout_text.GetTextBoxInfo();
  EXPECT_EQ(1u, boxes.size());

  EXPECT_EQ(0u, boxes[0].dom_start_offset);
  EXPECT_EQ(1u, boxes[0].dom_length);
  EXPECT_EQ(LayoutRect(30, 0, 10, 10), boxes[0].local_rect);
}

TEST_P(ParameterizedLayoutTextTest,
       IsBeforeAfterNonCollapsedCharacterNoLineWrap) {
  // Basic tests
  SetBasicBody("foo");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));  // "|foo"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "foo|"

  // Return false at node end/start, respectively
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "foo|"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(0));   // "|foo"

  // Consecutive spaces are collapsed into one
  SetBasicBody("f   bar");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));   // "f|   bar"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(2));  // "f |  bar"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "f  | bar"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(2));    // "f |  bar"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "f  | bar"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(4));   // "f   |bar"

  // Leading spaces in a block are collapsed
  SetBasicBody("  foo");
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));  // "|  foo"
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));  // " | foo"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(1));   // " | foo"
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(2));   // "  |foo"

  // Trailing spaces in a block are collapsed
  SetBasicBody("foo  ");
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(3));  // "foo|  "
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(4));  // "foo | "
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(4));   // "foo | "
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(5));   // "foo  |"

  // Non-collapsed space at node end
  SetBasicBody("foo <span>bar</span>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      3));  // "foo| <span>bar</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      4));  // "foo |<span>bar</span>"

  // Non-collapsed space at node start
  SetBasicBody("foo<span id=bar> bar</span>");
  EXPECT_TRUE(GetLayoutTextById("bar")->IsBeforeNonCollapsedCharacter(
      0));  // "foo<span>| bar</span>"
  EXPECT_TRUE(GetLayoutTextById("bar")->IsAfterNonCollapsedCharacter(
      1));  // "foo<span> |bar</span>"

  // Consecutive spaces across nodes
  SetBasicBody("foo <span id=bar> bar</span>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      3));  // "foo| <span> bar</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      4));  // "foo |<span> bar</span>"
  EXPECT_FALSE(GetLayoutTextById("bar")->IsBeforeNonCollapsedCharacter(
      0));  // foo <span>| bar</span>
  EXPECT_FALSE(GetLayoutTextById("bar")->IsAfterNonCollapsedCharacter(
      1));  // foo <span> |bar</span>

  // Non-collapsed whitespace text node
  SetBasicBody("foo<span id=space> </span>bar");
  EXPECT_TRUE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_TRUE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));

  // Collapsed whitespace text node
  SetBasicBody("foo <span id=space> </span>bar");
  EXPECT_FALSE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_FALSE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));
}

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedLineWrapSpace) {
  LoadAhem();

  // Line wrapping inside node
  SetAhemBody("xx xx", 2);
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(2));  // "xx| xx"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(3));   // "xx |xx"

  // Legacy layout fails in the remaining test cases
  if (!LayoutNGEnabled())
    return;

  // Line wrapping at node start
  SetAhemBody("xx<span id=span> xx</span>", 2);
  EXPECT_TRUE(GetLayoutTextById("span")->IsBeforeNonCollapsedCharacter(
      0));  // "xx<span>| xx</span>"
  EXPECT_TRUE(GetLayoutTextById("span")->IsAfterNonCollapsedCharacter(
      1));  // "xx<span>| xx</span>"

  // Line wrapping at node end
  SetAhemBody("xx <span>xx</span>", 2);
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(
      2));  // "xx| <span>xx</span>"
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(
      3));  // "xx |<span>xx</span>"

  // Entire node as line wrapping
  SetAhemBody("xx<span id=space> </span>xx", 2);
  EXPECT_TRUE(GetLayoutTextById("space")->IsBeforeNonCollapsedCharacter(0));
  EXPECT_TRUE(GetLayoutTextById("space")->IsAfterNonCollapsedCharacter(1));
}

TEST_P(ParameterizedLayoutTextTest, IsBeforeAfterNonCollapsedCharacterBR) {
  SetBasicBody("<br>");
  EXPECT_TRUE(GetBasicText()->IsBeforeNonCollapsedCharacter(0));
  EXPECT_FALSE(GetBasicText()->IsBeforeNonCollapsedCharacter(1));
  EXPECT_FALSE(GetBasicText()->IsAfterNonCollapsedCharacter(0));
  EXPECT_TRUE(GetBasicText()->IsAfterNonCollapsedCharacter(1));
}

TEST_P(ParameterizedLayoutTextTest, AbsoluteQuads) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { margin: 0 }
    div {
      font: 10px/1 Ahem;
      width: 5em;
    }
    </style>
    <div>012<span id=target>345 67</span></div>
  )HTML");
  LayoutText* layout_text = GetLayoutTextById("target");
  Vector<FloatQuad> quads;
  layout_text->AbsoluteQuads(quads);
  EXPECT_THAT(quads, testing::ElementsAre(FloatRect(30, 0, 30, 10),
                                          FloatRect(0, 10, 20, 10)));
}

TEST_P(ParameterizedLayoutTextTest, AbsoluteQuadsVRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { margin: 0 }
    div {
      font: 10px/1 Ahem;
      width: 10em;
      height: 5em;
      writing-mode: vertical-rl;
    }
    </style>
    <div>012<span id=target>345 67</span></div>
  )HTML");
  LayoutText* layout_text = GetLayoutTextById("target");
  Vector<FloatQuad> quads;
  layout_text->AbsoluteQuads(quads);
  EXPECT_THAT(quads, testing::ElementsAre(FloatRect(90, 30, 10, 30),
                                          FloatRect(80, 0, 10, 20)));
}

TEST_P(ParameterizedLayoutTextTest, PhysicalLinesBoundingBox) {
  LoadAhem();
  SetBasicBody(
      "<style>"
      "div {"
      "  font-family:Ahem;"
      "  font-size: 13px;"
      "  line-height: 19px;"
      "  padding: 3px;"
      "}"
      "</style>"
      "<div id=div>"
      "  012"
      "  <span id=one>345</span>"
      "  <br>"
      "  <span style='padding: 20px'>"
      "    <span id=two style='padding: 5px'>678</span>"
      "  </span>"
      "</div>");
  // Layout NG Physical Fragment Tree
  // Box offset:3,3 size:778x44
  //   LineBox offset:3,3 size:91x19
  //     Text offset:0,3 size:52x13 start: 0 end: 4
  //     Box offset:52,3 size:39x13
  //       Text offset:0,0 size:39x13 start: 4 end: 7
  //       Text offset:91,3 size:0x13 start: 7 end: 8
  //   LineBox offset:3,22 size:89x19
  //     Box offset:0,-17 size:89x53
  //       Box offset:20,15 size:49x23
  //         Text offset:5,5 size:39x13 start: 8 end: 11
  const Element& div = *GetDocument().getElementById("div");
  const Element& one = *GetDocument().getElementById("one");
  const Element& two = *GetDocument().getElementById("two");
  EXPECT_EQ(PhysicalRect(3, 6, 52, 13),
            ToLayoutText(div.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(55, 6, 39, 13),
            ToLayoutText(one.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(28, 25, 39, 13),
            ToLayoutText(two.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
}

TEST_P(ParameterizedLayoutTextTest, PhysicalLinesBoundingBoxVerticalRL) {
  LoadAhem();
  SetBasicBody(R"HTML(
    <style>
    div {
      font-family:Ahem;
      font-size: 13px;
      line-height: 19px;
      padding: 3px;
      writing-mode: vertical-rl;
    }
    </style>
    <div id=div>
      012
      <span id=one>345</span>
      <br>
      <span style='padding: 20px'>
        <span id=two style='padding: 5px'>678</span>
      </span>
    </div>
  )HTML");
  // Similar to the previous test, with logical coordinates converted to
  // physical coordinates.
  const Element& div = *GetDocument().getElementById("div");
  const Element& one = *GetDocument().getElementById("one");
  const Element& two = *GetDocument().getElementById("two");
  EXPECT_EQ(PhysicalRect(25, 3, 13, 52),
            ToLayoutText(div.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(25, 55, 13, 39),
            ToLayoutText(one.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
  EXPECT_EQ(PhysicalRect(6, 28, 13, 39),
            ToLayoutText(two.firstChild()->GetLayoutObject())
                ->PhysicalLinesBoundingBox());
}

TEST_P(ParameterizedLayoutTextTest, WordBreakElement) {
  SetBasicBody("foo <wbr> bar");

  const Element* wbr = GetDocument().QuerySelector("wbr");
  DCHECK(wbr->GetLayoutObject()->IsText());
  const LayoutText* layout_wbr = ToLayoutText(wbr->GetLayoutObject());

  EXPECT_EQ(0u, layout_wbr->ResolvedTextLength());
  EXPECT_EQ(0, layout_wbr->CaretMinOffset());
  EXPECT_EQ(0, layout_wbr->CaretMaxOffset());
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRect) {
  LoadAhem();
  // TODO(yoichio): Fix LayoutNG incompatibility.
  EXPECT_EQ(PhysicalRect(10, 0, 50, 10), GetSelectionRectFor("f^oo ba|r"));
  EXPECT_EQ(PhysicalRect(0, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em'>f^oo ba|r</div>"));
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("foo^<br id='target'>|bar"));
  EXPECT_EQ(PhysicalRect(10, 0, 20, 10), GetSelectionRectFor("f^oo<br>b|ar"));
  EXPECT_EQ(PhysicalRect(10, 0, 30, 10),
            GetSelectionRectFor("<div>f^oo</div><div>b|ar</div>"));
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10), GetSelectionRectFor("foo^ |bar"));
  EXPECT_EQ(PhysicalRect(0, 0, 0, 0), GetSelectionRectFor("^ |foo"));
  EXPECT_EQ(PhysicalRect(0, 0, 0, 0),
            GetSelectionRectFor("fo^o<wbr id='target'>ba|r"));
  EXPECT_EQ(
      PhysicalRect(0, 0, 10, 10),
      GetSelectionRectFor("<style>:first-letter { float: right}</style>^fo|o"));
  // Since we don't paint trimed white spaces on LayoutNG,  we don't need fix
  // this case.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, 0, 0, 0)
                              : PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("foo^ |"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineBreak) {
  LoadAhem();
  EXPECT_EQ(PhysicalRect(30, 0, 10, 10),
            GetSelectionRectFor("f^oo<br id='target'><br>ba|r"));
  EXPECT_EQ(PhysicalRect(0, 10, 10, 10),
            GetSelectionRectFor("f^oo<br><br id='target'>ba|r"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineBreakPre) {
  LoadAhem();
  EXPECT_EQ(
      PhysicalRect(30, 0, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo^\n|\nbar</div>"));
  EXPECT_EQ(
      PhysicalRect(0, 10, 10, 10),
      GetSelectionRectFor("<div style='white-space:pre;'>foo\n^\n|bar</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectRTL) {
  LoadAhem();
  // TODO(yoichio) : Fix LastLogicalLeafIgnoringLineBreak so that 'foo' is the
  // last fragment.
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(-10, 0, 30, 20)
                              : PhysicalRect(-10, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=rtl>"
                                "f^oo ba|r baz</div>"));
  EXPECT_EQ(PhysicalRect(0, 0, 40, 20),
            GetSelectionRectFor("<div style='width: 2em' dir=ltr>"
                                "f^oo ba|r baz</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectVertical) {
  LoadAhem();
  EXPECT_EQ(
      PhysicalRect(0, 0, 20, 40),
      GetSelectionRectFor("<div style='writing-mode: vertical-lr; height: 2em'>"
                          "f^oo ba|r baz</div>"));
  EXPECT_EQ(
      PhysicalRect(10, 0, 20, 40),
      GetSelectionRectFor("<div style='writing-mode: vertical-rl; height: 2em'>"
                          "f^oo ba|r baz</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectVerticalRTL) {
  LoadAhem();
  // TODO(yoichio): Investigate diff (maybe soft line break treatment).
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, -10, 20, 30)
                              : PhysicalRect(0, -10, 20, 40),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-lr; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, -10, 20, 30)
                              : PhysicalRect(10, -10, 20, 40),
            GetSelectionRectFor(
                "<div style='writing-mode: vertical-rl; height: 2em' dir=rtl>"
                "f^oo ba|r baz</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineHeight) {
  LoadAhem();
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 0, 10, 50)
                              : PhysicalRect(10, 20, 10, 10),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 50, 10, 50)
                              : PhysicalRect(10, 30, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(10, 100, 10, 50)
                              : PhysicalRect(10, 80, 10, 50),
            GetSelectionRectFor("<div style='line-height: 50px; width:1em;'>"
                                "foo bar b^a|</div>"));
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectNegativeLeading) {
  LoadAhem();
  SetSelectionAndUpdateLayoutSelection(R"HTML(
    <div id="container" style="font: 10px/10px Ahem">
      ^
      <span id="span" style="display: inline-block; line-height: 1px">
        Text
      </span>
      |
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("span");
  LayoutObject* text = span->SlowFirstChild();
  EXPECT_EQ(PhysicalRect(0, -5, LayoutNGEnabled() ? 40 : 50, 10),
            text->LocalSelectionVisualRect());
}

TEST_P(ParameterizedLayoutTextTest, LocalSelectionRectLineHeightVertical) {
  LoadAhem();
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(0, 10, 50, 10)
                              : PhysicalRect(20, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "f^o|o bar baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(50, 10, 50, 10)
                              : PhysicalRect(70, 10, 50, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo b^a|r baz</div>"));
  EXPECT_EQ(LayoutNGEnabled() ? PhysicalRect(100, 10, 50, 10)
                              : PhysicalRect(120, 10, 10, 10),
            GetSelectionRectFor("<div style='line-height: 50px; height:1em; "
                                "writing-mode:vertical-lr'>"
                                "foo bar b^a|z</div>"));
}

TEST_P(ParameterizedLayoutTextTest, VisualRectInDocumentSVGTspan) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0px;
        font: 20px/20px Ahem;
      }
    </style>
    <svg>
      <text x="10" y="50" width="100">
        <tspan id="target" dx="15" dy="25">tspan</tspan>
      </text>
    </svg>
  )HTML");

  LayoutText* target =
      ToLayoutText(GetLayoutObjectByElementId("target")->SlowFirstChild());
  const int ascent = 16;
  PhysicalRect expected(10 + 15, 50 + 25 - ascent, 20 * 5, 20);
  EXPECT_EQ(expected, target->VisualRectInDocument());
  EXPECT_EQ(expected, target->VisualRectInDocument(kUseGeometryMapper));
}

TEST_P(ParameterizedLayoutTextTest, VisualRectInDocumentSVGTspanTB) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0px;
        font: 20px/20px Ahem;
      }
    </style>
    <svg>
      <text x="50" y="10" width="100" writing-mode="tb">
        <tspan id="target" dx="15" dy="25">tspan</tspan>
      </text>
    </svg>
  )HTML");

  LayoutText* target =
      ToLayoutText(GetLayoutObjectByElementId("target")->SlowFirstChild());
  PhysicalRect expected(50 + 15 - 20 / 2, 10 + 25, 20, 20 * 5);
  EXPECT_EQ(expected, target->VisualRectInDocument());
  EXPECT_EQ(expected, target->VisualRectInDocument(kUseGeometryMapper));
}

}  // namespace blink

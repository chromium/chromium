// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_text_auto_space.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;

class InlineTextAutoSpaceTest : public RenderingTest,
                                ScopedCSSTextAutoSpaceForTest {
 public:
  explicit InlineTextAutoSpaceTest() : ScopedCSSTextAutoSpaceForTest(true) {}

  LayoutBlockFlow* PreparePageLayoutBlock(String html,
                                          String container_css = String()) {
    html = String(R"HTML(
      <style>
      #container {
        font-family: Ahem;
        font-size: 10px;)HTML") +
           container_css + R"HTML(
      }
      </style>
      <div id="container">)HTML" +
           html + "</div>";
    SetBodyInnerHTML(html);
    return GetLayoutBlockFlowByElementId("container");
  }

  Vector<wtf_size_t> AutoSpaceOffsets(String html,
                                      String container_css = String()) {
    const LayoutBlockFlow* container =
        PreparePageLayoutBlock(html, container_css);
    InlineNodeData* node_data = container->GetInlineNodeData();
    Vector<wtf_size_t> offsets;
    InlineTextAutoSpace auto_space(*node_data);
    auto_space.ApplyIfNeeded(*node_data, &offsets);
    return offsets;
  }
};

// Test the optimizations in `ApplyIfNeeded` don't affect results.
TEST_F(InlineTextAutoSpaceTest, NonHanIdeograph) {
  // For boundary-check, extend the range by 1 to lower and to upper.
  for (UChar ch = TextAutoSpace::kNonHanIdeographMin - 1;
       ch <= TextAutoSpace::kNonHanIdeographMax + 1; ++ch) {
    StringBuilder builder;
    builder.Append("X");
    builder.Append(ch);
    builder.Append("X");
    const String html = builder.ToString();
    Vector<wtf_size_t> offsets = AutoSpaceOffsets(html);
    TextAutoSpace::CharType type = TextAutoSpace::GetType(ch);
    if (type == TextAutoSpace::kIdeograph) {
      EXPECT_THAT(offsets, ElementsAre(1, 2)) << String::Format("U+%04X", ch);
    } else {
      EXPECT_THAT(offsets, ElementsAre()) << String::Format("U+%04X", ch);
    }
  }
}

// End to end test for text-autospace
TEST_F(InlineTextAutoSpaceTest, InsertSpacing) {
  LoadAhem();
  String test_string = u"AAAあああa";
  LayoutBlockFlow* container = PreparePageLayoutBlock(test_string);
  InlineNode inline_node{container};
  InlineNodeData* node_data = container->GetInlineNodeData();
  inline_node.PrepareLayoutIfNeeded();

  Vector<CharacterRange> final_ranges;
  for (const InlineItem& item : node_data->items) {
    const auto* shape_result = item.TextShapeResult();
    Vector<CharacterRange> ranges;
    shape_result->IndividualCharacterRanges(&ranges);
    final_ranges.AppendVector(ranges);
  }
  Vector<float> expected_result_start{0, 10, 20, 31.25, 41.25, 51.25, 62.5};
  ASSERT_EQ(expected_result_start.size(), final_ranges.size());
  for (wtf_size_t i = 0; i < final_ranges.size(); i++) {
    EXPECT_NEAR(final_ranges[i].start, expected_result_start[i], 1e-6)
        << "unexpected width at position i of " << i;
  }
}

struct HtmlData {
  const UChar* html;
  std::vector<wtf_size_t> offsets;
  const char* container_css = nullptr;
} g_html_data[] = {
    {u"ああああ", {}},
    {u"English only", {}},
    {u"Abcあああ", {3}},
    {u"123あああ", {3}},
    {u"あああAbc", {3}},
    {u"あああ123", {3}},
    {u"ああAああ", {2, 3}},
    {u"ああ1ああ", {2, 3}},
    {u"ああAbcああ", {2, 5}},
    {u"ああA12ああ", {2, 5}},
    {u"ああ123ああ", {2, 5}},
    {u"あ\U000739AD", {}},
    {u"<span>ああ</span>Aああ", {2, 3}},
    {u"<span>ああA</span>ああ", {2, 3}},
    {u"ああ<span>A</span>ああ", {2, 3}},
    {u"ああ<span>Aああ</span>", {2, 3}},
    {u"ああ 12 ああ", {}},
    {u"あ<span style='text-autospace: no-autospace'>1</span>2", {}},
    {u"あ<span style='text-autospace: no-autospace'>あ</span>2", {}},
    {u"あAあ", {}, "writing-mode: vertical-rl; text-orientation: upright"},
    {u"あ1あ", {}, "writing-mode: vertical-rl; text-orientation: upright"},
    {u"あ<span style='text-orientation: upright'>1</span>あ",
     {},
     "writing-mode: vertical-rl"},
    // The following tests are testing the RTL/LTR mixed layout. Whether to add
    // spacing at the boundary would be determined after line breaking, when the
    // adjacent runs are determined.
    // LTR RTL LTR
    {u"ああ\u05D0ああ", {2, 3}},
    {u"あ<span>あ\u05D0あ</span>あ", {2, 3}},
    // RTL LTR RTL
    {u"\u05D0ああ\u05D0あ", {1, 3, 4}},
    {u"ああ<span>\u05D0</span>ああ", {2, 3}},
    {u"\u05D0ああ\u05D0あ", {1, 3, 4}},

};
class HtmlTest : public InlineTextAutoSpaceTest,
                 public testing::WithParamInterface<HtmlData> {};
INSTANTIATE_TEST_SUITE_P(InlineTextAutoSpaceTest,
                         HtmlTest,
                         testing::ValuesIn(g_html_data));

TEST_P(HtmlTest, Apply) {
  const auto& test = GetParam();
  Vector<wtf_size_t> offsets = AutoSpaceOffsets(test.html, test.container_css);
  EXPECT_THAT(offsets, ElementsAreArray(test.offsets));
}

}  // namespace

}  // namespace blink

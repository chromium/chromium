// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/text_auto_space.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;

class TextAutoSpaceTest : public RenderingTest {
 public:
  struct AutoSpaceCallback : public TextAutoSpace::Callback {
    void DidApply(base::span<const OffsetWithSpacing> applied_offsets) final {
      for (const OffsetWithSpacing& offset : applied_offsets) {
        offsets.push_back(offset.offset);
      }
    }

    Vector<wtf_size_t> offsets;
  };

  LayoutBlockFlow* PreparePageLayoutBlock(String html,
                                          String container_css = String()) {
    html = String(R"HTML(
      <style>
      #container {
        text-autospace: normal;
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
    LayoutBlockFlow* container = PreparePageLayoutBlock(html, container_css);
    InlineNodeData* node_data = container->GetInlineNodeData();
    TextAutoSpace auto_space(*node_data);
    AutoSpaceCallback callback;
    auto_space.SetCallbackForTesting(&callback);
    auto_space.ApplyIfNeeded(InlineNode(container), *node_data);
    return callback.offsets;
  }
};

// This tests: 1) how well the `MayApply` optimization can reject unapplicable
// data early, and 2) if it doesn't miss cases where it must apply.
struct MayApplyData {
  String ToString() const { return str8 ? String(str8) : String(str16); }
  const char* str8;
  const UChar* str16;
  bool may_apply;
} g_may_apply_data[] = {
    {"English", nullptr, false},
    {nullptr, u"Caf\u00E9", false},
    // Common characters > U+00FF: ZWSP, ORC.
    {nullptr, u"\u200B\uFFFC", false},
    // Curly quotation marks.
    {nullptr, u"\u2018\u2019\u201C\u201D", false},
    // CJK ideographic characters.
    {nullptr, u"水", true},
};
class MayApplyTest : public TextAutoSpaceTest,
                     public testing::WithParamInterface<MayApplyData> {};
INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         MayApplyTest,
                         testing::ValuesIn(g_may_apply_data));

TEST_P(MayApplyTest, MayApply) {
  const auto& test = GetParam();
  const String string = test.ToString();
  LayoutBlockFlow* block_flow = PreparePageLayoutBlock(string);
  const InlineNodeData* inline_node_data = block_flow->GetInlineNodeData();
  TextAutoSpace auto_space(*inline_node_data);
  EXPECT_EQ(auto_space.MayApply(), test.may_apply);
}

// End to end test for text-autospace
TEST_F(TextAutoSpaceTest, InsertSpacing) {
  LoadAhem();
  String test_string = u"AAAあああa";
  LayoutBlockFlow* container = PreparePageLayoutBlock(test_string);
  InlineNode inline_node{container};
  InlineNodeData* node_data = container->GetInlineNodeData();
  inline_node.PrepareLayoutIfNeeded();

  Vector<CharacterRange> final_ranges;
  for (const Member<InlineItem>& item_ptr : node_data->items) {
    const InlineItem& item = *item_ptr;
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
    // 10
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
    // 20
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
    // Marks
    {u"あ\u3099X", {2}},  // Combining Katakana-Hiragana Voiced Sound Mark
    {u"あ\u309BX", {2}},  // Katakana-Hiragana Voiced Sound Mark
    {u"\u8279\uFE00\u8279\uFE01X", {4}},          // VS
    {u"\u795E\U000E0100\u793E\U000E0101X", {6}},  // IVS

};
class HtmlTest : public TextAutoSpaceTest,
                 public testing::WithParamInterface<HtmlData> {};
INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         HtmlTest,
                         testing::ValuesIn(g_html_data));

TEST_P(HtmlTest, Apply) {
  const auto& test = GetParam();
  Vector<wtf_size_t> offsets = AutoSpaceOffsets(test.html, test.container_css);
  EXPECT_THAT(offsets, ElementsAreArray(test.offsets));
}

}  // namespace

}  // namespace blink

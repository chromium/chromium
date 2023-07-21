// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

using testing::ElementsAreArray;

struct TypeData {
  UChar32 ch;
  TextAutoSpace::CharType type;
} g_type_data[] = {
    {' ', TextAutoSpace::kOther},
    {'0', TextAutoSpace::kLetterOrNumeral},
    {'A', TextAutoSpace::kLetterOrNumeral},
    {u'\u05D0', TextAutoSpace::kLetterOrNumeral},  // Hebrew Letter Alef
    {u'\u0E50', TextAutoSpace::kLetterOrNumeral},  // Thai Digit Zero
    {u'\u3041', TextAutoSpace::kIdeograph},        // Hiragana Letter Small A
    {u'\u30FB', TextAutoSpace::kOther},            // Katakana Middle Dot
    {u'\uFF21', TextAutoSpace::kOther},  // Fullwidth Latin Capital Letter A
    {U'\U00017000', TextAutoSpace::kLetterOrNumeral},  // Tangut Ideograph
    {U'\U00031350', TextAutoSpace::kIdeograph},  // CJK Unified Ideographs H
};

std::ostream& operator<<(std::ostream& ostream, const TypeData& type_data) {
  return ostream << "U+" << std::hex << type_data.ch;
}

class TextAutoSpaceTypeTest : public testing::Test,
                              public testing::WithParamInterface<TypeData> {};

INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         TextAutoSpaceTypeTest,
                         testing::ValuesIn(g_type_data));

TEST_P(TextAutoSpaceTypeTest, Char) {
  const auto& data = GetParam();
  EXPECT_EQ(TextAutoSpace::GetType(data.ch), data.type);
}

struct HtmlData {
  const UChar* html;
  std::vector<wtf_size_t> offsets;
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
    {u"<span>ああ</span>Aああ", {2, 3}},
    {u"<span>ああA</span>ああ", {2, 3}},
    {u"ああ<span>A</span>ああ", {2, 3}},
    {u"ああ<span>Aああ</span>", {2, 3}},
    {u"ああ 12 ああ", {}},
};
class HtmlTest : public RenderingTest,
                 public testing::WithParamInterface<HtmlData> {};
INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         HtmlTest,
                         testing::ValuesIn(g_html_data));

TEST_P(HtmlTest, Apply) {
  const auto& test = GetParam();
  SetBodyInnerHTML(String(R"HTML(
    <style>
    #container {
      font-size: 10px;
    }
    </style>
    <div id="container">)HTML") +
                   test.html + "</div>");
  const auto* container = GetLayoutBlockFlowByElementId("container");
  NGInlineNodeData* node_data = container->GetNGInlineNodeData();
  Vector<wtf_size_t> offsets;
  TextAutoSpace::ApplyIfNeeded(*node_data, &offsets);
  EXPECT_THAT(offsets, ElementsAreArray(test.offsets));
}

}  // namespace

}  // namespace blink

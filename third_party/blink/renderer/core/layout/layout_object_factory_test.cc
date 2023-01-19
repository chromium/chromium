// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutObjectFactoryTest : public RenderingTest {};

TEST_F(LayoutObjectFactoryTest, BR) {
  SetBodyInnerHTML("<br id=sample>");
  const auto& layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_TRUE(layout_object.IsLayoutNGObject());
}

// http://crbug.com/1060007
TEST_F(LayoutObjectFactoryTest, Counter) {
  InsertStyleElement(
      "li::before { content: counter(i, upper-roman); }"
      "ol { list-style: none; ");
  SetBodyInnerHTML("<ol><li id=sample>one</li></ol>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");
  const auto& sample = *GetElementById("sample");
  const auto& psedo = *sample.GetPseudoElement(kPseudoIdBefore);
  const auto& counter_layout_object =
      *To<LayoutCounter>(psedo.GetLayoutObject()->SlowFirstChild());

  EXPECT_EQ(R"DUMP(
LayoutNGListItem LI id="sample"
  +--LayoutInline ::before
  |  +--LayoutCounter (anonymous) "0"
  +--LayoutText #text "one"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
  EXPECT_TRUE(counter_layout_object.IsLayoutNGObject());
}

TEST_F(LayoutObjectFactoryTest, TextCombineInHorizontal) {
  InsertStyleElement(
      "div { writing-mode: horizontal-tb; }"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tcy id=sample>ab</tcy></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_EQ(R"DUMP(
LayoutInline TCY id="sample"
  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_F(LayoutObjectFactoryTest, TextCombineInVertical) {
  InsertStyleElement(
      "div { writing-mode: vertical-rl; }"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tcy id=sample>ab</tcy></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_EQ(R"DUMP(
LayoutInline TCY id="sample"
  +--LayoutNGTextCombine (anonymous)
  |  +--LayoutText #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_F(LayoutObjectFactoryTest, WordBreak) {
  SetBodyInnerHTML("<wbr id=sample>");
  const auto& layout_object = *GetLayoutObjectByElementId("sample");

  EXPECT_TRUE(layout_object.IsLayoutNGObject());
}

}  // namespace blink

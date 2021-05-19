// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutObjectFactoryTest : public ::testing::WithParamInterface<bool>,
                                private ScopedLayoutNGForTest,
                                public RenderingTest {
 protected:
  LayoutObjectFactoryTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }
};

INSTANTIATE_TEST_SUITE_P(LayoutObjectFactoryTest,
                         LayoutObjectFactoryTest,
                         ::testing::Bool());

TEST_P(LayoutObjectFactoryTest, BR) {
  SetBodyInnerHTML("<br id=sample>");
  const auto& layout_object = *GetLayoutObjectByElementId("sample");

  if (LayoutNGEnabled())
    EXPECT_TRUE(layout_object.IsLayoutNGObject());
  else
    EXPECT_FALSE(layout_object.IsLayoutNGObject());
}

TEST_P(LayoutObjectFactoryTest, TextCombineInHorizontal) {
  InsertStyleElement(
      "div { writing-mode: horizontal-tb; }"
      "tyc { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tyc id=sample>ab</tyc></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");
  EXPECT_EQ(R"DUMP(
LayoutInline TYC id="sample"
  +--LayoutTextCombine #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_P(LayoutObjectFactoryTest, TextCombineInVertical) {
  InsertStyleElement(
      "div { writing-mode: vertical-rl; }"
      "tyc { text-combine-upright: all; }");
  SetBodyInnerHTML("<div><tyc id=sample>ab</tyc></div>");
  const auto& sample_layout_object = *GetLayoutObjectByElementId("sample");
  EXPECT_EQ(R"DUMP(
LayoutInline TYC id="sample"
  +--LayoutTextCombine #text "ab"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_P(LayoutObjectFactoryTest, WordBreak) {
  SetBodyInnerHTML("<wbr id=sample>");
  const auto& layout_object = *GetLayoutObjectByElementId("sample");

  if (LayoutNGEnabled())
    EXPECT_TRUE(layout_object.IsLayoutNGObject());
  else
    EXPECT_FALSE(layout_object.IsLayoutNGObject());
}

}  // namespace blink

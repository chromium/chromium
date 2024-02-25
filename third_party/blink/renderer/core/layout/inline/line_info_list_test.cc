// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_info_list.h"

#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

class LineInfoListTest : public RenderingTest {};

TEST_F(LineInfoListTest, Get) {
  LineInfoListOf<4> list;
  LineInfo& first = list.Append();
  EXPECT_EQ(list.Size(), 1u);
  EXPECT_EQ(&first, &list.Front());
  EXPECT_EQ(&first, &list.Back());

  LineInfo& second = list.Append();
  EXPECT_EQ(list.Size(), 2u);
  EXPECT_NE(&second, &first);
  EXPECT_NE(&second, &list.Front());
  EXPECT_EQ(&second, &list.Back());
  second.SetStart({0, 1});

  // `Get` with a null break token should get the first instance.
  bool is_cached = false;
  LineInfo& first_cached = list.Get(/* break_token */ nullptr, is_cached);
  EXPECT_TRUE(is_cached);
  EXPECT_EQ(&first_cached, &first);

  // `Get` with a `second.Start()` break token should get the second instance.
  SetBodyInnerHTML(R"HTML(
    <div id="container">test</div>
  )HTML");
  InlineNode node(To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  auto* second_break_token = InlineBreakToken::Create(
      node, &node.Style(), second.Start(), InlineBreakToken::kDefault);
  is_cached = false;
  LineInfo& second_cached = list.Get(second_break_token, is_cached);
  EXPECT_TRUE(is_cached);
  EXPECT_EQ(&second_cached, &second);

  // When it can't find a cached instance, it should return an unused instance.
  list.Clear();
  is_cached = false;
  LineInfo& not_exist = list.Get(/* break_token */ nullptr, is_cached);
  EXPECT_FALSE(is_cached);
  EXPECT_EQ(&not_exist, &first);
}

}  // namespace blink

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info_list.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

class NGLineInfoListTest : public RenderingTest {};

TEST_F(NGLineInfoListTest, Get) {
  NGLineInfoList list;
  NGLineInfo& first = list.Append();
  EXPECT_EQ(list.Size(), 1u);
  EXPECT_EQ(&first, &list.Front());
  EXPECT_EQ(&first, &list.Back());

  NGLineInfo& second = list.Append();
  EXPECT_EQ(list.Size(), 2u);
  EXPECT_NE(&second, &first);
  EXPECT_NE(&second, &list.Front());
  EXPECT_EQ(&second, &list.Back());
  second.SetStart({0, 1});

  // `Get` with a null break token should get the first instance.
  bool is_cached = false;
  NGLineInfo& first_cached = list.Get(/* break_token */ nullptr, is_cached);
  EXPECT_TRUE(is_cached);
  EXPECT_EQ(&first_cached, &first);

  // `Get` with a `second.Start()` break token should get the second instance.
  SetBodyInnerHTML(R"HTML(
    <div id="container">test</div>
  )HTML");
  NGInlineNode node(
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container")));
  NGInlineBreakToken* second_break_token = NGInlineBreakToken::Create(
      node, &node.Style(), second.Start(), NGInlineBreakToken::kDefault);
  is_cached = false;
  NGLineInfo& second_cached = list.Get(second_break_token, is_cached);
  EXPECT_TRUE(is_cached);
  EXPECT_EQ(&second_cached, &second);

  // When it can't find a cached instance, it should return an unused instance.
  list.Clear();
  is_cached = false;
  NGLineInfo& not_exist = list.Get(/* break_token */ nullptr, is_cached);
  EXPECT_FALSE(is_cached);
  EXPECT_EQ(&not_exist, &first);
}

}  // namespace blink

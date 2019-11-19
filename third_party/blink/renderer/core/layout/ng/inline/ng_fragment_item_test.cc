// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

using testing::ElementsAre;

namespace blink {

class NGFragmentItemTest : public NGLayoutTest,
                           ScopedLayoutNGFragmentItemForTest {
 public:
  NGFragmentItemTest() : ScopedLayoutNGFragmentItemForTest(true) {}

  Vector<const NGFragmentItem*> ItemsForAsVector(
      const LayoutObject& layout_object) {
    const auto items = NGFragmentItem::ItemsFor(layout_object);
    Vector<const NGFragmentItem*> list;
    for (const NGFragmentItem& item : items) {
      EXPECT_EQ(item.GetLayoutObject(), &layout_object);
      list.push_back(&item);
    }
    return list;
  }
};

TEST_F(NGFragmentItemTest, BasicText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    div {
      width: 10ch;
    }
    </style>
    <div id="container">
      1234567 98765
    </div>
  )HTML");

  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  LayoutText* layout_text = ToLayoutText(container->FirstChild());
  const NGPhysicalBoxFragment* box = container->CurrentFragment();
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 4u);

  // The text node wraps, produces two fragments.
  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_EQ(items_for_text.size(), 2u);

  const NGFragmentItem& text1 = *items_for_text[0];
  EXPECT_EQ(text1.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text1.GetLayoutObject(), layout_text);
  EXPECT_EQ(text1.Offset(), PhysicalOffset());

  const NGFragmentItem& text2 = *items_for_text[1];
  EXPECT_EQ(text2.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text2.GetLayoutObject(), layout_text);
  EXPECT_EQ(text2.Offset(), PhysicalOffset(0, 10));

  EXPECT_EQ(IntRect(0, 0, 70, 20),
            layout_text->FragmentsVisualRectBoundingBox());
}

TEST_F(NGFragmentItemTest, BasicInlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    #container {
      width: 10ch;
    }
    #span1, #span2 {
      background: gray;
    }
    </style>
    <div id="container">
      000
      <span id="span1">1234 5678</span>
      999
      <span id="span2">12345678</span>
    </div>
  )HTML");

  // "span1" wraps, produces two fragments.
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  ASSERT_NE(span1, nullptr);
  Vector<const NGFragmentItem*> items_for_span1 = ItemsForAsVector(*span1);
  EXPECT_EQ(items_for_span1.size(), 2u);

  EXPECT_EQ(IntRect(0, 0, 80, 20), span1->FragmentsVisualRectBoundingBox());

  // "span2" doesn't wrap, produces only one fragment.
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  ASSERT_NE(span2, nullptr);
  Vector<const NGFragmentItem*> items_for_span2 = ItemsForAsVector(*span2);
  EXPECT_EQ(items_for_span2.size(), 1u);

  EXPECT_EQ(IntRect(0, 20, 80, 10), span2->FragmentsVisualRectBoundingBox());
}

}  // namespace blink

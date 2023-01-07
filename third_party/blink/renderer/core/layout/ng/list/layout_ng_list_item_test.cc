// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

namespace blink {

class LayoutNGListItemTest : public NGLayoutTest {};

namespace {

// http://crbug.com/1222633
TEST_F(LayoutNGListItemTest, FindSymbolMarkerLayoutTextWithTextCombine) {
  ScopedLayoutNGForTest enable_layout_ng_text_combine(true);
  InsertStyleElement(
      "li { text-combine-upright: all; writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<li id=target>a</li>");
  // LayoutNGListItem {LI}
  //   LayoutNGOutsideListMarker {::marker}
  //      LayoutNGTextCombine (anonymous)
  //        LayoutText (anonymous) "\x{2022} "
  //   LayoutNGTextCombine (anonymous)
  //     LayoutText {#text} "a"
  const auto& target = *GetElementById("target");
  const auto* const marker_layout_text =
      LayoutNGListItem::FindSymbolMarkerLayoutText(target.GetLayoutObject());
  const auto* const text_combine =
      To<LayoutNGTextCombine>(marker_layout_text->Parent());
  EXPECT_EQ(marker_layout_text,
            LayoutNGListItem::FindSymbolMarkerLayoutText(text_combine));
}

TEST_F(LayoutNGListItemTest, InsideWithFirstLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    li {
      list-style-position: inside;
    }
    .after::first-line {
      background: yellow;
    }
    </style>
    <div id=container>
      <ul>
        <li id=item>test</li>
      </ul>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  container->classList().Add("after");
  GetDocument().UpdateStyleAndLayoutTree();

  // The list-item should have a marker.
  auto* list_item = To<LayoutNGListItem>(GetLayoutObjectByElementId("item"));
  LayoutObject* marker = list_item->Marker();
  EXPECT_TRUE(marker);
  // The marker should have only 1 child.
  LayoutObject* marker_child = marker->SlowFirstChild();
  EXPECT_TRUE(marker_child);
  EXPECT_FALSE(marker_child->NextSibling());
}

}  // namespace
}  // namespace blink

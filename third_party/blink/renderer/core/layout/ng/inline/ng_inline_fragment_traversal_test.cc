// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class NGInlineFragmentTraversalTest : public NGLayoutTest {
 public:
  NGInlineFragmentTraversalTest() : NGLayoutTest() {}

 protected:
  const NGPhysicalBoxFragment& GetRootFragmentById(const char* id) const {
    const Element* element = GetElementById(id);
    DCHECK(element) << id;
    const LayoutObject* layout_object = element->GetLayoutObject();
    DCHECK(layout_object) << element;
    DCHECK(layout_object->IsLayoutBlockFlow()) << element;
    DCHECK(To<LayoutBlockFlow>(layout_object)->CurrentFragment()) << element;
    return *To<LayoutBlockFlow>(layout_object)->CurrentFragment();
  }
};

#define EXPECT_NEXT_BOX(iter, id)                                           \
  {                                                                         \
    const auto& current = *iter++;                                          \
    EXPECT_TRUE(current.fragment->IsBox()) << current.fragment->ToString(); \
    EXPECT_EQ(GetLayoutObjectByElementId(id),                               \
              current.fragment->GetLayoutObject());                         \
  }

#define EXPECT_NEXT_LINE_BOX(iter)             \
  {                                            \
    const auto& current = *iter++;             \
    EXPECT_TRUE(current.fragment->IsLineBox()) \
        << current.fragment->ToString();       \
  }

#define EXPECT_NEXT_TEXT(iter, content)                                      \
  {                                                                          \
    const auto& current = *iter++;                                           \
    EXPECT_TRUE(current.fragment->IsText()) << current.fragment->ToString(); \
    EXPECT_EQ(content,                                                       \
              To<NGPhysicalTextFragment>(current.fragment.get())->Text());   \
  }

TEST_F(NGInlineFragmentTraversalTest, DescendantsOf) {
  SetBodyInnerHTML(
      "<style>* { border: 1px solid}</style>"
      "<div id=t>foo<b id=b>bar</b><br>baz</div>");
  const auto descendants =
      NGInlineFragmentTraversal::DescendantsOf(GetRootFragmentById("t"));
  auto* iter = descendants.begin();

  EXPECT_NEXT_LINE_BOX(iter);
  EXPECT_NEXT_TEXT(iter, "foo");
  EXPECT_NEXT_BOX(iter, "b");
  EXPECT_NEXT_TEXT(iter, "bar");
  EXPECT_NEXT_TEXT(iter, "\n");
  EXPECT_NEXT_LINE_BOX(iter);
  EXPECT_NEXT_TEXT(iter, "baz");
  EXPECT_EQ(iter, descendants.end());
}

}  // namespace blink

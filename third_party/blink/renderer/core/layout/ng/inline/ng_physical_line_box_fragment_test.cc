// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class NGPhysicalLineBoxFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalLineBoxFragmentTest() : NGLayoutTest() {}

 protected:
  Vector<const NGPhysicalLineBoxFragment*> GetLineBoxes() const {
    const Element* container = GetElementById("root");
    DCHECK(container);
    const LayoutObject* layout_object = container->GetLayoutObject();
    DCHECK(layout_object) << container;
    DCHECK(layout_object->IsLayoutBlockFlow()) << container;
    const NGPhysicalBoxFragment* root_fragment =
        To<LayoutBlockFlow>(layout_object)->CurrentFragment();
    DCHECK(root_fragment) << container;

    Vector<const NGPhysicalLineBoxFragment*> lines;
    for (const auto& child :
         NGInlineFragmentTraversal::DescendantsOf(*root_fragment)) {
      if (const NGPhysicalLineBoxFragment* line =
              DynamicTo<NGPhysicalLineBoxFragment>(child.fragment.get())) {
        lines.push_back(line);
      }
    }
    return lines;
  }

  const NGPhysicalLineBoxFragment* GetLineBox() const {
    Vector<const NGPhysicalLineBoxFragment*> lines = GetLineBoxes();
    if (!lines.IsEmpty())
      return lines.front();
    return nullptr;
  }
};

#define EXPECT_TEXT_FRAGMENT(text, fragment) \
  { EXPECT_EQ(text, To<NGPhysicalTextFragment>(fragment)->Text().ToString()); }

#define EXPECT_BOX_FRAGMENT(id, fragment)               \
  {                                                     \
    EXPECT_TRUE(fragment);                              \
    EXPECT_TRUE(fragment->IsBox());                     \
    EXPECT_TRUE(fragment->GetNode());                   \
    EXPECT_EQ(GetElementById(id), fragment->GetNode()); \
  }

TEST_F(NGPhysicalLineBoxFragmentTest, HasPropagatedDescendantsFloat) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      font-size: 10px;
      width: 10ch;
    }
    .float { float: left; }
    </style>
    <div id=root>12345678 12345<div class=float>float</div></div>
  )HTML");
  Vector<const NGPhysicalLineBoxFragment*> lines = GetLineBoxes();
  EXPECT_EQ(lines.size(), 2u);
  EXPECT_FALSE(lines[0]->HasPropagatedDescendants());
  EXPECT_TRUE(lines[1]->HasPropagatedDescendants());
}

TEST_F(NGPhysicalLineBoxFragmentTest, HasPropagatedDescendantsOOF) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      font-size: 10px;
      width: 10ch;
    }
    .abspos { position: absolute; }
    </style>
    <div id=root>12345678 12345<div class=abspos>abspos</div></div>
  )HTML");
  Vector<const NGPhysicalLineBoxFragment*> lines = GetLineBoxes();
  EXPECT_EQ(lines.size(), 2u);
  EXPECT_FALSE(lines[0]->HasPropagatedDescendants());
  EXPECT_TRUE(lines[1]->HasPropagatedDescendants());
}

}  // namespace blink

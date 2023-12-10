// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PhysicalLineBoxFragmentTest : public RenderingTest {
 protected:
  HeapVector<Member<const PhysicalLineBoxFragment>> GetLineBoxes() const {
    const Element* container = GetElementById("root");
    DCHECK(container);
    const LayoutObject* layout_object = container->GetLayoutObject();
    DCHECK(layout_object) << container;
    DCHECK(layout_object->IsLayoutBlockFlow()) << container;
    InlineCursor cursor(*To<LayoutBlockFlow>(layout_object));
    HeapVector<Member<const PhysicalLineBoxFragment>> lines;
    for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine())
      lines.push_back(cursor.Current()->LineBoxFragment());
    return lines;
  }

  const PhysicalLineBoxFragment* GetLineBox() const {
    HeapVector<Member<const PhysicalLineBoxFragment>> lines = GetLineBoxes();
    if (!lines.empty())
      return lines.front().Get();
    return nullptr;
  }
};

#define EXPECT_BOX_FRAGMENT(id, fragment)               \
  {                                                     \
    EXPECT_TRUE(fragment);                              \
    EXPECT_TRUE(fragment->IsBox());                     \
    EXPECT_TRUE(fragment->GetNode());                   \
    EXPECT_EQ(GetElementById(id), fragment->GetNode()); \
  }

TEST_F(PhysicalLineBoxFragmentTest, HasPropagatedDescendantsFloat) {
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
  HeapVector<Member<const PhysicalLineBoxFragment>> lines = GetLineBoxes();
  EXPECT_EQ(lines.size(), 2u);
  EXPECT_FALSE(lines[0]->HasPropagatedDescendants());
  EXPECT_TRUE(lines[1]->HasPropagatedDescendants());
}

TEST_F(PhysicalLineBoxFragmentTest, HasPropagatedDescendantsOOF) {
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
  HeapVector<Member<const PhysicalLineBoxFragment>> lines = GetLineBoxes();
  EXPECT_EQ(lines.size(), 2u);
  EXPECT_FALSE(lines[0]->HasPropagatedDescendants());
  EXPECT_TRUE(lines[1]->HasPropagatedDescendants());
}

}  // namespace blink

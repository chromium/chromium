// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using testing::ElementsAreArray;

class NGPaintFragmentTraversalTest : public RenderingTest,
                                     private ScopedLayoutNGForTest {
 public:
  NGPaintFragmentTraversalTest()
      : RenderingTest(nullptr), ScopedLayoutNGForTest(true) {}

 protected:
  void SetUpHtml(const char* container_id, const char* html) {
    SetBodyInnerHTML(html);
    layout_block_flow_ =
        To<LayoutBlockFlow>(GetLayoutObjectByElementId(container_id));
    root_fragment_ = layout_block_flow_->PaintFragment();
  }

  const NGPaintFragment::ChildList RootChildren() const {
    return root_fragment_->Children();
  }

  Vector<const NGPaintFragment*> ToDepthFirstList(
      NGPaintFragmentTraversal* traversal) const {
    Vector<const NGPaintFragment*> results;
    for (; *traversal; traversal->MoveToNext()) {
      const NGPaintFragment& fragment = **traversal;
      results.push_back(&fragment);
    }
    return results;
  }

  Vector<const NGPaintFragment*> ToReverseDepthFirstList(
      NGPaintFragmentTraversal* traversal) const {
    Vector<const NGPaintFragment*> results;
    for (; *traversal; traversal->MoveToPrevious()) {
      const NGPaintFragment& fragment = **traversal;
      results.push_back(&fragment);
    }
    return results;
  }

  Vector<NGPaintFragment*, 16> ToList(
      const NGPaintFragment::ChildList& children) {
    Vector<NGPaintFragment*, 16> list;
    children.ToList(&list);
    return list;
  }

  LayoutBlockFlow* layout_block_flow_;
  const NGPaintFragment* root_fragment_;
};

TEST_F(NGPaintFragmentTraversalTest, MoveToNext) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragmentTraversal traversal(*root_fragment_);
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* line1 = ToList(root_fragment_->Children())[1];
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  EXPECT_THAT(
      ToDepthFirstList(&traversal),
      ElementsAreArray({line0, line0->FirstChild(), span, span->FirstChild(),
                        br, line1, line1->FirstChild()}));
}

TEST_F(NGPaintFragmentTraversalTest, MoveToNextWithRoot) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  NGPaintFragmentTraversal traversal(*line0);
  EXPECT_THAT(
      ToDepthFirstList(&traversal),
      ElementsAreArray({line0->FirstChild(), span, span->FirstChild(), br}));
}

TEST_F(NGPaintFragmentTraversalTest, MoveToPrevious) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragmentTraversal traversal(*root_fragment_);
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* line1 = ToList(root_fragment_->Children())[1];
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  traversal.MoveTo(*line1->FirstChild());
  EXPECT_THAT(
      ToReverseDepthFirstList(&traversal),
      ElementsAreArray({line1->FirstChild(), line1, br, span->FirstChild(),
                        span, line0->FirstChild(), line0}));
}

TEST_F(NGPaintFragmentTraversalTest, MoveToPreviousWithRoot) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  NGPaintFragmentTraversal traversal(*line0);
  traversal.MoveTo(*br);
  EXPECT_THAT(
      ToReverseDepthFirstList(&traversal),
      ElementsAreArray({br, span->FirstChild(), span, line0->FirstChild()}));
}

TEST_F(NGPaintFragmentTraversalTest, MoveTo) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragmentTraversal traversal(*root_fragment_);
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* line1 = ToList(root_fragment_->Children())[1];
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  traversal.MoveTo(*span);
  EXPECT_EQ(span, &*traversal);
  EXPECT_THAT(ToDepthFirstList(&traversal),
              ElementsAreArray(
                  {span, span->FirstChild(), br, line1, line1->FirstChild()}));
}

TEST_F(NGPaintFragmentTraversalTest, MoveToWithRoot) {
  SetUpHtml("t", R"HTML(
    <div id=t>
      line0
      <span style="background: red">red</span>
      <br>
      line1
    </div>
  )HTML");
  NGPaintFragment* line0 = root_fragment_->FirstChild();
  NGPaintFragment* span = ToList(line0->Children())[1];
  NGPaintFragment* br = ToList(line0->Children())[2];
  NGPaintFragmentTraversal traversal(*line0);
  traversal.MoveTo(*span);
  EXPECT_EQ(span, &*traversal);
  EXPECT_THAT(ToDepthFirstList(&traversal),
              ElementsAreArray({span, span->FirstChild(), br}));
}

TEST_F(NGPaintFragmentTraversalTest, InlineDescendantsOf) {
  SetUpHtml("t",
            "<ul>"
            "<li id=t style='position: absolute'>"
            "<span style='float: right'>float</span>"
            "<span style='position: absolute'>oof</span>"
            "text<br>"
            "<span style='display: inline-block'>inline block</span>"
            "</li>"
            "</ul>");

  // Tests that floats, out-of-flow positioned and descendants of atomic inlines
  // are excluded.
  Vector<const NGPaintFragment*> descendants;
  for (const NGPaintFragment* fragment :
       NGPaintFragmentTraversal::InlineDescendantsOf(*root_fragment_))
    descendants.push_back(fragment);
  ASSERT_EQ(6u, descendants.size());
  // TODO(layout-dev): This list marker is not in any line box. Should it be
  // treated as inline?
  EXPECT_TRUE(descendants[0]->PhysicalFragment().IsListMarker());
  EXPECT_TRUE(descendants[1]->PhysicalFragment().IsLineBox());
  EXPECT_TRUE(descendants[2]->PhysicalFragment().IsText());  // "text"
  EXPECT_TRUE(descendants[3]->PhysicalFragment().IsText());  // "br"
  EXPECT_TRUE(descendants[4]->PhysicalFragment().IsLineBox());
  EXPECT_TRUE(descendants[5]->PhysicalFragment().IsAtomicInline());
}

}  // namespace blink

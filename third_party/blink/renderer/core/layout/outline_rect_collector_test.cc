// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/outline_rect_collector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(OutlineRectCollectorTest, Empty) {
  test::TaskEnvironment task_environment;
  UnionOutlineRectCollector u;
  VectorOutlineRectCollector v;

  EXPECT_EQ(u.Rect(), PhysicalRect());
  EXPECT_TRUE(v.TakeRects().empty());
}

TEST(OutlineRectCollectorTest, AddRect) {
  test::TaskEnvironment task_environment;
  Vector<Vector<PhysicalRect>> tests = {
      Vector<PhysicalRect>{
          PhysicalRect(-1, -1, 10, 10), PhysicalRect(10, 20, 30, 40),
          PhysicalRect(1, 2, 3, 4), PhysicalRect(1, -1, 10, 15),
          PhysicalRect(-31, -15, 11, 16)},
      Vector<PhysicalRect>{PhysicalRect(1, 2, 3, 4)},
      Vector<PhysicalRect>{PhysicalRect(10, 20, 30, 40),
                           PhysicalRect(15, 25, 35, 45)},
      Vector<PhysicalRect>{PhysicalRect(-100, -200, 30, 40),
                           PhysicalRect(-150, -250, 35, 45)}};

  ASSERT_FALSE(tests.empty());
  for (wtf_size_t i = 0; i < tests.size(); ++i) {
    SCOPED_TRACE(i);

    const Vector<PhysicalRect>& input_rects = tests[i];
    UnionOutlineRectCollector u;
    VectorOutlineRectCollector v;

    for (auto& rect : input_rects) {
      u.AddRect(rect);
      v.AddRect(rect);
    }

    PhysicalRect union_result = u.Rect();
    VectorOf<PhysicalRect> vector_result = v.TakeRects();

    EXPECT_EQ(input_rects, vector_result);
    EXPECT_EQ(UnionRect(input_rects), union_result);
  }
}

TEST(OutlineRectCollectorTest, CombineWithOffset) {
  test::TaskEnvironment task_environment;
  UnionOutlineRectCollector u;
  VectorOutlineRectCollector v;

  u.AddRect(PhysicalRect(10, 20, 30, 40));
  v.AddRect(PhysicalRect(10, 20, 30, 40));

  std::unique_ptr<OutlineRectCollector> u_descendant =
      u.ForDescendantCollector();
  std::unique_ptr<OutlineRectCollector> v_descendant =
      v.ForDescendantCollector();

  u_descendant->AddRect(PhysicalRect(10, 20, 30, 40));
  v_descendant->AddRect(PhysicalRect(10, 20, 30, 40));

  u.Combine(u_descendant.get(), PhysicalOffset(15, -25));
  v.Combine(v_descendant.get(), PhysicalOffset(15, -25));

  PhysicalRect union_result = u.Rect();
  VectorOf<PhysicalRect> vector_result = v.TakeRects();

  EXPECT_EQ(union_result, PhysicalRect(10, -5, 45, 65));
  EXPECT_EQ(vector_result,
            (Vector<PhysicalRect>{PhysicalRect(10, 20, 30, 40),
                                  PhysicalRect(25, -5, 30, 40)}));
}

class OutlineRectCollectorRenderingTest : public RenderingTest {
 public:
  OutlineRectCollectorRenderingTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()) {}
};

TEST_F(OutlineRectCollectorRenderingTest, CombineWithAncestor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { contain: paint; width: 100px; height: 100px; }
      #parent { position: absolute; left: 10px; top: 20px; }
      #child { position: relative; left: 15px; top: 25px; }
    </style>
    <div id=parent>
      <div id=child></div>
    </div>
  )HTML");

  LayoutBoxModelObject* parent =
      DynamicTo<LayoutBoxModelObject>(GetLayoutObjectByElementId("parent"));
  LayoutObject* child = GetLayoutObjectByElementId("child");
  ASSERT_TRUE(parent);
  ASSERT_TRUE(child);

  UnionOutlineRectCollector u;
  VectorOutlineRectCollector v;

  u.AddRect(PhysicalRect(10, 20, 30, 40));
  v.AddRect(PhysicalRect(10, 20, 30, 40));

  std::unique_ptr<OutlineRectCollector> u_descendant =
      u.ForDescendantCollector();
  std::unique_ptr<OutlineRectCollector> v_descendant =
      v.ForDescendantCollector();

  u_descendant->AddRect(PhysicalRect(10, 20, 30, 40));
  v_descendant->AddRect(PhysicalRect(10, 20, 30, 40));

  u.Combine(u_descendant.get(), *child, parent, PhysicalOffset(15, -25));
  // The mapped rect should be:
  // x:
  // 10 (physical rect in add rect)
  // + 15 (left: 15px in styles) +
  // + 15 (offset in the combine call)
  // = 40
  //
  // y:
  // 20 (physical rect in add rect)
  // + 25 (top: 25px in styles)
  // - 25 (offset in the combine call)
  // = 20
  //
  // width and height should be unchanged.
  v.Combine(v_descendant.get(), *child, parent, PhysicalOffset(15, -25));

  PhysicalRect union_result = u.Rect();
  VectorOf<PhysicalRect> vector_result = v.TakeRects();

  EXPECT_EQ(union_result, PhysicalRect(10, 20, 60, 40));
  EXPECT_EQ(vector_result,
            (Vector<PhysicalRect>{PhysicalRect(10, 20, 30, 40),
                                  PhysicalRect(40, 20, 30, 40)}));
}

}  // namespace blink

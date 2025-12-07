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

TEST(OutlineRectCollectorTest, AddEmptyRect) {
  test::TaskEnvironment task_environment;
  UnionOutlineRectCollector u;
  VectorOutlineRectCollector v;
  EXPECT_TRUE(u.IsEmpty());
  EXPECT_EQ(PhysicalRect(), u.Rect());
  EXPECT_TRUE(v.IsEmpty());

  u.AddRect(PhysicalRect(10, 10, 0, 0));
  v.AddRect(PhysicalRect(10, 10, 0, 0));
  EXPECT_FALSE(u.IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 0, 0), u.Rect());
  EXPECT_FALSE(v.IsEmpty());

  u.AddRect(PhysicalRect(100, 100, 100, 0));
  v.AddRect(PhysicalRect(100, 100, 100, 0));
  EXPECT_FALSE(u.IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 190, 90), u.Rect());
  EXPECT_FALSE(v.IsEmpty());
  EXPECT_EQ(2u, v.TakeRects().size());
}

TEST(OutlineRectCollectorTest, UnionCombineWithEmptyRects) {
  test::TaskEnvironment task_environment;
  UnionOutlineRectCollector u1, u2;
  EXPECT_TRUE(u1.IsEmpty());
  EXPECT_TRUE(u2.IsEmpty());
  u1.Combine(&u2, PhysicalOffset());
  EXPECT_TRUE(u1.IsEmpty());

  UnionOutlineRectCollector u3;
  u3.AddRect(PhysicalRect(100, 100, 100, 0));
  u1.Combine(&u3, PhysicalOffset());
  EXPECT_FALSE(u1.IsEmpty());
  EXPECT_EQ(PhysicalRect(100, 100, 100, 0), u1.Rect());
}

TEST(OutlineRectCollectorTest, VectorCombineWithEmptyRects) {
  test::TaskEnvironment task_environment;
  VectorOutlineRectCollector v1, v2;
  EXPECT_TRUE(v1.IsEmpty());
  EXPECT_TRUE(v2.IsEmpty());
  v1.Combine(&v2, PhysicalOffset());
  EXPECT_TRUE(v1.IsEmpty());

  VectorOutlineRectCollector v3;
  v3.AddRect(PhysicalRect(100, 100, 100, 0));
  v1.Combine(&v3, PhysicalOffset());
  EXPECT_FALSE(v1.IsEmpty());
  EXPECT_EQ(Vector<PhysicalRect>{PhysicalRect(100, 100, 100, 0)},
            v1.TakeRects());
}

class OutlineRectCollectorRenderingTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();

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

    parent_ = GetLayoutBoxByElementId("parent");
    child_ = GetLayoutObjectByElementId("child");
    CHECK(parent_);
    CHECK(child_);
  }

  Persistent<LayoutBox> parent_;
  Persistent<LayoutObject> child_;
};

TEST_F(OutlineRectCollectorRenderingTest, CombineWithAncestor) {
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

  u.Combine(u_descendant.get(), *child_, parent_, PhysicalOffset(15, -25));
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
  v.Combine(v_descendant.get(), *child_, parent_, PhysicalOffset(15, -25));

  PhysicalRect union_result = u.Rect();
  VectorOf<PhysicalRect> vector_result = v.TakeRects();

  EXPECT_EQ(union_result, PhysicalRect(10, 20, 60, 40));
  EXPECT_EQ(vector_result,
            (Vector<PhysicalRect>{PhysicalRect(10, 20, 30, 40),
                                  PhysicalRect(40, 20, 30, 40)}));
}

TEST_F(OutlineRectCollectorRenderingTest, UnionCombineWithEmptyRects) {
  UnionOutlineRectCollector u;
  auto u_empty = u.ForDescendantCollector();
  EXPECT_TRUE(u_empty->IsEmpty());
  u.Combine(u_empty.get(), *child_, parent_, PhysicalOffset(15, -25));
  EXPECT_TRUE(u.IsEmpty());

  auto u_descendant = u.ForDescendantCollector();
  u_descendant->AddRect(PhysicalRect(10, 20, 30, 0));
  EXPECT_FALSE(u_descendant->IsEmpty());
  u.Combine(u_descendant.get(), *child_, parent_, PhysicalOffset(15, -25));
  EXPECT_FALSE(u.IsEmpty());
  EXPECT_EQ(PhysicalRect(40, 20, 30, 0), u.Rect());
}

TEST_F(OutlineRectCollectorRenderingTest, VectorCombineWithEmptyRects) {
  VectorOutlineRectCollector v;
  auto v_empty = v.ForDescendantCollector();
  EXPECT_TRUE(v_empty->IsEmpty());
  v.Combine(v_empty.get(), *child_, parent_, PhysicalOffset(15, -25));
  EXPECT_TRUE(v.IsEmpty());

  auto v_descendant = v.ForDescendantCollector();
  v_descendant->AddRect(PhysicalRect(10, 20, 30, 0));
  EXPECT_FALSE(v_descendant->IsEmpty());
  v.Combine(v_descendant.get(), *child_, parent_, PhysicalOffset(15, -25));
  EXPECT_FALSE(v.IsEmpty());
  EXPECT_EQ(Vector<PhysicalRect>{PhysicalRect(40, 20, 30, 0)}, v.TakeRects());
}

}  // namespace blink

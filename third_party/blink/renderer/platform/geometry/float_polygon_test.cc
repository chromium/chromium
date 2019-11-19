/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_polygon.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FloatPolygonTestValue {
  STACK_ALLOCATED();

 public:
  FloatPolygonTestValue(const float* coordinates, unsigned coordinates_length) {
    DCHECK(!(coordinates_length % 2));
    Vector<FloatPoint> vertices(coordinates_length / 2);
    for (unsigned i = 0; i < coordinates_length; i += 2)
      vertices[i / 2] = FloatPoint(coordinates[i], coordinates[i + 1]);
    polygon_ = std::make_unique<FloatPolygon>(std::move(vertices));
  }

  const FloatPolygon& Polygon() const { return *polygon_; }

 private:
  std::unique_ptr<FloatPolygon> polygon_;
};

namespace {

bool CompareEdgeIndex(const FloatPolygonEdge* edge1,
                      const FloatPolygonEdge* edge2) {
  return edge1->EdgeIndex() < edge2->EdgeIndex();
}

Vector<const FloatPolygonEdge*>
SortedOverlappingEdges(const FloatPolygon& polygon, float min_y, float max_y) {
  Vector<const FloatPolygonEdge*> result;
  polygon.OverlappingEdges(min_y, max_y, result);
  std::sort(result.begin(), result.end(), CompareEdgeIndex);
  return result;
}

}  // anonymous namespace

#define SIZEOF_ARRAY(p) (sizeof(p) / sizeof(p[0]))

/**
 * Checks a right triangle. This test covers all of the trivial FloatPolygon
 * accessors.
 *
 *                        200,100
 *                          /|
 *                         / |
 *                        /  |
 *                       -----
 *                 100,200   200,200
 */
TEST(FloatPolygonTest, basics) {
  const float kTriangleCoordinates[] = {200, 100, 200, 200, 100, 200};
  FloatPolygonTestValue triangle_test_value(kTriangleCoordinates,
                                            SIZEOF_ARRAY(kTriangleCoordinates));
  const FloatPolygon& triangle = triangle_test_value.Polygon();

  EXPECT_FALSE(triangle.IsEmpty());

  EXPECT_EQ(3u, triangle.NumberOfVertices());
  EXPECT_EQ(FloatPoint(200, 100), triangle.VertexAt(0));
  EXPECT_EQ(FloatPoint(200, 200), triangle.VertexAt(1));
  EXPECT_EQ(FloatPoint(100, 200), triangle.VertexAt(2));

  EXPECT_EQ(3u, triangle.NumberOfEdges());
  EXPECT_EQ(FloatPoint(200, 100), triangle.EdgeAt(0).Vertex1());
  EXPECT_EQ(FloatPoint(200, 200), triangle.EdgeAt(0).Vertex2());
  EXPECT_EQ(FloatPoint(200, 200), triangle.EdgeAt(1).Vertex1());
  EXPECT_EQ(FloatPoint(100, 200), triangle.EdgeAt(1).Vertex2());
  EXPECT_EQ(FloatPoint(100, 200), triangle.EdgeAt(2).Vertex1());
  EXPECT_EQ(FloatPoint(200, 100), triangle.EdgeAt(2).Vertex2());

  EXPECT_EQ(0u, triangle.EdgeAt(0).VertexIndex1());
  EXPECT_EQ(1u, triangle.EdgeAt(0).VertexIndex2());
  EXPECT_EQ(1u, triangle.EdgeAt(1).VertexIndex1());
  EXPECT_EQ(2u, triangle.EdgeAt(1).VertexIndex2());
  EXPECT_EQ(2u, triangle.EdgeAt(2).VertexIndex1());
  EXPECT_EQ(0u, triangle.EdgeAt(2).VertexIndex2());

  EXPECT_EQ(200, triangle.EdgeAt(0).MinX());
  EXPECT_EQ(200, triangle.EdgeAt(0).MaxX());
  EXPECT_EQ(100, triangle.EdgeAt(1).MinX());
  EXPECT_EQ(200, triangle.EdgeAt(1).MaxX());
  EXPECT_EQ(100, triangle.EdgeAt(2).MinX());
  EXPECT_EQ(200, triangle.EdgeAt(2).MaxX());

  EXPECT_EQ(100, triangle.EdgeAt(0).MinY());
  EXPECT_EQ(200, triangle.EdgeAt(0).MaxY());
  EXPECT_EQ(200, triangle.EdgeAt(1).MinY());
  EXPECT_EQ(200, triangle.EdgeAt(1).MaxY());
  EXPECT_EQ(100, triangle.EdgeAt(2).MinY());
  EXPECT_EQ(200, triangle.EdgeAt(2).MaxY());

  EXPECT_EQ(0u, triangle.EdgeAt(0).EdgeIndex());
  EXPECT_EQ(1u, triangle.EdgeAt(1).EdgeIndex());
  EXPECT_EQ(2u, triangle.EdgeAt(2).EdgeIndex());

  EXPECT_EQ(2u, triangle.EdgeAt(0).PreviousEdge().EdgeIndex());
  EXPECT_EQ(1u, triangle.EdgeAt(0).NextEdge().EdgeIndex());
  EXPECT_EQ(0u, triangle.EdgeAt(1).PreviousEdge().EdgeIndex());
  EXPECT_EQ(2u, triangle.EdgeAt(1).NextEdge().EdgeIndex());
  EXPECT_EQ(1u, triangle.EdgeAt(2).PreviousEdge().EdgeIndex());
  EXPECT_EQ(0u, triangle.EdgeAt(2).NextEdge().EdgeIndex());

  EXPECT_EQ(FloatRect(100, 100, 100, 100), triangle.BoundingBox());

  Vector<const FloatPolygonEdge*> result_a =
      SortedOverlappingEdges(triangle, 100, 200);
  EXPECT_EQ(3u, result_a.size());
  if (result_a.size() == 3) {
    EXPECT_EQ(0u, result_a[0]->EdgeIndex());
    EXPECT_EQ(1u, result_a[1]->EdgeIndex());
    EXPECT_EQ(2u, result_a[2]->EdgeIndex());
  }

  Vector<const FloatPolygonEdge*> result_b =
      SortedOverlappingEdges(triangle, 200, 200);
  EXPECT_EQ(3u, result_b.size());
  if (result_b.size() == 3) {
    EXPECT_EQ(0u, result_b[0]->EdgeIndex());
    EXPECT_EQ(1u, result_b[1]->EdgeIndex());
    EXPECT_EQ(2u, result_b[2]->EdgeIndex());
  }

  Vector<const FloatPolygonEdge*> result_c =
      SortedOverlappingEdges(triangle, 100, 150);
  EXPECT_EQ(2u, result_c.size());
  if (result_c.size() == 2) {
    EXPECT_EQ(0u, result_c[0]->EdgeIndex());
    EXPECT_EQ(2u, result_c[1]->EdgeIndex());
  }

  Vector<const FloatPolygonEdge*> result_d =
      SortedOverlappingEdges(triangle, 201, 300);
  EXPECT_EQ(0u, result_d.size());

  Vector<const FloatPolygonEdge*> result_e =
      SortedOverlappingEdges(triangle, 98, 99);
  EXPECT_EQ(0u, result_e.size());
}

/**
 * Tests ContainsNonZero and ContainsEvenOdd with a right triangle.
 *
 *                        200,100
 *                          /|
 *                         / |
 *                        /  |
 *                       -----
 *                 100,200   200,200
 */
TEST(FloatPolygonTest, triangle_nonzero) {
  const float kTriangleCoordinates[] = {200, 100, 200, 200, 100, 200};
  FloatPolygonTestValue triangle_test_value(kTriangleCoordinates,
                                            SIZEOF_ARRAY(kTriangleCoordinates));
  const FloatPolygon& triangle = triangle_test_value.Polygon();

  EXPECT_TRUE(triangle.ContainsNonZero(FloatPoint(200, 100)));
  EXPECT_TRUE(triangle.ContainsNonZero(FloatPoint(200, 200)));
  EXPECT_TRUE(triangle.ContainsNonZero(FloatPoint(100, 200)));
  EXPECT_TRUE(triangle.ContainsNonZero(FloatPoint(150, 150)));
  EXPECT_FALSE(triangle.ContainsNonZero(FloatPoint(100, 100)));
  EXPECT_FALSE(triangle.ContainsNonZero(FloatPoint(149, 149)));
  EXPECT_FALSE(triangle.ContainsNonZero(FloatPoint(150, 200.5)));
  EXPECT_FALSE(triangle.ContainsNonZero(FloatPoint(201, 200.5)));

  EXPECT_TRUE(triangle.ContainsEvenOdd(FloatPoint(200, 100)));
  EXPECT_TRUE(triangle.ContainsEvenOdd(FloatPoint(200, 200)));
  EXPECT_TRUE(triangle.ContainsEvenOdd(FloatPoint(100, 200)));
  EXPECT_TRUE(triangle.ContainsEvenOdd(FloatPoint(150, 150)));
  EXPECT_FALSE(triangle.ContainsEvenOdd(FloatPoint(100, 100)));
  EXPECT_FALSE(triangle.ContainsEvenOdd(FloatPoint(149, 149)));
  EXPECT_FALSE(triangle.ContainsEvenOdd(FloatPoint(150, 200.5)));
  EXPECT_FALSE(triangle.ContainsEvenOdd(FloatPoint(201, 200.5)));
}

#define TEST_EMPTY(coordinates)                                                \
  {                                                                            \
    FloatPolygonTestValue empty_polygon_test_value(coordinates,                \
                                                   SIZEOF_ARRAY(coordinates)); \
    const FloatPolygon& empty_polygon = empty_polygon_test_value.Polygon();    \
    EXPECT_TRUE(empty_polygon.IsEmpty());                                      \
  }

TEST(FloatPolygonTest, emptyPolygons) {
  const float kEmptyCoordinates1[] = {0, 0};
  TEST_EMPTY(kEmptyCoordinates1);

  const float kEmptyCoordinates2[] = {0, 0, 1, 1};
  TEST_EMPTY(kEmptyCoordinates2);

  const float kEmptyCoordinates3[] = {0, 0, 1, 1, 2, 2, 3, 3};
  TEST_EMPTY(kEmptyCoordinates3);

  const float kEmptyCoordinates4[] = {0, 0, 1, 1, 2, 2, 3, 3, 1, 1};
  TEST_EMPTY(kEmptyCoordinates4);

  const float kEmptyCoordinates5[] = {0, 0, 0, 1, 0, 2, 0, 3, 0, 1};
  TEST_EMPTY(kEmptyCoordinates5);

  const float kEmptyCoordinates6[] = {0, 0, 1, 0, 2, 0, 3, 0, 1, 0};
  TEST_EMPTY(kEmptyCoordinates6);
}

/*
 * Test FloatPolygon::ContainsEvenOdd() with a trapezoid. The vertices are
 * listed in counter-clockwise order.
 *
 *        150,100   250,100
 *          +----------+
 *         /            \
 *        /              \
 *       +----------------+
 *     100,150          300,150
 */
TEST(FloatPolygonTest, trapezoid) {
  const float kTrapezoidCoordinates[] = {100, 150, 300, 150,
                                         250, 100, 150, 100};
  FloatPolygonTestValue trapezoid_test_value(
      kTrapezoidCoordinates, SIZEOF_ARRAY(kTrapezoidCoordinates));
  const FloatPolygon& trapezoid = trapezoid_test_value.Polygon();

  EXPECT_FALSE(trapezoid.IsEmpty());
  EXPECT_EQ(4u, trapezoid.NumberOfVertices());
  EXPECT_EQ(FloatRect(100, 100, 200, 50), trapezoid.BoundingBox());

  EXPECT_TRUE(trapezoid.ContainsEvenOdd(FloatPoint(150, 100)));
  EXPECT_TRUE(trapezoid.ContainsEvenOdd(FloatPoint(150, 101)));
  EXPECT_TRUE(trapezoid.ContainsEvenOdd(FloatPoint(200, 125)));
  EXPECT_FALSE(trapezoid.ContainsEvenOdd(FloatPoint(149, 100)));
  EXPECT_FALSE(trapezoid.ContainsEvenOdd(FloatPoint(301, 150)));
}

/*
 * Test FloatPolygon::ContainsNonZero() with a non-convex rectilinear polygon.
 * The polygon has the same shape as the letter "H":
 *
 *    100,100  150,100   200,100   250,100
 *       +--------+        +--------+
 *       |        |        |        |
 *       |        |        |        |
 *       |        +--------+        |
 *       |     150,150   200,150    |
 *       |                          |
 *       |     150,200   200,200    |
 *       |        +--------+        |
 *       |        |        |        |
 *       |        |        |        |
 *       +--------+        +--------+
 *    100,250  150,250   200,250   250,250
 */
TEST(FloatPolygonTest, rectilinear) {
  const float kHCoordinates[] = {100, 100, 150, 100, 150, 150, 200, 150,
                                 200, 100, 250, 100, 250, 250, 200, 250,
                                 200, 200, 150, 200, 150, 250, 100, 250};
  FloatPolygonTestValue h_test_value(kHCoordinates,
                                     SIZEOF_ARRAY(kHCoordinates));
  const FloatPolygon& h = h_test_value.Polygon();

  EXPECT_FALSE(h.IsEmpty());
  EXPECT_EQ(12u, h.NumberOfVertices());
  EXPECT_EQ(FloatRect(100, 100, 150, 150), h.BoundingBox());

  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(100, 100)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(125, 100)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(125, 125)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(150, 100)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(200, 200)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(225, 225)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(250, 250)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(100, 250)));
  EXPECT_TRUE(h.ContainsNonZero(FloatPoint(125, 250)));

  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(99, 100)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(251, 100)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(151, 100)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(199, 100)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(175, 125)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(151, 250)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(199, 250)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(199, 250)));
  EXPECT_FALSE(h.ContainsNonZero(FloatPoint(175, 225)));
}

}  // namespace blink

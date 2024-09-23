// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace media_constraints {

using media_constraints::ResolutionSet;
using Point = ResolutionSet::Point;
using BoolSet = media_constraints::DiscreteSet<bool>;

namespace {

const int kDefaultWidth = 640;
const int kDefaultHeight = 480;
constexpr double kDefaultAspectRatio =
    static_cast<double>(kDefaultWidth) / static_cast<double>(kDefaultHeight);

// Defined as macro in order to get more informative line-number information
// when a test fails.
#define EXPECT_POINT_EQ(p1, p2)                     \
  do {                                              \
    EXPECT_DOUBLE_EQ((p1).height(), (p2).height()); \
    EXPECT_DOUBLE_EQ((p1).width(), (p2).width());   \
  } while (0)

// Checks if |point| is an element of |vertices| using
// Point::IsApproximatelyEqualTo() to test for equality.
void VerticesContain(const Vector<Point>& vertices, const Point& point) {
  bool result = false;
  for (const auto& vertex : vertices) {
    if (point.IsApproximatelyEqualTo(vertex)) {
      result = true;
      break;
    }
  }
  EXPECT_TRUE(result);
}

bool AreCounterclockwise(const Vector<Point>& vertices) {
  // Single point or segment are trivial cases.
  if (vertices.size() <= 2)
    return true;
  else if (vertices.size() > 6)  // Polygons of more than 6 sides are not valid.
    return false;

  // The polygon defined by a resolution set is always convex and has at most 6
  // sides. When producing a list of the vertices for such a polygon, it is
  // important that they are returned in counterclockwise (or clockwise) order,
  // to make sure that any consecutive pair of vertices (modulo the number of
  // vertices) corresponds to a polygon side. Our implementation uses
  // counterclockwise order.
  // Compute orientation using the determinant of each diagonal in the
  // polygon, using the first vertex as reference.
  Point prev_diagonal = vertices[1] - vertices[0];
  for (auto vertex = vertices.begin() + 2; vertex != vertices.end(); ++vertex) {
    Point current_diagonal = *vertex - vertices[0];
    // The determinant of the two diagonals returns the signed area of the
    // parallelogram they generate. The area is positive if the diagonals are in
    // counterclockwise order, zero if the diagonals have the same direction and
    // negative if the diagonals are in clockwise order.
    // See https://en.wikipedia.org/wiki/Determinant#2_.C3.97_2_matrices.
    double det = prev_diagonal.height() * current_diagonal.width() -
                 current_diagonal.height() * prev_diagonal.width();
    if (det <= 0)
      return false;
    prev_diagonal = current_diagonal;
  }
  return true;
}

// Determines if |vertices| is valid according to the contract for
// ResolutionCandidateSet::ComputeVertices().
bool AreValidVertices(const ResolutionSet& set, const Vector<Point>& vertices) {
  // Verify that every vertex is included in |set|.
  for (const auto& vertex : vertices) {
    if (!set.ContainsPoint(vertex))
      return false;
  }

  return AreCounterclockwise(vertices);
}

// This function provides an alternative method for computing the projection
// of |point| on the line of segment |s1||s2| to be used to compare the results
// provided by Point::ClosestPointInSegment(). Since it relies on library
// functions, it has larger error in practice than
// Point::ClosestPointInSegment(), so results must be compared with
// Point::IsApproximatelyEqualTo().
// This function only computes projections. The result may be outside the
// segment |s1||s2|.
Point ProjectionOnSegmentLine(const Point& point,
                              const Point& s1,
                              const Point& s2) {
  double segment_slope =
      (s2.width() - s1.width()) / (s2.height() - s1.height());
  double segment_angle = std::atan(segment_slope);
  double norm = std::sqrt(Point::Dot(point, point));
  double angle =
      (point.height() == 0 && point.width() == 0)
          ? 0.0
          : std::atan(point.width() / point.height()) - segment_angle;
  double projection_length = norm * std::cos(angle);
  double projection_height = projection_length * std::cos(segment_angle);
  double projection_width = projection_length * std::sin(segment_angle);
  return Point(projection_height, projection_width);
}

}  // namespace

class MediaStreamConstraintsUtilSetsTest : public testing::Test {
 protected:
  using P = Point;

  Point SelectClosestPointToIdeal(const ResolutionSet& set) {
    return set.SelectClosestPointToIdeal(
        factory_.CreateMediaConstraints().Basic(), kDefaultHeight,
        kDefaultWidth);
  }

  test::TaskEnvironment task_environment_;
  MockConstraintFactory factory_;
};

// This test tests the test-harness function AreValidVertices.
TEST_F(MediaStreamConstraintsUtilSetsTest, VertexListValidity) {
  EXPECT_TRUE(AreCounterclockwise({P(1, 1)}));
  EXPECT_TRUE(AreCounterclockwise({P(1, 1)}));
  EXPECT_TRUE(AreCounterclockwise({P(1, 0), P(0, 1)}));
  EXPECT_TRUE(AreCounterclockwise({P(1, 1), P(0, 0), P(1, 0)}));

  // Not in counterclockwise order.
  EXPECT_FALSE(AreCounterclockwise({P(1, 0), P(0, 0), P(1, 1)}));

  // Final vertex aligned with the previous two vertices.
  EXPECT_FALSE(AreCounterclockwise({P(1, 0), P(1, 1), P(1, 1.5), P(1, 0.1)}));

  // Not in counterclockwise order.
  EXPECT_FALSE(
      AreCounterclockwise({P(1, 0), P(3, 0), P(2, 2), P(3.1, 1), P(0, 1)}));

  EXPECT_TRUE(AreCounterclockwise(
      {P(1, 0), P(3, 0), P(3.1, 1), P(3, 2), P(1, 2), P(0.9, 1)}));

  // Not in counterclockwise order.
  EXPECT_FALSE(AreCounterclockwise(
      {P(1, 0), P(3, 0), P(3.1, 1), P(1, 2), P(3, 2), P(0.9, 1)}));

  // Counterclockwise, but more than 6 vertices.
  EXPECT_FALSE(AreCounterclockwise(
      {P(1, 0), P(3, 0), P(3.1, 1), P(3, 2), P(2, 2.1), P(1, 2), P(0.9, 1)}));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, PointOperations) {
  const Point kZero(0, 0);

  // Basic equality and inequality
  EXPECT_EQ(P(0, 0), kZero);
  EXPECT_EQ(P(50, 50), P(50, 50));
  EXPECT_NE(kZero, P(50, 50));
  EXPECT_NE(P(50, 50), P(100, 100));
  EXPECT_NE(P(50, 50), P(100, 50));

  // Operations with zero.
  EXPECT_EQ(kZero, kZero + kZero);
  EXPECT_EQ(kZero, kZero - kZero);
  EXPECT_EQ(kZero, 0.0 * kZero);
  EXPECT_EQ(0.0, P::Dot(kZero, kZero));
  EXPECT_EQ(0.0, P::SquareEuclideanDistance(kZero, kZero));
  EXPECT_EQ(kZero, P::ClosestPointInSegment(kZero, kZero, kZero));

  // Operations with zero and nonzero values.
  EXPECT_EQ(P(50, 50), kZero + P(50, 50));
  EXPECT_EQ(P(50, 50) + kZero, kZero + P(50, 50));
  EXPECT_EQ(P(50, 50), P(50, 50) - kZero);
  EXPECT_EQ(kZero, P(50, 50) - P(50, 50));
  EXPECT_EQ(kZero, 0.0 * P(50, 50));
  EXPECT_EQ(0.0, P::Dot(kZero, P(50, 50)));
  EXPECT_EQ(0.0, P::Dot(P(50, 50), kZero));
  EXPECT_EQ(5000, P::SquareEuclideanDistance(kZero, P(50, 50)));
  EXPECT_EQ(P::SquareEuclideanDistance(P(50, 50), kZero),
            P::SquareEuclideanDistance(kZero, P(50, 50)));
  EXPECT_EQ(kZero, P::ClosestPointInSegment(kZero, kZero, P(50, 50)));
  EXPECT_EQ(kZero, P::ClosestPointInSegment(kZero, P(50, 50), kZero));
  EXPECT_EQ(P(50, 50),
            P::ClosestPointInSegment(P(50, 50), P(50, 50), P(50, 50)));

  // Operations with nonzero values.
  // Additions.
  EXPECT_EQ(P(100, 50), P(50, 50) + P(50, 0));
  EXPECT_EQ(P(100, 50), P(50, 0) + P(50, 50));

  // Substractions.
  EXPECT_EQ(P(50, 50), P(100, 100) - P(50, 50));
  EXPECT_EQ(P(50, 50), P(100, 50) - P(50, 0));
  EXPECT_EQ(P(50, 0), P(100, 50) - P(50, 50));

  // Scalar-vector products.
  EXPECT_EQ(P(50, 50), 1.0 * P(50, 50));
  EXPECT_EQ(P(75, 75), 1.5 * P(50, 50));
  EXPECT_EQ(P(200, 100), 2.0 * P(100, 50));
  EXPECT_EQ(2.0 * (P(100, 100) + P(100, 50)),
            2.0 * P(100, 100) + 2.0 * P(100, 50));

  // Dot products.
  EXPECT_EQ(2 * 50 * 100, P::Dot(P(50, 50), P(100, 100)));
  EXPECT_EQ(P::Dot(P(100, 100), P(50, 50)), P::Dot(P(50, 50), P(100, 100)));
  EXPECT_EQ(0, P::Dot(P(100, 0), P(0, 100)));

  // Distances.
  EXPECT_EQ(25, P::SquareEuclideanDistance(P(4, 0), P(0, 3)));
  EXPECT_EQ(75 * 75, P::SquareEuclideanDistance(P(100, 0), P(25, 0)));
  EXPECT_EQ(75 * 75, P::SquareEuclideanDistance(P(0, 100), P(0, 25)));
  EXPECT_EQ(5 * 5 + 9 * 9, P::SquareEuclideanDistance(P(5, 1), P(10, 10)));

  // Closest point to segment from (10,0) to (50,0).
  EXPECT_EQ(P(25, 0), P::ClosestPointInSegment(P(25, 25), P(10, 0), P(50, 0)));
  EXPECT_EQ(P(50, 0),
            P::ClosestPointInSegment(P(100, 100), P(10, 0), P(50, 0)));
  EXPECT_EQ(P(10, 0), P::ClosestPointInSegment(P(0, 100), P(10, 0), P(50, 0)));

  // Closest point to segment from (0,10) to (0,50).
  EXPECT_EQ(P(0, 25), P::ClosestPointInSegment(P(25, 25), P(0, 10), P(0, 50)));
  EXPECT_EQ(P(0, 50),
            P::ClosestPointInSegment(P(100, 100), P(0, 10), P(0, 50)));
  EXPECT_EQ(P(0, 10), P::ClosestPointInSegment(P(100, 0), P(0, 10), P(0, 50)));

  // Closest point to segment from (0,10) to (10,0).
  EXPECT_EQ(P(5, 5), P::ClosestPointInSegment(P(25, 25), P(0, 10), P(10, 0)));
  EXPECT_EQ(P(5, 5), P::ClosestPointInSegment(P(100, 100), P(0, 10), P(10, 0)));
  EXPECT_EQ(P(10, 0), P::ClosestPointInSegment(P(100, 0), P(0, 10), P(10, 0)));
  EXPECT_EQ(P(0, 10), P::ClosestPointInSegment(P(0, 100), P(0, 10), P(10, 0)));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionUnconstrained) {
  ResolutionSet set;
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(1, 1));
  EXPECT_TRUE(set.ContainsPoint(2000, 2000));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionConstrained) {
  ResolutionSet set = ResolutionSet::FromHeight(10, 100);
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_FALSE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromHeight(0, 100);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_FALSE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromHeight(100, ResolutionSet::kMaxDimension);
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_FALSE(set.ContainsPoint(10, 10));
  EXPECT_FALSE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_TRUE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromWidth(10, 100);
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_FALSE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromWidth(0, 100);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_FALSE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromWidth(100, ResolutionSet::kMaxDimension);
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_FALSE(set.ContainsPoint(10, 10));
  EXPECT_FALSE(set.ContainsPoint(50, 50));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_TRUE(set.ContainsPoint(500, 500));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromAspectRatio(1.0, 2.0);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(10, 20));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_TRUE(set.ContainsPoint(2000, 4000));
  EXPECT_FALSE(set.ContainsPoint(1, 50));
  EXPECT_FALSE(set.ContainsPoint(50, 1));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromAspectRatio(0.0, 2.0);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(10, 20));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_TRUE(set.ContainsPoint(2000, 4000));
  EXPECT_FALSE(set.ContainsPoint(1, 50));
  EXPECT_TRUE(set.ContainsPoint(50, 1));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromAspectRatio(1.0, HUGE_VAL);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(10, 10));
  EXPECT_TRUE(set.ContainsPoint(10, 20));
  EXPECT_TRUE(set.ContainsPoint(100, 100));
  EXPECT_TRUE(set.ContainsPoint(2000, 4000));
  EXPECT_TRUE(set.ContainsPoint(1, 50));
  EXPECT_FALSE(set.ContainsPoint(50, 1));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionTrivialEmptiness) {
  ResolutionSet set = ResolutionSet::FromHeight(100, 10);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_TRUE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromWidth(100, 10);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_TRUE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());

  set = ResolutionSet::FromAspectRatio(100.0, 10.0);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_TRUE(set.IsAspectRatioEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionLineConstraintsEmptiness) {
  ResolutionSet set(1, 1, 1, 1, 1, 1);
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_TRUE(set.ContainsPoint(1, 1));
  EXPECT_FALSE(set.ContainsPoint(1, 0));
  EXPECT_FALSE(set.ContainsPoint(0, 1));

  // Three lines that do not intersect in the same point is empty.
  set = ResolutionSet(1, 1, 1, 1, 0.5, 0.5);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_TRUE(set.IsAspectRatioEmpty());
  EXPECT_FALSE(set.ContainsPoint(0, 0));
  EXPECT_FALSE(set.ContainsPoint(1, 1));
  EXPECT_FALSE(set.ContainsPoint(1, 0));
  EXPECT_FALSE(set.ContainsPoint(0, 1));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionBoxEmptiness) {
  const int kMin = 100;
  const int kMax = 200;
  // Max aspect ratio below box.
  ResolutionSet set(kMin, kMax, kMin, kMax, 0.4, 0.4);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_TRUE(set.IsAspectRatioEmpty());

  // Min aspect ratio above box.
  set = ResolutionSet(kMin, kMax, kMin, kMax, 3.0, HUGE_VAL);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_TRUE(set.IsAspectRatioEmpty());

  // Min aspect ratio crosses box.
  set = ResolutionSet(kMin, kMax, kMin, kMax, 1.0, HUGE_VAL);
  EXPECT_FALSE(set.IsEmpty());

  // Max aspect ratio crosses box.
  set = ResolutionSet(kMin, kMax, kMin, kMax, 0.0, 1.0);
  EXPECT_FALSE(set.IsEmpty());

  // Min and max aspect ratios cross box.
  set = ResolutionSet(kMin, kMax, kMin, kMax, 0.9, 1.1);
  EXPECT_FALSE(set.IsEmpty());

  // Min and max aspect ratios cover box.
  set = ResolutionSet(kMin, kMax, kMin, kMax, 0.2, 100);
  EXPECT_FALSE(set.IsEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionPointIntersection) {
  ResolutionSet set1(1, 2, 1, 2, 0.0, HUGE_VAL);
  ResolutionSet set2 = ResolutionSet::FromExactAspectRatio(0.5);
  auto intersection = set1.Intersection(set2);

  // The intersection should contain only the point (h=2, w=1)
  EXPECT_TRUE(intersection.ContainsPoint(2, 1));

  // It should not contain any point in the vicinity of the included point
  // (integer version).
  EXPECT_FALSE(intersection.ContainsPoint(1, 0));
  EXPECT_FALSE(intersection.ContainsPoint(2, 0));
  EXPECT_FALSE(intersection.ContainsPoint(3, 0));
  EXPECT_FALSE(intersection.ContainsPoint(1, 1));
  EXPECT_FALSE(intersection.ContainsPoint(3, 1));
  EXPECT_FALSE(intersection.ContainsPoint(1, 2));
  EXPECT_FALSE(intersection.ContainsPoint(2, 2));
  EXPECT_FALSE(intersection.ContainsPoint(3, 2));

  // It should not contain any point in the vicinity of the included point
  // (floating-point version).
  EXPECT_FALSE(intersection.ContainsPoint(P(2.0001, 1.0001)));
  EXPECT_FALSE(intersection.ContainsPoint(P(2.0001, 1.0)));
  EXPECT_FALSE(intersection.ContainsPoint(P(2.0001, 0.9999)));
  EXPECT_FALSE(intersection.ContainsPoint(P(2.0, 1.0001)));
  EXPECT_FALSE(intersection.ContainsPoint(P(2.0, 0.9999)));
  EXPECT_FALSE(intersection.ContainsPoint(P(1.9999, 1.0001)));
  EXPECT_FALSE(intersection.ContainsPoint(P(1.9999, 1.0)));
  EXPECT_FALSE(intersection.ContainsPoint(P(1.9999, 0.9999)));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionLineIntersection) {
  ResolutionSet set1(1, 2, 1, 2, 0.0, HUGE_VAL);
  ResolutionSet set2 = ResolutionSet::FromExactAspectRatio(1.0);

  // The intersection should contain (1,1) and (2,2)
  auto intersection = set1.Intersection(set2);
  EXPECT_TRUE(intersection.ContainsPoint(1, 1));
  EXPECT_TRUE(intersection.ContainsPoint(2, 2));

  // It should not contain the other points in the bounding box.
  EXPECT_FALSE(intersection.ContainsPoint(1, 2));
  EXPECT_FALSE(intersection.ContainsPoint(2, 1));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionBoxIntersection) {
  const int kMin1 = 0;
  const int kMax1 = 2;
  ResolutionSet set1(kMin1, kMax1, kMin1, kMax1, 0.0, HUGE_VAL);

  const int kMin2 = 1;
  const int kMax2 = 3;
  ResolutionSet set2(kMin2, kMax2, kMin2, kMax2, 0.0, HUGE_VAL);

  auto intersection = set1.Intersection(set2);
  for (int i = kMin1; i <= kMax2; ++i) {
    for (int j = kMin1; j <= kMax2; ++j) {
      if (i >= kMin2 && j >= kMin2 && i <= kMax1 && j <= kMax1)
        EXPECT_TRUE(intersection.ContainsPoint(i, j));
      else
        EXPECT_FALSE(intersection.ContainsPoint(i, j));
    }
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionPointSetClosestPoint) {
  const int kHeight = 10;
  const int kWidth = 10;
  const double kAspectRatio = 1.0;
  ResolutionSet set(kHeight, kHeight, kWidth, kWidth, kAspectRatio,
                    kAspectRatio);

  for (int height = 0; height < 100; height += 10) {
    for (int width = 0; width < 100; width += 10) {
      EXPECT_EQ(P(kHeight, kWidth), set.ClosestPointTo(P(height, width)));
    }
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionLineSetClosestPoint) {
  {
    const int kHeight = 10;
    auto set = ResolutionSet::FromExactHeight(kHeight);
    for (int height = 0; height < 100; height += 10) {
      for (int width = 0; width < 100; width += 10) {
        EXPECT_EQ(P(kHeight, width), set.ClosestPointTo(P(height, width)));
      }
    }
    const int kWidth = 10;
    set = ResolutionSet::FromExactWidth(kWidth);
    for (int height = 0; height < 100; height += 10) {
      for (int width = 0; width < 100; width += 10) {
        EXPECT_EQ(P(height, kWidth), set.ClosestPointTo(P(height, width)));
      }
    }
  }

  {
    const double kAspectRatios[] = {0.0, 0.1, 0.2, 0.5,
                                    1.0, 2.0, 5.0, HUGE_VAL};
    for (double aspect_ratio : kAspectRatios) {
      auto set = ResolutionSet::FromExactAspectRatio(aspect_ratio);
      for (int height = 0; height < 100; height += 10) {
        for (int width = 0; width < 100; width += 10) {
          Point point(height, width);
          Point expected =
              ProjectionOnSegmentLine(point, P(0, 0), P(1, aspect_ratio));
          Point actual = set.ClosestPointTo(point);
          // This requires higher tolerance than ExpectPointEx due to the larger
          // error of the alternative projection method.
          EXPECT_TRUE(expected.IsApproximatelyEqualTo(actual));
        }
      }
    }
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionGeneralSetClosestPoint) {
  // This set contains the following vertices:
  // (10, 10), (20, 10), (100, 50), (100, 100), (100/1.5, 100), (10, 15)
  ResolutionSet set(10, 100, 10, 100, 0.5, 1.5);

  // Check that vertices are the closest points to themselves.
  auto vertices = set.ComputeVertices();
  for (auto& vertex : vertices)
    EXPECT_EQ(vertex, set.ClosestPointTo(vertex));

  // Point inside the set.
  EXPECT_EQ(P(11, 11), set.ClosestPointTo(P(11, 11)));

  // Close to horizontal segment (10, 10) (20, 10).
  EXPECT_EQ(P(15, 10), set.ClosestPointTo(P(15, 9)));

  // Close to horizontal segment (100, 100) (100/1.5, 100).
  EXPECT_EQ(P(99, 100), set.ClosestPointTo(P(99, 200)));

  // Close to vertical segment (10, 15) (10, 10).
  EXPECT_EQ(P(10, 12.5), set.ClosestPointTo(P(2, 12.5)));

  // Close to vertical segment (100, 50) (100, 100).
  EXPECT_EQ(P(100, 75), set.ClosestPointTo(P(120, 75)));

  // Close to oblique segment (20, 10) (100, 50)
  {
    Point point(70, 15);
    Point expected = ProjectionOnSegmentLine(point, P(20, 10), P(100, 50));
    Point actual = set.ClosestPointTo(point);
    EXPECT_POINT_EQ(expected, actual);
  }

  // Close to oblique segment (100/1.5, 100) (10, 15)
  {
    Point point(12, 70);
    Point expected =
        ProjectionOnSegmentLine(point, P(100 / 1.5, 100), P(10, 15));
    Point actual = set.ClosestPointTo(point);
    EXPECT_POINT_EQ(expected, actual);
  }

  // Points close to vertices.
  EXPECT_EQ(P(10, 10), set.ClosestPointTo(P(9, 9)));
  EXPECT_EQ(P(20, 10), set.ClosestPointTo(P(20, 9)));
  EXPECT_EQ(P(100, 50), set.ClosestPointTo(P(101, 50)));
  EXPECT_EQ(P(100, 100), set.ClosestPointTo(P(101, 101)));
  EXPECT_EQ(P(100 / 1.5, 100), set.ClosestPointTo(P(100 / 1.5, 101)));
  EXPECT_EQ(P(10, 15), set.ClosestPointTo(P(9, 15)));
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionIdealIntersects) {
  ResolutionSet set(100, 1000, 100, 1000, 0.5, 2.0);

  const int kIdealHeight = 500;
  const int kIdealWidth = 1000;
  const double kIdealAspectRatio = 1.5;

  // Ideal height.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(kIdealHeight, kIdealHeight * kDefaultAspectRatio),
                    point);
  }

  // Ideal width.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(kIdealWidth);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(kIdealWidth / kDefaultAspectRatio, kIdealWidth),
                    point);
  }

  // Ideal aspect ratio.
  {
    factory_.Reset();
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_DOUBLE_EQ(kDefaultHeight, point.height());
    EXPECT_DOUBLE_EQ(kDefaultHeight * kIdealAspectRatio, point.width());
  }

  // Ideal height and width.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().width.SetIdeal(kIdealWidth);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(kIdealHeight, kIdealWidth), point);
  }

  // Ideal height and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(kIdealHeight, kIdealHeight * kIdealAspectRatio),
                    point);
  }

  // Ideal width and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(kIdealWidth);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(kIdealWidth / kIdealAspectRatio, kIdealWidth), point);
  }

  // Ideal height, width and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().width.SetIdeal(kIdealWidth);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    // Ideal aspect ratio should be ignored.
    EXPECT_POINT_EQ(Point(kIdealHeight, kIdealWidth), point);
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionIdealOutsideSinglePoint) {
  // This set is a triangle with vertices (100,100), (1000,100) and (1000,1000).
  ResolutionSet set(100, 1000, 100, 1000, 0.0, 1.0);

  const int kIdealHeight = 50;
  const int kIdealWidth = 1100;
  const double kIdealAspectRatio = 0.09;
  const Point kVertex1(100, 100);
  const Point kVertex2(1000, 100);
  const Point kVertex3(1000, 1000);

  // Ideal height.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(kVertex1, point);
  }

  // Ideal width.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(kIdealWidth);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(kVertex3, point);
  }

  // Ideal aspect ratio.
  {
    factory_.Reset();
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(kVertex2, point);
  }

  // Ideal height and width.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().width.SetIdeal(kIdealWidth);
    Point point = SelectClosestPointToIdeal(set);
    Point expected = set.ClosestPointTo(Point(kIdealHeight, kIdealWidth));
    EXPECT_POINT_EQ(expected, point);
  }

  // Ideal height and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    Point expected = set.ClosestPointTo(
        Point(kIdealHeight, kIdealHeight * kIdealAspectRatio));
    EXPECT_POINT_EQ(expected, point);
  }

  // Ideal width and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(kIdealWidth);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    Point expected =
        set.ClosestPointTo(Point(kIdealWidth / kIdealAspectRatio, kIdealWidth));
    EXPECT_POINT_EQ(expected, point);
  }

  // Ideal height, width and aspect-ratio.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    factory_.basic().width.SetIdeal(kIdealWidth);
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    // kIdealAspectRatio is ignored if all three ideals are given.
    Point expected = set.ClosestPointTo(Point(kIdealHeight, kIdealWidth));
    EXPECT_POINT_EQ(expected, point);
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest,
       ResolutionIdealOutsideMultiplePoints) {
  // This set is a triangle with vertices (100,100), (1000,100) and (1000,1000).
  ResolutionSet set(100, 1000, 100, 1000, 0.0, 1.0);

  const int kIdealHeight = 1100;
  const int kIdealWidth = 50;
  const double kIdealAspectRatio = 11.0;
  const Point kVertex1(100, 100);
  const Point kVertex2(1000, 100);
  const Point kVertex3(1000, 1000);

  // Ideal height.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(kIdealHeight);
    Point point = SelectClosestPointToIdeal(set);
    // Parallel to the side between kVertex2 and kVertex3. Point closest to
    // default aspect ratio is kVertex3.
    EXPECT_POINT_EQ(kVertex3, point);
  }

  // Ideal width.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(kIdealWidth);
    Point point = SelectClosestPointToIdeal(set);
    // Parallel to the side between kVertex1 and kVertex2. Point closest to
    // default aspect ratio is kVertex1.
    EXPECT_POINT_EQ(kVertex1, point);
  }

  // Ideal aspect ratio.
  {
    factory_.Reset();
    factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    Point point = SelectClosestPointToIdeal(set);
    // The side between kVertex1 and kVertex3 is closest. The points closest to
    // default dimensions are (kDefaultHeight, kDefaultHeight * AR)
    // and (kDefaultWidth / AR, kDefaultWidth). Since the aspect ratio of the
    // polygon side is less than the default, the algorithm preserves the
    // default width.
    Point expected(kDefaultWidth / kVertex1.AspectRatio(), kDefaultWidth);
    EXPECT_POINT_EQ(expected, point);
    EXPECT_TRUE(set.ContainsPoint(expected));
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest,
       ResolutionUnconstrainedExtremeIdeal) {
  ResolutionSet set;

  // Ideal height.
  {
    factory_.Reset();
    factory_.basic().height.SetIdeal(std::numeric_limits<int32_t>::max());
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(
        Point(ResolutionSet::kMaxDimension, ResolutionSet::kMaxDimension),
        point);
    factory_.basic().height.SetIdeal(0);
    point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(0, 0), point);
  }

  // Ideal width.
  {
    factory_.Reset();
    factory_.basic().width.SetIdeal(std::numeric_limits<int32_t>::max());
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(ResolutionSet::kMaxDimension / kDefaultAspectRatio,
                          ResolutionSet::kMaxDimension),
                    point);
    factory_.basic().width.SetIdeal(0);
    point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(0, 0), point);
  }

  // Ideal Aspect Ratio.
  {
    factory_.Reset();
    factory_.basic().aspect_ratio.SetIdeal(HUGE_VAL);
    Point point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(0, ResolutionSet::kMaxDimension), point);
    factory_.basic().aspect_ratio.SetIdeal(0.0);
    point = SelectClosestPointToIdeal(set);
    EXPECT_POINT_EQ(Point(ResolutionSet::kMaxDimension, 0), point);
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ResolutionVertices) {
  // Empty set.
  {
    ResolutionSet set(1000, 100, 1000, 100, 0.5, 1.5);
    ASSERT_TRUE(set.IsEmpty());
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(0U, vertices.size());
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Three lines that intersect at the same point.
  {
    ResolutionSet set(1, 1, 1, 1, 1, 1);
    EXPECT_FALSE(set.IsEmpty());
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(1U, vertices.size());
    VerticesContain(vertices, Point(1, 1));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // A line segment with the lower-left and upper-right corner of the box.
  {
    ResolutionSet set(0, 100, 0, 100, 1.0, 1.0);
    EXPECT_FALSE(set.IsEmpty());
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(0, 0));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(0, 100, 0, 100, 1.0, HUGE_VAL);
    EXPECT_FALSE(set.IsEmpty());
    vertices = set.ComputeVertices();
    EXPECT_EQ(3U, vertices.size());
    VerticesContain(vertices, Point(0, 0));
    VerticesContain(vertices, Point(100, 100));
    VerticesContain(vertices, Point(0, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(0, 100, 0, 100, 0, 1.0);
    EXPECT_FALSE(set.IsEmpty());
    vertices = set.ComputeVertices();
    EXPECT_EQ(3U, vertices.size());
    VerticesContain(vertices, Point(0, 0));
    VerticesContain(vertices, Point(100, 100));
    VerticesContain(vertices, Point(100, 0));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // A line segment that crosses the bottom and right sides of the box.
  {
    const double kAspectRatio = 50.0 / 75.0;
    ResolutionSet set(50, 100, 50, 100, kAspectRatio, kAspectRatio);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100, 100.0 * kAspectRatio));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 50, 100, kAspectRatio, HUGE_VAL);
    vertices = set.ComputeVertices();
    EXPECT_EQ(5U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100, 100.0 * kAspectRatio));
    VerticesContain(vertices, Point(50, 50));
    VerticesContain(vertices, Point(50, 100));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 50, 100, 0.0, kAspectRatio);
    vertices = set.ComputeVertices();
    EXPECT_EQ(3U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100, 100.0 * kAspectRatio));
    VerticesContain(vertices, Point(100, 50));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // A line segment that crosses the left and top sides of the box.
  {
    const double kAspectRatio = 75.0 / 50.0;
    ResolutionSet set(50, 100, 50, 100, kAspectRatio, kAspectRatio);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 50, 100, kAspectRatio, HUGE_VAL);
    vertices = set.ComputeVertices();
    EXPECT_EQ(3U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    VerticesContain(vertices, Point(50, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 50, 100, 0.0, kAspectRatio);
    vertices = set.ComputeVertices();
    EXPECT_EQ(5U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    VerticesContain(vertices, Point(50, 50));
    VerticesContain(vertices, Point(100, 100));
    VerticesContain(vertices, Point(100, 50));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // An aspect ratio constraint crosses the bottom and top sides of the box.
  {
    const double kAspectRatio = 75.0 / 50.0;
    ResolutionSet set(0, 100, 50, 100, kAspectRatio, kAspectRatio);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(0, 100, 50, 100, kAspectRatio, HUGE_VAL);
    vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    VerticesContain(vertices, Point(0, 50));
    VerticesContain(vertices, Point(0, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(0, 100, 50, 100, 0.0, kAspectRatio);
    vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50 / kAspectRatio, 50));
    VerticesContain(vertices, Point(100 / kAspectRatio, 100));
    VerticesContain(vertices, Point(100, 50));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // An aspect-ratio constraint crosses the left and right sides of the box.
  {
    const double kAspectRatio = 75.0 / 50.0;
    ResolutionSet set(50, 100, 0, 200, kAspectRatio, kAspectRatio);
    auto vertices = set.ComputeVertices();
    // This one fails if floating-point precision is too high.
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100, 100 * kAspectRatio));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 0, 200, kAspectRatio, HUGE_VAL);
    vertices = set.ComputeVertices();
    // This one fails if floating-point precision is too high.
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100, 100 * kAspectRatio));
    VerticesContain(vertices, Point(50, 200));
    VerticesContain(vertices, Point(100, 200));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 0, 200, 0.0, kAspectRatio);
    vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * kAspectRatio));
    VerticesContain(vertices, Point(100, 100 * kAspectRatio));
    VerticesContain(vertices, Point(50, 0));
    VerticesContain(vertices, Point(100, 0));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Aspect-ratio lines touch the corners of the box.
  {
    ResolutionSet set(50, 100, 50, 100, 0.5, 2.0);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50, 50));
    VerticesContain(vertices, Point(100, 50));
    VerticesContain(vertices, Point(50, 100));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Hexagons.
  {
    ResolutionSet set(10, 100, 10, 100, 0.5, 1.5);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(6U, vertices.size());
    VerticesContain(vertices, Point(10, 10));
    VerticesContain(vertices, Point(100, 100));
    VerticesContain(vertices, Point(10, 10 * 1.5));
    VerticesContain(vertices, Point(100 / 1.5, 100));
    VerticesContain(vertices, Point(10 / 0.5, 10));
    VerticesContain(vertices, Point(100, 100 * 0.5));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 50, 100, 50.0 / 75.0, 75.0 / 50.0);
    vertices = set.ComputeVertices();
    EXPECT_EQ(6U, vertices.size());
    VerticesContain(vertices, Point(50, 50));
    VerticesContain(vertices, Point(100, 100));
    VerticesContain(vertices, Point(75, 50));
    VerticesContain(vertices, Point(50, 75));
    VerticesContain(vertices, Point(100, 100.0 * 50.0 / 75.0));
    VerticesContain(vertices, Point(100 * 50.0 / 75.0, 100.0));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Both aspect-ratio constraints cross the left and top sides of the box.
  {
    ResolutionSet set(10, 100, 10, 100, 1.5, 1.7);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(10, 10 * 1.5));
    VerticesContain(vertices, Point(10, 10 * 1.7));
    VerticesContain(vertices, Point(100 / 1.5, 100));
    VerticesContain(vertices, Point(100 / 1.7, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Both aspect-ratio constraints cross the left and right sides of the box.
  {
    ResolutionSet set(10, 100, 10, ResolutionSet::kMaxDimension, 1.5, 1.7);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(10, 10 * 1.5));
    VerticesContain(vertices, Point(10, 10 * 1.7));
    VerticesContain(vertices, Point(100, 100 * 1.5));
    VerticesContain(vertices, Point(100, 100 * 1.7));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Both aspect-ratio constraints cross the bottom and top sides of the box.
  {
    ResolutionSet set(10, 100, 50, 100, 2.0, 4.0);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50 / 2.0, 50));
    VerticesContain(vertices, Point(100 / 2.0, 100));
    VerticesContain(vertices, Point(50 / 4.0, 50));
    VerticesContain(vertices, Point(100 / 4.0, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Both aspect-ratio constraints cross the bottom and right sides of the box.
  {
    ResolutionSet set(10, 100, 50, 100, 0.7, 0.9);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(4U, vertices.size());
    VerticesContain(vertices, Point(50 / 0.7, 50));
    VerticesContain(vertices, Point(50 / 0.9, 50));
    VerticesContain(vertices, Point(100, 100 * 0.7));
    VerticesContain(vertices, Point(100, 100 * 0.9));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Pentagons.
  {
    ResolutionSet set(10, 100, 50, 100, 0.7, 4.0);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(5U, vertices.size());
    VerticesContain(vertices, Point(50 / 0.7, 50));
    VerticesContain(vertices, Point(100, 100 * 0.7));
    VerticesContain(vertices, Point(50 / 4.0, 50));
    VerticesContain(vertices, Point(100 / 4.0, 100));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(50, 100, 10, 100, 0.7, 1.5);
    vertices = set.ComputeVertices();
    EXPECT_EQ(5U, vertices.size());
    VerticesContain(vertices, Point(50, 50 * 0.7));
    VerticesContain(vertices, Point(100, 100 * 0.7));
    VerticesContain(vertices, Point(50, 50 * 1.5));
    VerticesContain(vertices, Point(100 / 1.5, 100));
    VerticesContain(vertices, Point(100, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }

  // Extreme aspect ratios, for completeness.
  {
    ResolutionSet set(0, 100, 0, ResolutionSet::kMaxDimension, 0.0, 0.0);
    auto vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(0, 0));
    VerticesContain(vertices, Point(100, 0));
    EXPECT_TRUE(AreValidVertices(set, vertices));

    set = ResolutionSet(0, ResolutionSet::kMaxDimension, 0, 100, HUGE_VAL,
                        HUGE_VAL);
    vertices = set.ComputeVertices();
    EXPECT_EQ(2U, vertices.size());
    VerticesContain(vertices, Point(0, 0));
    VerticesContain(vertices, Point(0, 100));
    EXPECT_TRUE(AreValidVertices(set, vertices));
  }
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ExactResolution) {
  const int kExactWidth = 640;
  const int kExactHeight = 480;
  ResolutionSet set =
      ResolutionSet::FromExactResolution(kExactWidth, kExactHeight);
  EXPECT_TRUE(set.ContainsPoint(kExactHeight, kExactWidth));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight - 1, kExactWidth - 1));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight - 1, kExactWidth));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight - 1, kExactWidth + 1));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight, kExactWidth - 1));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight, kExactWidth + 1));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight + 1, kExactWidth - 1));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight + 1, kExactWidth));
  EXPECT_FALSE(set.ContainsPoint(kExactHeight + 1, kExactWidth + 1));
  EXPECT_FALSE(set.ContainsPoint(1, 1));
  EXPECT_FALSE(set.ContainsPoint(2000, 2000));
  EXPECT_FALSE(set.IsHeightEmpty());
  EXPECT_FALSE(set.IsWidthEmpty());
  EXPECT_FALSE(set.IsAspectRatioEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, ZeroExactResolution) {
  ResolutionSet set = ResolutionSet::FromExactResolution(0, 0);
  EXPECT_TRUE(set.ContainsPoint(0, 0));
  EXPECT_EQ(set.min_aspect_ratio(), 0.0);
  EXPECT_EQ(set.max_aspect_ratio(), HUGE_VAL);
}

TEST_F(MediaStreamConstraintsUtilSetsTest, NumericRangeSetDouble) {
  using DoubleRangeSet = media_constraints::NumericRangeSet<double>;
  // Open set.
  DoubleRangeSet set;
  EXPECT_FALSE(set.Min().has_value());
  EXPECT_FALSE(set.Max().has_value());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_TRUE(set.Contains(0.0));
  EXPECT_TRUE(set.Contains(1.0));
  EXPECT_TRUE(set.Contains(HUGE_VAL));
  EXPECT_TRUE(set.Contains(-1.0));

  // Constrained set.
  const double kMin = 1.0;
  const double kMax = 10.0;
  set = DoubleRangeSet(kMin, kMax);
  EXPECT_EQ(kMin, *set.Min());
  EXPECT_EQ(kMax, *set.Max());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_FALSE(set.Contains(0.0));
  EXPECT_TRUE(set.Contains(1.0));
  EXPECT_TRUE(set.Contains(10.0));
  EXPECT_FALSE(set.Contains(HUGE_VAL));
  EXPECT_FALSE(set.Contains(-1.0));

  // If the lower bound is greater than the upper bound, the set is empty.
  set = DoubleRangeSet(kMax, kMin);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.Contains(0.0));
  EXPECT_FALSE(set.Contains(1.0));
  EXPECT_FALSE(set.Contains(HUGE_VAL));
  EXPECT_FALSE(set.Contains(-1.0));

  // An explicit empty set is empty.
  set = DoubleRangeSet::EmptySet();
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.Contains(0.0));
  EXPECT_FALSE(set.Contains(1.0));
  EXPECT_FALSE(set.Contains(HUGE_VAL));
  EXPECT_FALSE(set.Contains(-1.0));

  // Intersection.
  set = DoubleRangeSet(kMin, kMax);
  const double kMin2 = 5.0;
  const double kMax2 = 20.0;
  auto intersection = set.Intersection(DoubleRangeSet(kMin2, kMax2));
  EXPECT_EQ(kMin2, intersection.Min());
  EXPECT_EQ(kMax, intersection.Max());
  EXPECT_FALSE(intersection.IsEmpty());

  // Intersection with partially open sets.
  set = DoubleRangeSet(std::nullopt, kMax);
  intersection = set.Intersection(DoubleRangeSet(kMin2, std::nullopt));
  EXPECT_EQ(kMin2, *intersection.Min());
  EXPECT_EQ(kMax, *intersection.Max());
  EXPECT_FALSE(intersection.IsEmpty());

  // Empty intersection.
  intersection = set.Intersection(DoubleRangeSet(kMax + 1, HUGE_VAL));
  EXPECT_TRUE(intersection.IsEmpty());

  // Intersection with empty set.
  intersection = set.Intersection(DoubleRangeSet::EmptySet());
  EXPECT_TRUE(intersection.IsEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, NumericRangeSetFromConstraint) {
  // Exact value translates in a range with a single value.
  LongConstraint constraint = LongConstraint("aConstraint");
  constraint.SetExact(10);
  media_constraints::NumericRangeSet<int> range =
      media_constraints::NumericRangeSet<int>::FromConstraint(constraint);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 10);
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 10);

  // A constraint with min and max translates to range with same min and same
  // max.
  constraint = LongConstraint("aConstraint");
  constraint.SetMin(0);
  constraint.SetMax(100);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(constraint);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 0);
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 100);

  // A constraint with only a min or a max value translates to a half-bounded
  // set in both cases.
  constraint = LongConstraint("aConstraint");
  constraint.SetMin(0);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(constraint);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 0);
  EXPECT_FALSE(range.Max());

  constraint = LongConstraint("aConstraint");
  constraint.SetMax(100);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(constraint);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 100);
  EXPECT_FALSE(range.Min());

  // A constraint with no values specified maps to an unbounded range.
  constraint = LongConstraint("aConstraint");
  range = media_constraints::NumericRangeSet<int>::FromConstraint(constraint);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_FALSE(range.Min());
  EXPECT_FALSE(range.Max());
}

TEST_F(MediaStreamConstraintsUtilSetsTest,
       NumericRangeSetFromConstraintWithBounds) {
  int upper_bound = 25;
  int lower_bound = 5;
  // Exact value translates in a range with a single value.
  LongConstraint constraint = LongConstraint("aConstraint");
  constraint.SetExact(10);
  media_constraints::NumericRangeSet<int> range =
      media_constraints::NumericRangeSet<int>::FromConstraint(
          constraint, lower_bound, upper_bound);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 10);
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 10);

  // A constraint with min and max translates to range with same min and same
  // max. If lower and upper bound do not permit that, will have unspecified
  // min and max respectively.
  constraint = LongConstraint("aConstraint");
  constraint.SetMin(0);
  constraint.SetMax(100);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(constraint, 0,
                                                                  100);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 0);
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 100);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_FALSE(range.Min());
  EXPECT_FALSE(range.Max());

  // A constraint with only a min or a max value translates to a half-bounded
  // or unbounded range depending on the whether the lower and the upper bounds
  // allow for it.
  constraint = LongConstraint("aConstraint");
  constraint.SetMin(0);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_FALSE(range.Min());
  EXPECT_FALSE(range.Max());

  constraint = LongConstraint("aConstraint");
  constraint.SetMax(100);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_FALSE(range.Min());
  EXPECT_FALSE(range.Max());

  // A constraint with no values specified maps to an unbounded range
  // independently of upper and lower bounds.
  constraint = LongConstraint("aConstraint");
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_FALSE(range.Min());
  EXPECT_FALSE(range.Max());

  // If the constraint specifies a range that does not overlap with lower and
  // upper bounds, the resulting range will be empty.
  constraint = LongConstraint("aConstraint");
  constraint.SetMin(-5);
  constraint.SetMax(0);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_TRUE(range.IsEmpty());

  constraint = LongConstraint("aConstraint");
  constraint.SetMin(105);
  constraint.SetMax(110);
  range = media_constraints::NumericRangeSet<int>::FromConstraint(
      constraint, lower_bound, upper_bound);
  EXPECT_TRUE(range.IsEmpty());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, NumericRangeSetFromValue) {
  // Getting a range from a single value, will return a range with a single
  // value set as both max and min.
  auto range = media_constraints::NumericRangeSet<int>::FromValue(0);
  EXPECT_FALSE(range.IsEmpty());
  EXPECT_TRUE(range.Min());
  EXPECT_EQ(*range.Min(), 0);
  EXPECT_TRUE(range.Max());
  EXPECT_EQ(*range.Max(), 0);
}

TEST_F(MediaStreamConstraintsUtilSetsTest, DiscreteSetString) {
  // Universal set.
  using StringSet = media_constraints::DiscreteSet<String>;
  StringSet set = StringSet::UniversalSet();
  EXPECT_TRUE(set.Contains("arbitrary"));
  EXPECT_TRUE(set.Contains("strings"));
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_TRUE(set.is_universal());
  EXPECT_FALSE(set.HasExplicitElements());

  // Constrained set.
  set = StringSet(Vector<String>({"a", "b", "c"}));
  EXPECT_TRUE(set.Contains("a"));
  EXPECT_TRUE(set.Contains("b"));
  EXPECT_TRUE(set.Contains("c"));
  EXPECT_FALSE(set.Contains("d"));
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_FALSE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_EQ(String("a"), set.FirstElement());

  // Empty set.
  set = StringSet::EmptySet();
  EXPECT_FALSE(set.Contains("a"));
  EXPECT_FALSE(set.Contains("b"));
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.is_universal());
  EXPECT_FALSE(set.HasExplicitElements());

  // Intersection.
  set = StringSet(Vector<String>({"a", "b", "c"}));
  StringSet set2 = StringSet(Vector<String>({"b", "c", "d"}));
  auto intersection = set.Intersection(set2);
  EXPECT_FALSE(intersection.Contains("a"));
  EXPECT_TRUE(intersection.Contains("b"));
  EXPECT_TRUE(intersection.Contains("c"));
  EXPECT_FALSE(intersection.Contains("d"));
  EXPECT_FALSE(intersection.IsEmpty());
  EXPECT_FALSE(intersection.is_universal());
  EXPECT_TRUE(intersection.HasExplicitElements());
  EXPECT_EQ(String("b"), intersection.FirstElement());

  // Empty intersection.
  set2 = StringSet(Vector<String>({"d", "e", "f"}));
  intersection = set.Intersection(set2);
  EXPECT_FALSE(intersection.Contains("a"));
  EXPECT_FALSE(intersection.Contains("b"));
  EXPECT_FALSE(intersection.Contains("c"));
  EXPECT_FALSE(intersection.Contains("d"));
  EXPECT_TRUE(intersection.IsEmpty());
  EXPECT_FALSE(intersection.is_universal());
  EXPECT_FALSE(intersection.HasExplicitElements());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, DiscreteSetBool) {
  // Universal set.
  BoolSet set = BoolSet::UniversalSet();
  EXPECT_TRUE(set.Contains(true));
  EXPECT_TRUE(set.Contains(false));
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_TRUE(set.is_universal());
  EXPECT_FALSE(set.HasExplicitElements());

  // Constrained set.
  set = BoolSet({true});
  EXPECT_TRUE(set.Contains(true));
  EXPECT_FALSE(set.Contains(false));
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_FALSE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_TRUE(set.FirstElement());

  set = BoolSet({false});
  EXPECT_FALSE(set.Contains(true));
  EXPECT_TRUE(set.Contains(false));
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_FALSE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_FALSE(set.FirstElement());

  // Empty set.
  set = BoolSet::EmptySet();
  EXPECT_FALSE(set.Contains(true));
  EXPECT_FALSE(set.Contains(false));
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_FALSE(set.is_universal());
  EXPECT_FALSE(set.HasExplicitElements());

  // Intersection.
  set = BoolSet::UniversalSet();
  auto intersection = set.Intersection(BoolSet({true}));
  EXPECT_TRUE(intersection.Contains(true));
  EXPECT_FALSE(intersection.Contains(false));
  intersection = set.Intersection(set);
  EXPECT_TRUE(intersection.Contains(true));
  EXPECT_TRUE(intersection.Contains(true));

  // Empty intersection.
  set = BoolSet({true});
  intersection = set.Intersection(BoolSet({false}));
  EXPECT_TRUE(intersection.IsEmpty());

  // Explicit universal set with true as the first element.
  // This cannot result from a boolean constraint because they can only specify
  // one exact value.
  set = BoolSet({true, false});
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_TRUE(set.FirstElement());
  intersection = set.Intersection(BoolSet());
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_TRUE(set.FirstElement());
  intersection = BoolSet().Intersection(set);
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_TRUE(set.FirstElement());

  // Explicit universal set with false as the first element.
  // This cannot result from a boolean constraint because they can only specify
  // one exact value.
  set = BoolSet({false, true});
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_FALSE(set.FirstElement());
  intersection = set.Intersection(BoolSet());
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_FALSE(set.FirstElement());
  intersection = BoolSet().Intersection(set);
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_FALSE(set.FirstElement());

  // Intersection of explicit universal sets with different first elements.
  // This cannot result from boolean constraints because they can only specify
  // one exact value. The first element of the left-hand side is selected as the
  // first element of the intersection.
  set = BoolSet({true, false}).Intersection(BoolSet({false, true}));
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_TRUE(set.FirstElement());
  set = BoolSet({false, true}).Intersection(BoolSet({true, false}));
  EXPECT_TRUE(set.is_universal());
  EXPECT_TRUE(set.HasExplicitElements());
  EXPECT_FALSE(set.FirstElement());
}

TEST_F(MediaStreamConstraintsUtilSetsTest, RescaleSetFromConstraints) {
  factory_.Reset();
  factory_.CreateMediaConstraints();
  BoolSet set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_TRUE(set.is_universal());
  EXPECT_FALSE(set.HasExplicitElements());

  // Invalid exact value.
  factory_.basic().resize_mode.SetExact("invalid");
  set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_TRUE(set.IsEmpty());

  // No rescaling
  factory_.basic().resize_mode.SetExact(WebMediaStreamTrack::kResizeModeNone);
  set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_TRUE(set.Contains(false));
  EXPECT_FALSE(set.Contains(true));

  // Rescaling
  factory_.basic().resize_mode.SetExact(
      WebMediaStreamTrack::kResizeModeRescale);
  set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_TRUE(set.Contains(true));
  EXPECT_FALSE(set.Contains(false));

  // Both explicit
  Vector<String> rescale_modes = {WebMediaStreamTrack::kResizeModeRescale,
                                  WebMediaStreamTrack::kResizeModeNone};
  factory_.basic().resize_mode.SetExact(rescale_modes);
  set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_TRUE(set.Contains(true));
  EXPECT_TRUE(set.Contains(false));

  // Invalid and no rescaling.
  rescale_modes[0] = "invalid";
  factory_.basic().resize_mode.SetExact(Vector<String>(rescale_modes));
  set =
      media_constraints::RescaleSetFromConstraint(factory_.basic().resize_mode);
  EXPECT_FALSE(set.Contains(true));
  EXPECT_TRUE(set.Contains(false));
}

}  // namespace media_constraints
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"

#include <cmath>

#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"

namespace blink {
namespace media_constraints {

using Point = ResolutionSet::Point;

namespace {

constexpr double kTolerance = 1e-5;

// Not perfect, but good enough for this application.
bool AreApproximatelyEqual(double d1, double d2) {
  if (std::fabs((d1 - d2)) <= kTolerance)
    return true;

  return d1 == d2 || (std::fabs((d1 - d2) / d1) <= kTolerance &&
                      std::fabs((d1 - d2) / d2) <= kTolerance);
}

bool IsLess(double d1, double d2) {
  return d1 < d2 && !AreApproximatelyEqual(d1, d2);
}

bool IsLessOrEqual(double d1, double d2) {
  return d1 < d2 || AreApproximatelyEqual(d1, d2);
}

bool IsGreater(double d1, double d2) {
  return d1 > d2 && !AreApproximatelyEqual(d1, d2);
}

bool IsGreaterOrEqual(double d1, double d2) {
  return d1 > d2 || AreApproximatelyEqual(d1, d2);
}

int ToValidDimension(int dimension) {
  if (dimension > ResolutionSet::kMaxDimension)
    return ResolutionSet::kMaxDimension;
  if (dimension < 0)
    return 0;

  return static_cast<int>(dimension);
}

int MinDimensionFromConstraint(const LongConstraint& constraint) {
  if (!ConstraintHasMin(constraint))
    return 0;

  return ToValidDimension(ConstraintMin(constraint));
}

int MaxDimensionFromConstraint(const LongConstraint& constraint) {
  if (!ConstraintHasMax(constraint))
    return ResolutionSet::kMaxDimension;

  return ToValidDimension(ConstraintMax(constraint));
}

double ToValidAspectRatio(double aspect_ratio) {
  return aspect_ratio < 0.0 ? 0.0 : aspect_ratio;
}

double MinAspectRatioFromConstraint(const DoubleConstraint& constraint) {
  if (!ConstraintHasMin(constraint))
    return 0.0;

  return ToValidAspectRatio(ConstraintMin(constraint));
}

double MaxAspectRatioFromConstraint(const DoubleConstraint& constraint) {
  if (!ConstraintHasMax(constraint))
    return HUGE_VAL;

  return ToValidAspectRatio(ConstraintMax(constraint));
}

bool IsPositiveFiniteAspectRatio(double aspect_ratio) {
  return std::isfinite(aspect_ratio) && aspect_ratio > 0.0;
}

// If |vertices| has a single element, return |vertices[0]|.
// If |vertices| has two elements, returns the point in the segment defined by
// |vertices| that is closest to |point|.
// |vertices| must have 1 or 2 elements. Otherwise, behavior is undefined.
// This function is called when |point| has already been determined to be
// outside a polygon and |vertices| is the vertex or side closest to |point|.
Point GetClosestPointToVertexOrSide(const std::vector<Point> vertices,
                                    const Point& point) {
  DCHECK(!vertices.empty());
  // If only a single vertex closest to |point|, return that vertex.
  if (vertices.size() == 1U)
    return vertices[0];

  DCHECK_EQ(vertices.size(), 2U);
  // If a polygon side is closest to the ideal height, return the
  // point with aspect ratio closest to the default.
  return Point::ClosestPointInSegment(point, vertices[0], vertices[1]);
}

Point SelectPointWithLargestArea(const Point& p1, const Point& p2) {
  return p1.width() * p1.height() > p2.width() * p2.height() ? p1 : p2;
}

}  // namespace

Point::Point(double height, double width) : height_(height), width_(width) {
  DCHECK(!std::isnan(height_));
  DCHECK(!std::isnan(width_));
}
Point::Point(const Point& other) = default;
Point& Point::operator=(const Point& other) = default;
Point::~Point() = default;

bool Point::operator==(const Point& other) const {
  return height_ == other.height_ && width_ == other.width_;
}

bool Point::operator!=(const Point& other) const {
  return !(*this == other);
}

bool Point::IsApproximatelyEqualTo(const Point& other) const {
  return AreApproximatelyEqual(height_, other.height_) &&
         AreApproximatelyEqual(width_, other.width_);
}

Point Point::operator+(const Point& other) const {
  return Point(height_ + other.height_, width_ + other.width_);
}

Point Point::operator-(const Point& other) const {
  return Point(height_ - other.height_, width_ - other.width_);
}

Point operator*(double d, const Point& p) {
  return Point(d * p.height(), d * p.width());
}

// Returns the dot product between |p1| and |p2|.
// static
double Point::Dot(const Point& p1, const Point& p2) {
  return p1.height_ * p2.height_ + p1.width_ * p2.width_;
}

// static
double Point::SquareEuclideanDistance(const Point& p1, const Point& p2) {
  Point diff = p1 - p2;
  return Dot(diff, diff);
}

// static
Point Point::ClosestPointInSegment(const Point& p,
                                   const Point& s1,
                                   const Point& s2) {
  // If |s1| and |s2| are the same, it is not really a segment. The closest
  // point to |p| is |s1|=|s2|.
  if (s1 == s2)
    return s1;

  // Translate coordinates to a system where the origin is |s1|.
  Point p_trans = p - s1;
  Point s2_trans = s2 - s1;

  // On this system, we are interested in the projection of |p_trans| on
  // |s2_trans|. The projection is m * |s2_trans|, where
  //       m = Dot(|s2_trans|, |p_trans|) / Dot(|s2_trans|, |s2_trans|).
  // If 0 <= m <= 1, the projection falls within the segment, and the closest
  // point is the projection itself.
  // If m < 0, the closest point is S1.
  // If m > 1, the closest point is S2.
  double m = Dot(s2_trans, p_trans) / Dot(s2_trans, s2_trans);
  if (m < 0)
    return s1;
  if (m > 1)
    return s2;

  // Return the projection in the original coordinate system.
  return s1 + m * s2_trans;
}

ResolutionSet::ResolutionSet(int min_height,
                             int max_height,
                             int min_width,
                             int max_width,
                             double min_aspect_ratio,
                             double max_aspect_ratio)
    : min_height_(min_height),
      max_height_(max_height),
      min_width_(min_width),
      max_width_(max_width),
      min_aspect_ratio_(min_aspect_ratio),
      max_aspect_ratio_(max_aspect_ratio) {
  DCHECK_GE(min_height_, 0);
  DCHECK_GE(max_height_, 0);
  DCHECK_LE(max_height_, kMaxDimension);
  DCHECK_GE(min_width_, 0);
  DCHECK_GE(max_width_, 0);
  DCHECK_LE(max_width_, kMaxDimension);
  DCHECK_GE(min_aspect_ratio_, 0.0);
  DCHECK_GE(max_aspect_ratio_, 0.0);
  DCHECK(!std::isnan(min_aspect_ratio_));
  DCHECK(!std::isnan(max_aspect_ratio_));
}

ResolutionSet::ResolutionSet()
    : ResolutionSet(0, kMaxDimension, 0, kMaxDimension, 0.0, HUGE_VAL) {}

ResolutionSet::ResolutionSet(const ResolutionSet& other) = default;
ResolutionSet::~ResolutionSet() = default;
ResolutionSet& ResolutionSet::operator=(const ResolutionSet& other) = default;

bool ResolutionSet::IsHeightEmpty() const {
  return min_height_ > max_height_ || min_height_ >= kMaxDimension ||
         max_height_ <= 0;
}

bool ResolutionSet::IsWidthEmpty() const {
  return min_width_ > max_width_ || min_width_ >= kMaxDimension ||
         max_width_ <= 0;
}

bool ResolutionSet::IsAspectRatioEmpty() const {
  double max_resolution_aspect_ratio =
      static_cast<double>(max_width_) / static_cast<double>(min_height_);
  double min_resolution_aspect_ratio =
      static_cast<double>(min_width_) / static_cast<double>(max_height_);

  return IsGreater(min_aspect_ratio_, max_aspect_ratio_) ||
         IsLess(max_resolution_aspect_ratio, min_aspect_ratio_) ||
         IsGreater(min_resolution_aspect_ratio, max_aspect_ratio_) ||
         !std::isfinite(min_aspect_ratio_) || max_aspect_ratio_ <= 0.0;
}

bool ResolutionSet::IsEmpty() const {
  return IsHeightEmpty() || IsWidthEmpty() || IsAspectRatioEmpty();
}

bool ResolutionSet::ContainsPoint(const Point& point) const {
  double ratio = point.AspectRatio();
  return point.height() >= min_height_ && point.height() <= max_height_ &&
         point.width() >= min_width_ && point.width() <= max_width_ &&
         ((IsGreaterOrEqual(ratio, min_aspect_ratio_) &&
           IsLessOrEqual(ratio, max_aspect_ratio_)) ||
          // (0.0, 0.0) is always included in the aspect-ratio range.
          (point.width() == 0.0 && point.height() == 0.0));
}

bool ResolutionSet::ContainsPoint(int height, int width) const {
  return ContainsPoint(Point(height, width));
}

ResolutionSet ResolutionSet::Intersection(const ResolutionSet& other) const {
  return ResolutionSet(std::max(min_height_, other.min_height_),
                       std::min(max_height_, other.max_height_),
                       std::max(min_width_, other.min_width_),
                       std::min(max_width_, other.max_width_),
                       std::max(min_aspect_ratio_, other.min_aspect_ratio_),
                       std::min(max_aspect_ratio_, other.max_aspect_ratio_));
}

Point ResolutionSet::SelectClosestPointToIdeal(
    const WebMediaTrackConstraintSet& constraint_set,
    int default_height,
    int default_width) const {
  DCHECK_GE(default_height, 1);
  DCHECK_GE(default_width, 1);
  double default_aspect_ratio =
      static_cast<double>(default_width) / static_cast<double>(default_height);

  DCHECK(!IsEmpty());
  int num_ideals = 0;
  if (constraint_set.height.HasIdeal())
    ++num_ideals;
  if (constraint_set.width.HasIdeal())
    ++num_ideals;
  if (constraint_set.aspect_ratio.HasIdeal())
    ++num_ideals;

  switch (num_ideals) {
    case 0:
      return SelectClosestPointToIdealAspectRatio(
          default_aspect_ratio, default_height, default_width);

    case 1:
      // This case requires a point closest to a line.
      // In all variants, if the ideal line intersects the polygon, select the
      // point in the intersection that is closest to preserving the default
      // aspect ratio or a default dimension.
      // If the ideal line is outside the polygon, there is either a single
      // vertex or a polygon side closest to the ideal line. If a single vertex,
      // select that vertex. If a polygon side, select the point on that side
      // that is closest to preserving the default aspect ratio or a default
      // dimension.
      if (constraint_set.height.HasIdeal()) {
        int ideal_height = ToValidDimension(constraint_set.height.Ideal());
        ResolutionSet ideal_line = ResolutionSet::FromExactHeight(ideal_height);
        ResolutionSet intersection = Intersection(ideal_line);
        if (!intersection.IsEmpty()) {
          return intersection.ClosestPointTo(
              Point(ideal_height, ideal_height * default_aspect_ratio));
        }
        std::vector<Point> closest_vertices =
            GetClosestVertices(&Point::height, ideal_height);
        Point ideal_point(closest_vertices[0].height(),
                          closest_vertices[0].height() * default_aspect_ratio);
        return GetClosestPointToVertexOrSide(closest_vertices, ideal_point);
      }
      if (constraint_set.width.HasIdeal()) {
        int ideal_width = ToValidDimension(constraint_set.width.Ideal());
        ResolutionSet ideal_line = ResolutionSet::FromExactWidth(ideal_width);
        ResolutionSet intersection = Intersection(ideal_line);
        if (!intersection.IsEmpty()) {
          return intersection.ClosestPointTo(
              Point(ideal_width / default_aspect_ratio, ideal_width));
        }
        std::vector<Point> closest_vertices =
            GetClosestVertices(&Point::width, ideal_width);
        Point ideal_point(closest_vertices[0].width() / default_aspect_ratio,
                          closest_vertices[0].width());
        return GetClosestPointToVertexOrSide(closest_vertices, ideal_point);
      }
      {
        DCHECK(constraint_set.aspect_ratio.HasIdeal());
        double ideal_aspect_ratio =
            ToValidAspectRatio(constraint_set.aspect_ratio.Ideal());
        return SelectClosestPointToIdealAspectRatio(
            ideal_aspect_ratio, default_height, default_width);
      }

    case 2:
    case 3:
      double ideal_height;
      double ideal_width;
      if (constraint_set.height.HasIdeal()) {
        ideal_height = ToValidDimension(constraint_set.height.Ideal());
        ideal_width =
            constraint_set.width.HasIdeal()
                ? ToValidDimension(constraint_set.width.Ideal())
                : ideal_height *
                      ToValidAspectRatio(constraint_set.aspect_ratio.Ideal());
      } else {
        DCHECK(constraint_set.width.HasIdeal());
        DCHECK(constraint_set.aspect_ratio.HasIdeal());
        ideal_width = ToValidDimension(constraint_set.width.Ideal());
        ideal_height = ideal_width /
                       ToValidAspectRatio(constraint_set.aspect_ratio.Ideal());
      }
      return ClosestPointTo(Point(ideal_height, ideal_width));

    default:
      NOTREACHED();
  }
  NOTREACHED();
  return Point(-1, -1);
}

Point ResolutionSet::SelectClosestPointToIdealAspectRatio(
    double ideal_aspect_ratio,
    int default_height,
    int default_width) const {
  ResolutionSet intersection =
      Intersection(ResolutionSet::FromExactAspectRatio(ideal_aspect_ratio));
  if (!intersection.IsEmpty()) {
    Point default_height_point(default_height,
                               default_height * ideal_aspect_ratio);
    Point default_width_point(default_width / ideal_aspect_ratio,
                              default_width);
    return SelectPointWithLargestArea(
        intersection.ClosestPointTo(default_height_point),
        intersection.ClosestPointTo(default_width_point));
  }
  std::vector<Point> closest_vertices =
      GetClosestVertices(&Point::AspectRatio, ideal_aspect_ratio);
  double actual_aspect_ratio = closest_vertices[0].AspectRatio();
  Point default_height_point(default_height,
                             default_height * actual_aspect_ratio);
  Point default_width_point(default_width / actual_aspect_ratio, default_width);
  return SelectPointWithLargestArea(
      GetClosestPointToVertexOrSide(closest_vertices, default_height_point),
      GetClosestPointToVertexOrSide(closest_vertices, default_width_point));
}

Point ResolutionSet::ClosestPointTo(const Point& point) const {
  DCHECK(std::numeric_limits<double>::has_infinity);
  DCHECK(std::isfinite(point.height()));
  DCHECK(std::isfinite(point.width()));

  if (ContainsPoint(point))
    return point;

  auto vertices = ComputeVertices();
  DCHECK_GE(vertices.size(), 1U);
  Point best_candidate(0, 0);
  double best_distance = HUGE_VAL;
  for (size_t i = 0; i < vertices.size(); ++i) {
    Point candidate = Point::ClosestPointInSegment(
        point, vertices[i], vertices[(i + 1) % vertices.size()]);
    double distance = Point::SquareEuclideanDistance(point, candidate);
    if (distance < best_distance) {
      best_candidate = candidate;
      best_distance = distance;
    }
  }

  DCHECK(std::isfinite(best_distance));
  return best_candidate;
}

std::vector<Point> ResolutionSet::GetClosestVertices(double (Point::*accessor)()
                                                         const,
                                                     double value) const {
  DCHECK(!IsEmpty());
  std::vector<Point> vertices = ComputeVertices();
  std::vector<Point> closest_vertices;
  double best_diff = HUGE_VAL;
  for (const auto& vertex : vertices) {
    double diff;
    if (std::isfinite(value))
      diff = std::fabs((vertex.*accessor)() - value);
    else
      diff = (vertex.*accessor)() == value ? 0.0 : HUGE_VAL;
    if (diff <= best_diff) {
      if (diff < best_diff) {
        best_diff = diff;
        closest_vertices.clear();
      }
      closest_vertices.push_back(vertex);
    }
  }
  DCHECK(!closest_vertices.empty());
  DCHECK_LE(closest_vertices.size(), 2U);
  return closest_vertices;
}

// static
ResolutionSet ResolutionSet::FromHeight(int min, int max) {
  return ResolutionSet(min, max, 0, kMaxDimension, 0.0, HUGE_VAL);
}

// static
ResolutionSet ResolutionSet::FromExactHeight(int value) {
  return ResolutionSet(value, value, 0, kMaxDimension, 0.0, HUGE_VAL);
}

// static
ResolutionSet ResolutionSet::FromWidth(int min, int max) {
  return ResolutionSet(0, kMaxDimension, min, max, 0.0, HUGE_VAL);
}

// static
ResolutionSet ResolutionSet::FromExactWidth(int value) {
  return ResolutionSet(0, kMaxDimension, value, value, 0.0, HUGE_VAL);
}

// static
ResolutionSet ResolutionSet::FromAspectRatio(double min, double max) {
  return ResolutionSet(0, kMaxDimension, 0, kMaxDimension, min, max);
}

// static
ResolutionSet ResolutionSet::FromExactAspectRatio(double value) {
  return ResolutionSet(0, kMaxDimension, 0, kMaxDimension, value, value);
}

// static
ResolutionSet ResolutionSet::FromExactResolution(int width, int height) {
  double aspect_ratio = ToValidAspectRatio(static_cast<double>(width) / height);
  return ResolutionSet(ToValidDimension(height), ToValidDimension(height),
                       ToValidDimension(width), ToValidDimension(width),
                       std::isnan(aspect_ratio) ? 0.0 : aspect_ratio,
                       std::isnan(aspect_ratio) ? HUGE_VAL : aspect_ratio);
}

std::vector<Point> ResolutionSet::ComputeVertices() const {
  std::vector<Point> vertices;
  // Add vertices in counterclockwise order
  // Start with (min_height, min_width) and continue along min_width.
  TryAddVertex(&vertices, Point(min_height_, min_width_));
  if (IsPositiveFiniteAspectRatio(max_aspect_ratio_))
    TryAddVertex(&vertices, Point(min_width_ / max_aspect_ratio_, min_width_));
  if (IsPositiveFiniteAspectRatio(min_aspect_ratio_))
    TryAddVertex(&vertices, Point(min_width_ / min_aspect_ratio_, min_width_));
  TryAddVertex(&vertices, Point(max_height_, min_width_));
  // Continue along max_height.
  if (IsPositiveFiniteAspectRatio(min_aspect_ratio_)) {
    TryAddVertex(&vertices,
                 Point(max_height_, max_height_ * min_aspect_ratio_));
  }
  if (IsPositiveFiniteAspectRatio(max_aspect_ratio_)) {
    TryAddVertex(&vertices,
                 Point(max_height_, max_height_ * max_aspect_ratio_));
  }
  TryAddVertex(&vertices, Point(max_height_, max_width_));
  // Continue along max_width.
  if (IsPositiveFiniteAspectRatio(min_aspect_ratio_))
    TryAddVertex(&vertices, Point(max_width_ / min_aspect_ratio_, max_width_));
  if (IsPositiveFiniteAspectRatio(max_aspect_ratio_))
    TryAddVertex(&vertices, Point(max_width_ / max_aspect_ratio_, max_width_));
  TryAddVertex(&vertices, Point(min_height_, max_width_));
  // Finish along min_height.
  if (IsPositiveFiniteAspectRatio(max_aspect_ratio_)) {
    TryAddVertex(&vertices,
                 Point(min_height_, min_height_ * max_aspect_ratio_));
  }
  if (IsPositiveFiniteAspectRatio(min_aspect_ratio_)) {
    TryAddVertex(&vertices,
                 Point(min_height_, min_height_ * min_aspect_ratio_));
  }

  DCHECK_LE(vertices.size(), 6U);
  return vertices;
}

void ResolutionSet::TryAddVertex(std::vector<Point>* vertices,
                                 const Point& point) const {
  if (!ContainsPoint(point))
    return;

  // Add the point to the |vertices| if not already added.
  // This is to prevent duplicates in case an aspect ratio intersects a width
  // or height right on a vertex.
  if (vertices->empty() ||
      (*(vertices->end() - 1) != point && *vertices->begin() != point)) {
    vertices->push_back(point);
  }
}

ResolutionSet ResolutionSet::FromConstraintSet(
    const WebMediaTrackConstraintSet& constraint_set) {
  return ResolutionSet(
      MinDimensionFromConstraint(constraint_set.height),
      MaxDimensionFromConstraint(constraint_set.height),
      MinDimensionFromConstraint(constraint_set.width),
      MaxDimensionFromConstraint(constraint_set.width),
      MinAspectRatioFromConstraint(constraint_set.aspect_ratio),
      MaxAspectRatioFromConstraint(constraint_set.aspect_ratio));
}

DiscreteSet<std::string> StringSetFromConstraint(
    const StringConstraint& constraint) {
  if (!constraint.HasExact())
    return DiscreteSet<std::string>::UniversalSet();

  std::vector<std::string> elements;
  for (const auto& entry : constraint.Exact())
    elements.push_back(entry.Ascii());

  return DiscreteSet<std::string>(std::move(elements));
}

DiscreteSet<bool> BoolSetFromConstraint(const BooleanConstraint& constraint) {
  if (!constraint.HasExact())
    return DiscreteSet<bool>::UniversalSet();

  return DiscreteSet<bool>({constraint.Exact()});
}

DiscreteSet<bool> RescaleSetFromConstraint(
    const StringConstraint& resize_mode_constraint) {
  DCHECK_EQ(resize_mode_constraint.GetName(),
            WebMediaTrackConstraintSet().resize_mode.GetName());
  bool contains_none = resize_mode_constraint.Matches(
      WebString::FromASCII(WebMediaStreamTrack::kResizeModeNone));
  bool contains_rescale = resize_mode_constraint.Matches(
      WebString::FromASCII(WebMediaStreamTrack::kResizeModeRescale));
  if (resize_mode_constraint.Exact().empty() ||
      (contains_none && contains_rescale)) {
    return DiscreteSet<bool>::UniversalSet();
  }

  if (contains_none)
    return DiscreteSet<bool>({false});

  if (contains_rescale)
    return DiscreteSet<bool>({true});

  return DiscreteSet<bool>::EmptySet();
}

}  // namespace media_constraints
}  // namespace blink

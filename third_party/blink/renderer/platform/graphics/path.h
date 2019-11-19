/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 *               2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PATH_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathMeasure.h"

namespace blink {

class AffineTransform;
class FloatPoint;
class FloatRect;
class FloatSize;
class StrokeData;

enum PathElementType {
  kPathElementMoveToPoint,          // The points member will contain 1 value.
  kPathElementAddLineToPoint,       // The points member will contain 1 value.
  kPathElementAddQuadCurveToPoint,  // The points member will contain 2 values.
  kPathElementAddCurveToPoint,      // The points member will contain 3 values.
  kPathElementCloseSubpath          // The points member will contain no values.
};

// The points in the structure are the same as those that would be used with the
// add... method. For example, a line returns the endpoint, while a cubic
// returns two tangent points and the endpoint.
struct PathElement {
  PathElementType type;
  FloatPoint* points;
};

typedef void (*PathApplierFunction)(void* info, const PathElement*);

class PLATFORM_EXPORT Path {
  USING_FAST_MALLOC(Path);

 public:
  Path();
  ~Path();

  Path(const Path&);
  Path(const SkPath&);
  Path& operator=(const Path&);
  Path& operator=(const SkPath&);
  bool operator==(const Path&) const;
  bool operator!=(const Path& other) const { return !(*this == other); }

  bool Contains(const FloatPoint&) const;
  bool Contains(const FloatPoint&, WindRule) const;
  bool StrokeContains(const FloatPoint&, const StrokeData&) const;

  FloatRect BoundingRect() const;
  FloatRect StrokeBoundingRect(const StrokeData&) const;

  float length() const;
  FloatPoint PointAtLength(float length) const;
  void PointAndNormalAtLength(float length, FloatPoint&, float&) const;

  // Helper for computing a sequence of positions and normals (normal angles) on
  // a path. The best possible access pattern will be one where the |length|
  // value is strictly increasing. For other access patterns, performance will
  // vary depending on curvature and number of segments, but should never be
  // worse than that of the state-less method on Path.
  class PLATFORM_EXPORT PositionCalculator {
    DISALLOW_COPY_AND_ASSIGN(PositionCalculator);
    USING_FAST_MALLOC(PositionCalculator);

   public:
    explicit PositionCalculator(const Path&);

    void PointAndNormalAtLength(float length, FloatPoint&, float&);

   private:
    SkPath path_;
    SkPathMeasure path_measure_;
    SkScalar accumulated_length_;
  };

  void Clear();
  bool IsEmpty() const;
  bool IsClosed() const;

  // Specify whether this path is volatile. Temporary paths that are discarded
  // or modified after use should be marked as volatile. This is a hint to the
  // device to not cache this path.
  void SetIsVolatile(bool);

  // Gets the current point of the current path, which is conceptually the final
  // point reached by the path so far. Note the Path can be empty
  // (isEmpty() == true) and still have a current point.
  bool HasCurrentPoint() const;
  FloatPoint CurrentPoint() const;

  void SetWindRule(const WindRule);

  void MoveTo(const FloatPoint&);
  void AddLineTo(const FloatPoint&);
  void AddQuadCurveTo(const FloatPoint& control_point,
                      const FloatPoint& end_point);
  void AddBezierCurveTo(const FloatPoint& control_point1,
                        const FloatPoint& control_point2,
                        const FloatPoint& end_point);
  void AddArcTo(const FloatPoint&, const FloatPoint&, float radius);
  void AddArcTo(const FloatPoint&,
                const FloatSize& r,
                float x_rotate,
                bool large_arc,
                bool sweep);
  void CloseSubpath();

  void AddArc(const FloatPoint&,
              float radius,
              float start_angle,
              float end_angle);
  void AddRect(const FloatRect&);
  void AddEllipse(const FloatPoint&,
                  float radius_x,
                  float radius_y,
                  float rotation,
                  float start_angle,
                  float end_angle);
  void AddEllipse(const FloatRect&);

  void AddRoundedRect(const FloatRect&, const FloatSize& rounding_radii);
  void AddRoundedRect(const FloatRect&,
                      const FloatSize& top_left_radius,
                      const FloatSize& top_right_radius,
                      const FloatSize& bottom_left_radius,
                      const FloatSize& bottom_right_radius);
  void AddRoundedRect(const FloatRoundedRect&);

  void AddPath(const Path&, const AffineTransform&);

  void Translate(const FloatSize&);

  const SkPath& GetSkPath() const { return path_; }

  void Apply(void* info, PathApplierFunction) const;
  void Transform(const AffineTransform&);

  void AddPathForRoundedRect(const FloatRect&,
                             const FloatSize& top_left_radius,
                             const FloatSize& top_right_radius,
                             const FloatSize& bottom_left_radius,
                             const FloatSize& bottom_right_radius);

  bool SubtractPath(const Path&);

  // Updates the path to the union (inclusive-or) of itself with the given
  // argument.
  bool UnionPath(const Path& other);

  bool IntersectPath(const Path& other);

 private:
  void AddEllipse(const FloatPoint&,
                  float radius_x,
                  float radius_y,
                  float start_angle,
                  float end_angle);
  SkPath StrokePath(const StrokeData&) const;

  SkPath path_;
};

class PLATFORM_EXPORT RefCountedPath : public blink::Path,
                                       public RefCounted<RefCountedPath> {
  USING_FAST_MALLOC(RefCountedPath);

 public:
  template <typename... Args>
  RefCountedPath(Args&&... args) : blink::Path(std::forward<Args>(args)...) {}
};

// Only used for DCHECKs
PLATFORM_EXPORT bool EllipseIsRenderable(float start_angle, float end_angle);

}  // namespace blink

#endif

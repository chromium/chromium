// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_RAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_RAY_H_

#include "third_party/blink/renderer/core/style/basic_shapes.h"

namespace gfx {
class PointF;
}

namespace blink {

struct PointAndTangent;
struct StyleOffsetRotation;

class StyleRay : public BasicShape {
 public:
  enum class RaySize {
    kClosestSide,
    kClosestCorner,
    kFarthestSide,
    kFarthestCorner,
    kSides
  };

  static scoped_refptr<StyleRay> Create(float angle, RaySize, bool contain);
  ~StyleRay() override = default;

  float CalculateRayPathLength(const gfx::PointF& initial_position,
                               const gfx::SizeF& containing_box_size) const;
  float CalculateLength(const gfx::PointF& anchor,
                        const Length& offset_distance,
                        const StyleOffsetRotation& offset_rotate,
                        const gfx::PointF& initial_position,
                        const gfx::RectF& bounding_box,
                        const gfx::SizeF& containing_box_size) const;
  PointAndTangent PointAndNormalAtLength(float length) const;

  float Angle() const { return ClampTo<float, float>(angle_); }
  RaySize Size() const { return size_; }
  bool Contain() const { return contain_; }

  void GetPath(Path&, const gfx::RectF&, float) override;

  ShapeType GetType() const override { return kStyleRayType; }

 protected:
  bool IsEqualAssumingSameType(const BasicShape&) const override;

 private:
  StyleRay(float angle, RaySize, bool contain);

  float angle_;
  RaySize size_;
  bool contain_;
};

template <>
struct DowncastTraits<StyleRay> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kStyleRayType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_RAY_H_

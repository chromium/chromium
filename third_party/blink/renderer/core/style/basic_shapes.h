/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BASIC_SHAPES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BASIC_SHAPES_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FloatRect;
class FloatSize;
class Path;

class CORE_EXPORT BasicShape : public RefCounted<BasicShape> {
  USING_FAST_MALLOC(BasicShape);

 public:
  virtual ~BasicShape() = default;

  enum ShapeType {
    kBasicShapeEllipseType,
    kBasicShapePolygonType,
    kBasicShapeCircleType,
    kBasicShapeInsetType,
    kStyleRayType,
    kStylePathType
  };

  bool IsSameType(const BasicShape& other) const {
    return GetType() == other.GetType();
  }

  virtual void GetPath(Path&, const FloatRect&) = 0;
  virtual WindRule GetWindRule() const { return RULE_NONZERO; }
  virtual bool operator==(const BasicShape&) const = 0;

  virtual ShapeType GetType() const = 0;

 protected:
  BasicShape() = default;
};

class BasicShapeCenterCoordinate {
  DISALLOW_NEW();

 public:
  enum Direction { kTopLeft, kBottomRight };

  BasicShapeCenterCoordinate(Direction direction = kTopLeft,
                             const Length& length = Length::Fixed(0))
      : direction_(direction),
        length_(length),
        computed_length_(direction == kTopLeft
                             ? length
                             : length.SubtractFromOneHundredPercent()) {}

  BasicShapeCenterCoordinate(const BasicShapeCenterCoordinate& other)
      : direction_(other.GetDirection()),
        length_(other.length()),
        computed_length_(other.computed_length_) {}

  bool operator==(const BasicShapeCenterCoordinate& other) const {
    return direction_ == other.direction_ && length_ == other.length_ &&
           computed_length_ == other.computed_length_;
  }

  Direction GetDirection() const { return direction_; }
  const Length& length() const { return length_; }
  const Length& ComputedLength() const { return computed_length_; }

 private:
  Direction direction_;
  Length length_;
  Length computed_length_;
};

class BasicShapeRadius {
  DISALLOW_NEW();

 public:
  enum RadiusType { kValue, kClosestSide, kFarthestSide };
  BasicShapeRadius() : type_(kClosestSide) {}
  explicit BasicShapeRadius(const Length& v) : value_(v), type_(kValue) {}
  explicit BasicShapeRadius(RadiusType t) : type_(t) {}
  BasicShapeRadius(const BasicShapeRadius& other)
      : value_(other.Value()), type_(other.GetType()) {}
  bool operator==(const BasicShapeRadius& other) const {
    return type_ == other.type_ && value_ == other.value_;
  }

  const Length& Value() const { return value_; }
  RadiusType GetType() const { return type_; }

 private:
  Length value_;
  RadiusType type_;
};

class CORE_EXPORT BasicShapeCircle final : public BasicShape {
 public:
  static scoped_refptr<BasicShapeCircle> Create() {
    return base::AdoptRef(new BasicShapeCircle);
  }

  const BasicShapeCenterCoordinate& CenterX() const { return center_x_; }
  const BasicShapeCenterCoordinate& CenterY() const { return center_y_; }
  const BasicShapeRadius& Radius() const { return radius_; }

  float FloatValueForRadiusInBox(FloatSize) const;
  void SetCenterX(BasicShapeCenterCoordinate center_x) { center_x_ = center_x; }
  void SetCenterY(BasicShapeCenterCoordinate center_y) { center_y_ = center_y; }
  void SetRadius(BasicShapeRadius radius) { radius_ = radius; }

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeCircleType; }

 private:
  BasicShapeCircle() = default;

  BasicShapeCenterCoordinate center_x_;
  BasicShapeCenterCoordinate center_y_;
  BasicShapeRadius radius_;
};

template <>
struct DowncastTraits<BasicShapeCircle> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kBasicShapeCircleType;
  }
};

class BasicShapeEllipse final : public BasicShape {
 public:
  static scoped_refptr<BasicShapeEllipse> Create() {
    return base::AdoptRef(new BasicShapeEllipse);
  }

  const BasicShapeCenterCoordinate& CenterX() const { return center_x_; }
  const BasicShapeCenterCoordinate& CenterY() const { return center_y_; }
  const BasicShapeRadius& RadiusX() const { return radius_x_; }
  const BasicShapeRadius& RadiusY() const { return radius_y_; }
  float FloatValueForRadiusInBox(const BasicShapeRadius&,
                                 float center,
                                 float box_width_or_height) const;

  void SetCenterX(BasicShapeCenterCoordinate center_x) { center_x_ = center_x; }
  void SetCenterY(BasicShapeCenterCoordinate center_y) { center_y_ = center_y; }
  void SetRadiusX(BasicShapeRadius radius_x) { radius_x_ = radius_x; }
  void SetRadiusY(BasicShapeRadius radius_y) { radius_y_ = radius_y; }

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeEllipseType; }

 private:
  BasicShapeEllipse() = default;

  BasicShapeCenterCoordinate center_x_;
  BasicShapeCenterCoordinate center_y_;
  BasicShapeRadius radius_x_;
  BasicShapeRadius radius_y_;
};

template <>
struct DowncastTraits<BasicShapeEllipse> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kBasicShapeEllipseType;
  }
};

class BasicShapePolygon final : public BasicShape {
 public:
  static scoped_refptr<BasicShapePolygon> Create() {
    return base::AdoptRef(new BasicShapePolygon);
  }

  const Vector<Length>& Values() const { return values_; }

  void SetWindRule(WindRule wind_rule) { wind_rule_ = wind_rule; }
  void AppendPoint(const Length& x, const Length& y) {
    values_.push_back(x);
    values_.push_back(y);
  }

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  WindRule GetWindRule() const override { return wind_rule_; }

  ShapeType GetType() const override { return kBasicShapePolygonType; }

 private:
  BasicShapePolygon() : wind_rule_(RULE_NONZERO) {}

  WindRule wind_rule_;
  Vector<Length> values_;
};

template <>
struct DowncastTraits<BasicShapePolygon> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kBasicShapePolygonType;
  }
};

class BasicShapeInset : public BasicShape {
 public:
  static scoped_refptr<BasicShapeInset> Create() {
    return base::AdoptRef(new BasicShapeInset);
  }

  const Length& Top() const { return top_; }
  const Length& Right() const { return right_; }
  const Length& Bottom() const { return bottom_; }
  const Length& Left() const { return left_; }

  const LengthSize& TopLeftRadius() const { return top_left_radius_; }
  const LengthSize& TopRightRadius() const { return top_right_radius_; }
  const LengthSize& BottomRightRadius() const { return bottom_right_radius_; }
  const LengthSize& BottomLeftRadius() const { return bottom_left_radius_; }

  void SetTop(const Length& top) { top_ = top; }
  void SetRight(const Length& right) { right_ = right; }
  void SetBottom(const Length& bottom) { bottom_ = bottom; }
  void SetLeft(const Length& left) { left_ = left; }

  void SetTopLeftRadius(const LengthSize& radius) { top_left_radius_ = radius; }
  void SetTopRightRadius(const LengthSize& radius) {
    top_right_radius_ = radius;
  }
  void SetBottomRightRadius(const LengthSize& radius) {
    bottom_right_radius_ = radius;
  }
  void SetBottomLeftRadius(const LengthSize& radius) {
    bottom_left_radius_ = radius;
  }

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kBasicShapeInsetType; }

 private:
  BasicShapeInset() = default;

  Length right_;
  Length top_;
  Length bottom_;
  Length left_;

  LengthSize top_left_radius_;
  LengthSize top_right_radius_;
  LengthSize bottom_right_radius_;
  LengthSize bottom_left_radius_;
};

template <>
struct DowncastTraits<BasicShapeInset> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kBasicShapeInsetType;
  }
};

}  // namespace blink
#endif

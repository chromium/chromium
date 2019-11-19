// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_RAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_RAY_H_

#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

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

  float Angle() const { return angle_; }
  RaySize Size() const { return size_; }
  bool Contain() const { return contain_; }

  void GetPath(Path&, const FloatRect&) override;
  bool operator==(const BasicShape&) const override;

  ShapeType GetType() const override { return kStyleRayType; }

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

#endif

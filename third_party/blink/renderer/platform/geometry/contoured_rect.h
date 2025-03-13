// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CONTOURED_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CONTOURED_RECT_H_

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {
class QuadF;
}

namespace blink {

class Path;

// A ContouredRect is a rect with corners that can have arbitrary
// radius and curvature, as in not necessarily rounded.
// It is a superset of FloatRoundedRect, with the added information needed
// to render corner shapes correctly.
class PLATFORM_EXPORT ContouredRect {
  DISALLOW_NEW();

 public:
  // A corner curvature is the exponent of a superellipse quadrant.
  // e.g. 2 is round, 1 is bevel/angled, infinity is defunct/straight.
  class CornerCurvature {
   public:
    static constexpr float kRound = 2;
    static constexpr float kBevel = 1;

    constexpr CornerCurvature() = default;
    constexpr CornerCurvature(float top_left,
                              float top_right,
                              float bottom_right,
                              float bottom_left)
        : top_left_(top_left),
          top_right_(top_right),
          bottom_right_(bottom_right),
          bottom_left_(bottom_left) {
      DCHECK_GE(top_left, 0);
      DCHECK_GE(top_right, 0);
      DCHECK_GE(bottom_right, 0);
      DCHECK_GE(bottom_left, 0);
    }

    constexpr bool IsRound() const {
      return (top_left_ == kRound) && IsUniform();
    }

    constexpr bool IsUniform() const {
      return top_left_ == top_right_ && top_left_ == bottom_right_ &&
             top_left_ == bottom_left_;
    }

    constexpr float TopLeft() const { return top_left_; }
    constexpr float TopRight() const { return top_right_; }
    constexpr float BottomRight() const { return bottom_right_; }
    constexpr float BottomLeft() const { return bottom_left_; }

    constexpr bool operator==(const CornerCurvature&) const = default;

    String ToString() const;

   private:
    float top_left_ = kRound;
    float top_right_ = kRound;
    float bottom_right_ = kRound;
    float bottom_left_ = kRound;
  };

  constexpr ContouredRect() = default;
  constexpr explicit ContouredRect(const FloatRoundedRect& rect)
      : rect_(rect) {}
  constexpr ContouredRect(const FloatRoundedRect& rect,
                          const CornerCurvature& curvature)
      : rect_(rect), corner_curvature_(curvature) {}
  bool operator==(const ContouredRect&) const = default;
  constexpr const gfx::RectF& BoundingRect() const { return rect_.Rect(); }
  constexpr const CornerCurvature& GetCornerCurvature() const {
    return corner_curvature_;
  }

  constexpr bool HasRoundCurvature() const {
    return corner_curvature_.IsRound() || !IsRounded();
  }

  const FloatRoundedRect::Radii& GetRadii() const { return rect_.GetRadii(); }

  void SetRadii(const FloatRoundedRect::Radii& radii) { rect_.SetRadii(radii); }

  bool IsRounded() const { return rect_.IsRounded(); }

  const FloatRoundedRect& AsRoundedRect() const { return rect_; }

  const gfx::RectF& Rect() const { return rect_.Rect(); }

  constexpr bool IsEmpty() const { return rect_.IsEmpty(); }

  void SetRoundedRect(const FloatRoundedRect& rect) { rect_ = rect; }
  void SetCornerCurvature(const CornerCurvature& curvature) {
    corner_curvature_ = curvature;
  }

  void Move(const gfx::Vector2dF& offset) { rect_.Move(offset); }
  void Outset(const gfx::OutsetsF& outsets);
  void Outset(float outset) { Outset(gfx::OutsetsF(outset)); }
  void Inset(const gfx::InsetsF& insets) { Outset(insets.ToOutsets()); }
  void Inset(float inset) { Inset(gfx::InsetsF(inset)); }
  void OutsetForMarginOrShadow(const gfx::OutsetsF& outsets);
  void OutsetForMarginOrShadow(float outset) {
    OutsetForMarginOrShadow(gfx::OutsetsF(outset));
  }

  void OutsetForShapeMargin(float outset);
  bool IntersectsQuad(const gfx::QuadF&) const;

  // Whether the radii are constrained in the size of rect().
  bool IsRenderable() const { return rect_.IsRenderable(); }
  String ToString() const;
  Path GetPath() const;

 private:
  FloatRoundedRect rect_;
  CornerCurvature corner_curvature_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CONTOURED_RECT_H_

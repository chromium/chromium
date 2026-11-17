// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_clip_path_value.h"

#include <array>

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

void CompositorClipPathValue::Draw(cc::PaintCanvas* canvas) const {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (rrect_) {
    canvas->drawRRect(*rrect_, flags);
  } else {
    canvas->drawPath(path_, flags);
  }
}

void CompositorClipPathValue::Interpolate(
    const CompositorClipPathValue& other,
    float progress,
    CompositorClipPathValue* result) const {
  if (!AreShapeTypesInterpolable(other)) {
    InterpolateDiscretely(other, progress, result);
    return;
  }

  // Interpolating RRects is cheaper, so we do it if possible.
  if (rrect_ && other.rrect_) {
    result->rrect_ = InterpolateRRects(*rrect_, *other.rrect_, progress);
    return;
  }

  // Exclude differing numbers of points.
  if (!ArePathsInterpolatable(other)) {
    InterpolateDiscretely(other, progress, result);
    return;
  }

  result->rrect_ = std::nullopt;
  other.path_.interpolate(path_, progress, &result->path_);
}

const SkRect& CompositorClipPathValue::GetBounds() const {
  if (rrect_) {
    return rrect_->rect();
  }
  return path_.getBounds();
}

// static
CompositorClipPathValue CompositorClipPathValue::FromShape(
    const BasicShape& shape,
    const gfx::RectF& reference_box,
    const gfx::Vector2dF& clip_offset,
    float zoom) {
  Path path = shape.GetPath(reference_box, zoom, /*path_scale=*/1.f);
  if (!clip_offset.IsZero()) {
    path = PathBuilder(path).Translate(clip_offset).Finalize();
  }
  return CompositorClipPathValue(path.GetSkPath(), path.MaybeConvertToSkRRect(),
                                 shape.GetType(), false);
}

// static
CompositorClipPathValue CompositorClipPathValue::FromGeometryBox(
    ContouredRect& box,
    const gfx::Vector2dF& clip_offset) {
  if (!clip_offset.IsZero()) {
    box.Move(clip_offset);
  }
  return CompositorClipPathValue(box.GetPath().GetSkPath(),
                                 box.GetPath().MaybeConvertToSkRRect(),
                                 std::nullopt, true);
}

// static
CompositorClipPathValue CompositorClipPathValue::FromRectF(
    const gfx::RectF& rect) {
  return CompositorClipPathValue(SkPath::Rect(gfx::RectFToSkRect(rect)),
                                 std::nullopt, std::nullopt, false);
}

CompositorClipPathValue::CompositorClipPathValue(
    const SkPath& path,
    const std::optional<SkRRect>& rrect,
    const std::optional<BasicShape::ShapeType>& shape_type,
    bool is_geometry_box)
    : path_(path),
      rrect_(rrect),
      shape_type_(shape_type),
      is_geometry_box_(is_geometry_box) {}

bool CompositorClipPathValue::AreShapeTypesInterpolable(
    const CompositorClipPathValue& other) const {
  bool can_interpolate_shapes = !is_geometry_box_ && shape_type_.has_value() &&
                                other.shape_type_.has_value() &&
                                shape_type_ == other.shape_type_;
  // TODO(crbug.com/379052285): Clarify geometry box behavior. We currently
  // don't support the case where both a geometry box and a shape are specified.
  bool can_interpolate_boxes = is_geometry_box_ && other.is_geometry_box_;
  return can_interpolate_shapes || can_interpolate_boxes;
}

bool CompositorClipPathValue::ArePathsInterpolatable(
    const CompositorClipPathValue& other) const {
  return path_.isInterpolatable(other.path_);
}

void CompositorClipPathValue::InterpolateDiscretely(
    const CompositorClipPathValue& other,
    float progress,
    CompositorClipPathValue* result) const {
  if (progress < 0.5f) {
    *result = *this;
  } else {
    *result = other;
  }
}

namespace {

bool RectsAreRoughlyEqual(const SkRect& a, const SkRect& b) {
  return SkScalarNearlyEqual(a.left(), b.left()) &&
         SkScalarNearlyEqual(a.top(), b.top()) &&
         SkScalarNearlyEqual(a.right(), b.right()) &&
         SkScalarNearlyEqual(a.bottom(), b.bottom());
}

bool RRectsAreRoughlyEqual(const SkRRect& a, const SkRRect& b) {
  if (!RectsAreRoughlyEqual(a.rect(), b.rect())) {
    return false;
  }

  for (SkRRect::Corner corner :
       {SkRRect::kUpperLeft_Corner, SkRRect::kUpperRight_Corner,
        SkRRect::kLowerRight_Corner, SkRRect::kLowerLeft_Corner}) {
    if (!SkScalarNearlyEqual(a.radii(corner).x(), b.radii(corner).x()) ||
        !SkScalarNearlyEqual(a.radii(corner).y(), b.radii(corner).y())) {
      return false;
    }
  }
  return true;
}

bool PathsAreRoughlyEqual(const SkPath& a, const SkPath& b) {
  if (a.getFillType() != b.getFillType() ||
      a.verbs().size() != b.verbs().size() ||
      a.points().size() != b.points().size() ||
      a.conicWeights().size() != b.conicWeights().size()) {
    return false;
  }

  for (size_t i = 0; i < a.verbs().size(); ++i) {
    if (a.verbs()[i] != b.verbs()[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < a.points().size(); ++i) {
    if (!SkScalarNearlyEqual(a.points()[i].x(), b.points()[i].x()) ||
        !SkScalarNearlyEqual(a.points()[i].y(), b.points()[i].y())) {
      return false;
    }
  }
  for (size_t i = 0; i < a.conicWeights().size(); ++i) {
    if (!SkScalarNearlyEqual(a.conicWeights()[i], b.conicWeights()[i])) {
      return false;
    }
  }
  return true;
}

SkScalar Lerp(SkScalar a, SkScalar b, float progress) {
  return a + (b - a) * progress;
}

SkVector LerpCorner(const SkRRect& from,
                    const SkRRect& to,
                    SkRRect::Corner corner,
                    float progress) {
  const SkVector from_radii = from.radii(corner);
  const SkVector to_radii = to.radii(corner);
  return SkVector{Lerp(from_radii.fX, to_radii.fX, progress),
                  Lerp(from_radii.fY, to_radii.fY, progress)};
}

}  // namespace

bool CompositorClipPathValue::IsRoughlyEqualForTesting(
    const CompositorClipPathValue& other) const {
  if (rrect_.has_value() != other.rrect_.has_value()) {
    return false;
  }
  if (rrect_) {
    return RRectsAreRoughlyEqual(*rrect_, *other.rrect_);
  }
  return PathsAreRoughlyEqual(path_, other.path_);
}

// static
SkRRect CompositorClipPathValue::InterpolateRRects(const SkRRect& from,
                                                   const SkRRect& to,
                                                   float progress) {
  const std::array<SkVector, 4> radii = {
      LerpCorner(from, to, SkRRect::kUpperLeft_Corner, progress),
      LerpCorner(from, to, SkRRect::kUpperRight_Corner, progress),
      LerpCorner(from, to, SkRRect::kLowerRight_Corner, progress),
      LerpCorner(from, to, SkRRect::kLowerLeft_Corner, progress)};
  const SkRect& fromRect = from.rect();
  const SkRect& toRect = to.rect();
  SkRRect result;
  result.setRectRadii(
      SkRect::MakeLTRB(Lerp(fromRect.fLeft, toRect.fLeft, progress),
                       Lerp(fromRect.fTop, toRect.fTop, progress),
                       Lerp(fromRect.fRight, toRect.fRight, progress),
                       Lerp(fromRect.fBottom, toRect.fBottom, progress)),
      radii.data());
  return result;
}

}  // namespace blink

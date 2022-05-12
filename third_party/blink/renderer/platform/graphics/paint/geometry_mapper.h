// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

// Clips can use gfx::RectF::Intersect or gfx::RectF::InclusiveIntersect.
enum InclusiveIntersectOrNot { kNonInclusiveIntersect, kInclusiveIntersect };

// When performing overlap testing during compositing, we may need to expand the
// visual rect in two cases when mapping from descendant state to ancestor
// state: mapping through a fixed transform node to the viewport it is attached
// to, and mapping through an animating transform or filter.
//
// This allows for a more conservative overlap test that assumes potentially
// more overlap than we'd encounter otherwise, in order to reduce the need to
// re-run overlap testing in response to things like scrolling.
//
// The expansion for fixed covers all coordinates where the fixed content may
// end up when the scroller is at the end of the extents.
//
// For animation, the visual or clip rect is expanded to infinity when we meet
// any animating transform or filter when walking from a descendant state to an
// ancestor state, when mapping a visual rect or getting the accumulated clip
// rect. After we expanded the rect, we will still apply ancestor clips when
// continuing walking up the tree. TODO(crbug.com/1026653): Consider animation
// bounds instead of using infinite rect.
enum ExpandVisualRectForCompositingOverlapOrNot {
  kDontExpandVisualRectForCompositingOverlap,
  kExpandVisualRectForCompositingOverlap,
};

// GeometryMapper is a helper class for fast computations of transformed and
// visual rects in different PropertyTreeStates. The design document has a
// number of details on use cases, algorithmic definitions, and running times.
//
// NOTE: A GeometryMapper object is only valid for property trees that do not
// change. If any mutation occurs, a new GeometryMapper object must be allocated
// corresponding to the new state.
//
// Design document: http://bit.ly/28P4FDA
class PLATFORM_EXPORT GeometryMapper {
  STATIC_ONLY(GeometryMapper);

 public:
  // The return value of SourceToDestinationProjection. If the result is known
  // to be accumulation of 2d translations, |matrix| is nullptr, and
  // |translation_2d| is the accumulated 2d translation. Otherwise |matrix|
  // points to the accumulated projection, and |translation_2d| is zero.
  class Translation2DOrMatrix {
    DISALLOW_NEW();

   public:
    Translation2DOrMatrix() { DCHECK(IsIdentity()); }
    explicit Translation2DOrMatrix(const gfx::Vector2dF& translation_2d)
        : translation_2d_(translation_2d) {
      DCHECK(IsIdentityOr2DTranslation());
    }
    explicit Translation2DOrMatrix(const TransformationMatrix& matrix)
        : matrix_(matrix) {
      DCHECK(!IsIdentityOr2DTranslation());
    }

    bool IsIdentity() const { return !matrix_ && translation_2d_.IsZero(); }
    bool IsIdentityOr2DTranslation() const { return !matrix_; }
    const gfx::Vector2dF& Translation2D() const {
      DCHECK(IsIdentityOr2DTranslation());
      return translation_2d_;
    }
    const TransformationMatrix& Matrix() const {
      DCHECK(!IsIdentityOr2DTranslation());
      return *matrix_;
    }

    template <typename Rect>
    void MapRect(Rect& rect) const {
      if (LIKELY(IsIdentityOr2DTranslation()))
        MoveRect(rect, Translation2D());
      else
        rect = Matrix().MapRect(rect);
    }

    void MapQuad(gfx::QuadF& quad) const {
      if (LIKELY(IsIdentityOr2DTranslation()))
        quad += Translation2D();
      else
        quad = Matrix().MapQuad(quad);
    }

    void MapFloatClipRect(FloatClipRect& rect) const {
      if (LIKELY(IsIdentityOr2DTranslation()))
        rect.Move(Translation2D());
      else
        rect.Map(Matrix());
    }

    gfx::PointF MapPoint(const gfx::PointF& point) const {
      if (LIKELY(IsIdentityOr2DTranslation()))
        return point + Translation2D();
      return Matrix().MapPoint(point);
    }

    void PostTranslate(float x, float y) {
      if (LIKELY(IsIdentityOr2DTranslation()))
        translation_2d_ += gfx::Vector2dF(x, y);
      else
        matrix_->PostTranslate(x, y);
    }

    SkM44 ToSkM44() const { return Matrix().ToSkM44(); }

    SkMatrix ToSkMatrix() const {
      if (LIKELY(IsIdentityOr2DTranslation())) {
        return SkMatrix::Translate(Translation2D().x(), Translation2D().y());
      }
      return Matrix().ToSkM44().asM33();
    }

    bool operator==(const Translation2DOrMatrix& other) const {
      return translation_2d_ == other.translation_2d_ &&
             matrix_ == other.matrix_;
    }

    bool operator!=(const Translation2DOrMatrix& other) const {
      return !(*this == other);
    }

   private:
    gfx::Vector2dF translation_2d_;
    absl::optional<TransformationMatrix> matrix_;
  };

  // Returns the matrix that is suitable to map geometries on the source plane
  // to some backing in the destination plane.
  // Formal definition:
  //   output = flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // There are some cases that flatten(destination_to_screen) being
  // singular yet we can still define a reasonable projection, for example:
  // 1. Both nodes inherited a common singular flat ancestor:
  // 2. Both nodes are co-planar to a common singular ancestor:
  // Not every cases outlined above are supported!
  // Read implementation comments for specific restrictions.
  static Translation2DOrMatrix SourceToDestinationProjection(
      const TransformPaintPropertyNodeOrAlias& source,
      const TransformPaintPropertyNodeOrAlias& destination) {
    return SourceToDestinationProjection(source.Unalias(),
                                         destination.Unalias());
  }
  static Translation2DOrMatrix SourceToDestinationProjection(
      const TransformPaintPropertyNode& source,
      const TransformPaintPropertyNode& destination);

  // Same as SourceToDestinationProjection() except that it maps the rect
  // rather than returning the matrix.
  // |mapping_rect| is both input and output. Its type can be gfx::RectF,
  // LayoutRect, gfx::Rect, gfx::Rect or gfx::RectF.
  template <typename Rect>
  static void SourceToDestinationRect(
      const TransformPaintPropertyNodeOrAlias& source,
      const TransformPaintPropertyNodeOrAlias& destination,
      Rect& mapping_rect) {
    SourceToDestinationRect(source.Unalias(), destination.Unalias(),
                            mapping_rect);
  }
  template <typename Rect>
  static void SourceToDestinationRect(
      const TransformPaintPropertyNode& source,
      const TransformPaintPropertyNode& destination,
      Rect& mapping_rect) {
    SourceToDestinationProjection(source, destination).MapRect(mapping_rect);
  }

  // Returns the clip rect between |local_state| and |ancestor_state|. The clip
  // rect is the total clip rect that should be applied when painting contents
  // of |local_state| in |ancestor_state| space. Because this clip rect applies
  // on contents of |local_state|, it's not affected by any effect nodes between
  // |local_state| and |ancestor_state|.
  //
  // The LayoutClipRect of any clip nodes is used, *not* the PaintClipRect.
  //
  // Note that the clip of |ancestor_state| is *not* applied.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.

  static FloatClipRect LocalToAncestorClipRect(
      const PropertyTreeStateOrAlias& local_state,
      const PropertyTreeStateOrAlias& ancestor_state,
      OverlayScrollbarClipBehavior behavior = kIgnoreOverlayScrollbarSize) {
    return LocalToAncestorClipRect(local_state.Unalias(),
                                   ancestor_state.Unalias(), behavior);
  }
  static FloatClipRect LocalToAncestorClipRect(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize);

  // Maps from a rect in |local_state| to its visual rect in |ancestor_state|.
  // If there is no effect node between |local_state| (included) and
  // |ancestor_state| (not included), the result is computed by multiplying the
  // rect by its combined transform between |local_state| and |ancestor_space|,
  // then flattening into 2D space, then intersecting by the clip for
  // |local_state|'s clips. If there are any pixel-moving effect nodes between
  // |local_state| and |ancestor_state|, for each segment of states separated
  // by the effect nodes, we'll execute the above process and map the result
  // rect with the effect.
  //
  // Note that the clip of |ancestor_state| is *not* applied.
  //
  // DCHECK fails if any of the paint property tree nodes in |local_state| are
  // not equal to or a descendant of that in |ancestor_state|.
  //
  // |mapping_rect| is both input and output.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  //
  // Returns true if the mapped rect is non-empty. (Note: this has special
  // meaning in the presence of inclusive intersection.)
  //
  // Note: if inclusive intersection is specified, then the
  // GeometryMapperClipCache is bypassed (the GeometryMapperTRansformCache is
  // still used, however).
  //
  // If kInclusiveIntersect is set, clipping operations will
  // use gfx::RectF::InclusiveIntersect, and the return value of
  // InclusiveIntersect will be propagated to the return value of this method.
  // Otherwise, clipping operations will use LayoutRect::intersect, and the
  // return value will be true only if the clipped rect has non-zero area.
  // See the documentation for gfx::RectF::InclusiveIntersect for more
  // information.
  static bool LocalToAncestorVisualRect(
      const PropertyTreeStateOrAlias& local_state,
      const PropertyTreeStateOrAlias& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior clip = kIgnoreOverlayScrollbarSize,
      InclusiveIntersectOrNot intersect = kNonInclusiveIntersect,
      ExpandVisualRectForCompositingOverlapOrNot expand =
          kDontExpandVisualRectForCompositingOverlap) {
    return LocalToAncestorVisualRect(local_state.Unalias(),
                                     ancestor_state.Unalias(), mapping_rect,
                                     clip, intersect, expand);
  }
  static bool LocalToAncestorVisualRect(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize,
      InclusiveIntersectOrNot = kNonInclusiveIntersect,
      ExpandVisualRectForCompositingOverlapOrNot =
          kDontExpandVisualRectForCompositingOverlap);

  static void ClearCache();

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  static Translation2DOrMatrix SourceToDestinationProjectionInternal(
      const TransformPaintPropertyNode& source,
      const TransformPaintPropertyNode& destination,
      bool& has_animation,
      bool& has_fixed,
      bool& success);

  static FloatClipRect LocalToAncestorClipRectInternal(
      const ClipPaintPropertyNode& descendant,
      const ClipPaintPropertyNode& ancestor_clip,
      const TransformPaintPropertyNode& ancestor_transform,
      OverlayScrollbarClipBehavior,
      InclusiveIntersectOrNot,
      ExpandVisualRectForCompositingOverlapOrNot,
      bool& success);

  // The return value has the same meaning as that for
  // LocalToAncestorVisualRect.
  static bool LocalToAncestorVisualRectInternal(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior,
      InclusiveIntersectOrNot,
      ExpandVisualRectForCompositingOverlapOrNot,
      bool& success);

  // The return value has the same meaning as that for
  // LocalToAncestorVisualRect.
  static bool SlowLocalToAncestorVisualRectWithEffects(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior,
      InclusiveIntersectOrNot,
      ExpandVisualRectForCompositingOverlapOrNot,
      bool& success);

  static void MoveRect(gfx::RectF& rect, const gfx::Vector2dF& delta) {
    rect.Offset(delta.x(), delta.y());
  }

  static void MoveRect(LayoutRect& rect, const gfx::Vector2dF& delta) {
    rect.Move(LayoutSize(delta.x(), delta.y()));
  }

  static void MoveRect(gfx::Rect& rect, const gfx::Vector2dF& delta) {
    gfx::RectF rect_f(rect);
    MoveRect(rect_f, delta);
    rect = gfx::ToEnclosingRect(rect_f);
  }

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/contoured_border_geometry.h"
#include "third_party/blink/renderer/core/paint/geometry_box_utils.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/geometry_box_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

namespace {

// Used for CompositedStateResolutionType::kInitial. The following failure
// reasons are allowed when computing the composited paint status prior to
// initializing paint properties.
const CompositorAnimations::FailureReasons kDeferredDecisionReasons =
    CompositorAnimations::kTargetHasInvalidCompositingState;

SVGResourceClient* GetResourceClient(const LayoutObject& object) {
  if (object.IsSVGChild())
    return SVGResources::GetClient(object);
  CHECK(object.IsBoxModelObject());
  return To<LayoutBoxModelObject>(object).Layer()->ResourceInfo();
}

LayoutSVGResourceClipper* ResolveElementReference(
    const LayoutObject& object,
    const ReferenceClipPathOperation& reference_clip_path_operation) {
  SVGResourceClient* client = GetResourceClient(object);
  // We may not have a resource client for some non-rendered elements (like
  // filter primitives) that we visit during paint property tree construction.
  if (!client)
    return nullptr;
  LayoutSVGResourceClipper* resource_clipper =
      GetSVGResourceAsType(*client, reference_clip_path_operation);
  if (!resource_clipper)
    return nullptr;

  resource_clipper->ClearInvalidationMask();
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*resource_clipper))
    return nullptr;

  SECURITY_DCHECK(!resource_clipper->SelfNeedsFullLayout());
  return resource_clipper;
}

PhysicalRect BorderBoxRect(const LayoutBoxModelObject& object) {
  // It is complex to map from an SVG border box to a reference box (for
  // example, `GeometryBox::kViewBox` is independent of the border box) so we
  // use `SVGResources::ReferenceBoxForEffects` for SVG reference boxes.
  CHECK(!object.IsSVGChild());

  if (auto* box = DynamicTo<LayoutBox>(object)) {
    // If the box is fragment-less return an empty box.
    if (box->PhysicalFragmentCount() == 0u) {
      return PhysicalRect();
    }
    return box->PhysicalBorderBoxRect();
  }

  // The spec doesn't say what to do if there are multiple lines. Gecko uses the
  // first fragment in that case. We'll do the same here.
  // See: https://crbug.com/641907
  const LayoutInline& layout_inline = To<LayoutInline>(object);
  if (layout_inline.IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(layout_inline);
    if (cursor) {
      return cursor.Current().RectInContainerFragment();
    }
  }
  return PhysicalRect();
}

// Should the paint offset be applied to clip-path geometry for
// `clip_path_owner`?
bool UsesPaintOffset(const LayoutObject& clip_path_owner) {
  return !clip_path_owner.IsSVGChild();
}

CompositedPaintStatus CompositeClipPathStatus(Node* node) {
  Element* element = DynamicTo<Element>(node);
  if (!element) {
    return CompositedPaintStatus::kNoAnimation;
  }

  ElementAnimations* element_animations = element->GetElementAnimations();
  if (!element_animations) {
    return CompositedPaintStatus::kNoAnimation;
  }
  return element_animations->CompositedClipPathStatus();
}

void SetCompositeClipPathStatus(Node* node, CompositedPaintStatus status) {
  Element* element = DynamicTo<Element>(node);
  if (!element)
    return;

  ElementAnimations* element_animations = element->GetElementAnimations();
  DCHECK(element_animations || status == CompositedPaintStatus::kNotComposited);
  if (element_animations) {
    element_animations->SetCompositedClipPathStatus(status);
  }
}

bool AdjustClipPathStatusForCompositingFailureReasons(
    const LayoutObject& layout_object,
    const Animation& animation,
    bool for_painting) {
  CompositorAnimations::FailureReasons failure_reasons =
      animation.CheckCanStartAnimationOnCompositor(nullptr);
  if (failure_reasons == CompositorAnimations::kNoFailure) {
    SetCompositeClipPathStatus(layout_object.GetNode(),
                               CompositedPaintStatus::kComposited);
    return true;
  }

  if (!for_painting && ((failure_reasons & ~kDeferredDecisionReasons) ==
                        CompositorAnimations::kNoFailure)) {
    // Status remains kNeedsRepaint
    return true;
  }

  SetCompositeClipPathStatus(layout_object.GetNode(),
                             CompositedPaintStatus::kNotComposited);
  return false;
}

void PaintWorkletBasedClip(GraphicsContext& context,
                           const LayoutObject& clip_path_owner,
                           const gfx::RectF& dst_rect,
                           const gfx::RectF& reference_box) {
  DCHECK(ClipPathClipper::HasCompositeClipPathAnimation(
      clip_path_owner,
      ClipPathClipper::CompositedStateResolutionType::kReadCache));

  ClipPathPaintImageGenerator* generator =
      clip_path_owner.GetFrame()->GetClipPathPaintImageGenerator();

  // The mask image should be the same size as the destination rect, but will
  // have an origin of 0,0 as it has its own coordinate space.
  gfx::RectF src_rect = gfx::RectF(dst_rect.size());

  float zoom = ClipPathClipper::UsesZoomedReferenceBox(clip_path_owner)
                   ? clip_path_owner.StyleRef().EffectiveZoom()
                   : 1;

  scoped_refptr<Image> paint_worklet_image = generator->Paint(
      zoom, reference_box, dst_rect, *clip_path_owner.GetNode());
  // Dark mode should always be disabled for clip mask.
  context.DrawImage(*paint_worklet_image, Image::kSyncDecode,
                    ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                    dst_rect, &src_rect, SkBlendMode::kSrcOver,
                    kRespectImageOrientation);
}

// TODO(crbug.com/454365238): Fallback point for cc clip-path animations, should
// be annotated with a histogram.
bool ClipPathAnimationShouldFallback(const LayoutObject& layout_object,
                                     bool is_in_block_fragmentation) {
  // If not all the fragments of this layout object have been populated yet, it
  // will be impossible to tell if a composited clip path animation is possible
  // or not based only on the layout object. Exclude the possibility if we're
  // fragmented.
  if (is_in_block_fragmentation) {
    return true;
  }

  // We also shouldn't composite in the case of will-change: contents.
  if (layout_object.StyleRef().SubtreeWillChangeContents()) {
    return true;
  }

  // Clip Path animations require paint properties to work. Text objects, or
  // objects without a box model are not given paint properties.
  if (layout_object.IsText() ||
      !(layout_object.IsBoxModelObject() || layout_object.IsSVG())) {
    return true;
  }

  // Reference clip paths are not supported.
  if (layout_object.StyleRef().HasClipPath() &&
      IsA<ReferenceClipPathOperation>(layout_object.StyleRef().ClipPath())) {
    return true;
  }

  // TODO(crbug.com/449152897): Backdrop-filter and clip path paint worklet
  // images are not rasterized correctly.
  if (!layout_object.StyleRef().BackdropFilter().IsEmpty()) {
    return true;
  }

  return false;
}

}  // namespace

// Is the reference box (as returned by LocalReferenceBox) for |clip_path_owner|
// zoomed with EffectiveZoom()?
bool ClipPathClipper::UsesZoomedReferenceBox(
    const LayoutObject& clip_path_owner) {
  return !clip_path_owner.IsSVGChild() || clip_path_owner.IsSVGForeignObject();
}

ContouredRect ClipPathClipper::RoundedReferenceBox(GeometryBox geometry_box,
                                                   const LayoutObject& object) {
  if (object.IsSVGChild()) {
    return ContouredRect(
        FloatRoundedRect(ClipPathClipper::LocalReferenceBox(object)));
  }

  const auto& box = To<LayoutBoxModelObject>(object);
  PhysicalRect border_box_rect = BorderBoxRect(box);
  ContouredRect contoured_border_box_rect =
      ContouredBorderGeometry::ContouredBorder(box.StyleRef(), border_box_rect);
  if (geometry_box == GeometryBox::kMarginBox) {
    contoured_border_box_rect.OutsetWithCornerCorrection(gfx::OutsetsF(
        GeometryBoxUtils::ReferenceBoxBorderBoxOutsets(geometry_box, box)));
  } else {
    contoured_border_box_rect.Outset(gfx::OutsetsF(
        GeometryBoxUtils::ReferenceBoxBorderBoxOutsets(geometry_box, box)));
  }
  return contoured_border_box_rect;
}

Animation* ClipPathClipper::GetClipPathAnimation(
    const LayoutObject& layout_object) {
  ClipPathPaintImageGenerator* generator =
      layout_object.GetFrame()->GetClipPathPaintImageGenerator();
  CHECK(generator);

  Element* element = To<Element>(layout_object.GetNode());
  Animation* animation = generator->GetAnimationIfCompositable(element);

#if EXPENSIVE_DCHECKS_ARE_ON()
  if (animation &&
      CompositeClipPathStatus(element) == CompositedPaintStatus::kComposited) {
    CHECK(animation->HasActiveAnimationsOnCompositor() ||
          animation->CheckCanStartAnimationOnCompositor(nullptr) ==
              CompositorAnimations::kNoFailure);
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  return animation;
}

bool ClipPathClipper::HasCompositeClipPathAnimation(
    const LayoutObject& layout_object,
    CompositedStateResolutionType type) {
  if (!RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    return false;
  }

  if (layout_object.GetDocument().Lifecycle().GetState() <
      DocumentLifecycle::kInPrePaint) {
    DCHECK(false) << "HasCompositeClipPathAnimation result not defined before"
                  << " pre-paint.";
    base::debug::DumpWithoutCrashing();
  }

  CompositedPaintStatus status =
      CompositeClipPathStatus(layout_object.GetNode());
  switch (status) {
    case CompositedPaintStatus::kComposited:
      CHECK(GetClipPathAnimation(layout_object));
      return true;
    case CompositedPaintStatus::kNoAnimation:
    case CompositedPaintStatus::kNotComposited:
      return false;
    case CompositedPaintStatus::kNeedsRepaint:
      // TODO(crbug.com/374656290): Replace this with CHECK_LE when this bug is
      // known to be resolved.
      if (layout_object.GetDocument().Lifecycle().GetState() >=
              DocumentLifecycle::kInPaint &&
          (CompositedStateResolutionType::kReadCache != type)) {
        DCHECK(false)
            << "Clip-path status must not be resolved after pre-paint";
        base::debug::DumpWithoutCrashing();
      }

      Animation* animation = GetClipPathAnimation(layout_object);
      // TODO(crbug.com/454365238): Fallback point for cc clip-path animations,
      // should be annotated with a histogram.
      if (animation &&
          AdjustClipPathStatusForCompositingFailureReasons(
              layout_object, *animation,
              type == CompositedStateResolutionType::kFullResolve)) {
        return true;
      }

      SetCompositeClipPathStatus(layout_object.GetNode(),
                                 CompositedPaintStatus::kNotComposited);
      return false;
  }
}

bool ClipPathClipper::ClipPathStatusResolved(
    const LayoutObject& layout_object) {
  if (!RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    // Paradoxically, we return true here, as if the feature is disabled we
    // know for sure that the status is not composited.
    return true;
  }

  CompositedPaintStatus status =
      CompositeClipPathStatus(layout_object.GetNode());

  return status != CompositedPaintStatus::kNeedsRepaint;
}
void ClipPathClipper::FallbackClipPathAnimationIfNecessary(
    const LayoutObject& layout_object,
    bool is_in_block_fragmentation) {
  if (!RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    return;
  }

  if (layout_object.GetDocument().Lifecycle().GetState() >=
      DocumentLifecycle::kInPaint) {
    DCHECK(false) << "FallbackClipPathAnimationIfNecessary must not be called "
                     "after pre-paint.";
    base::debug::DumpWithoutCrashing();
  }

  if (ClipPathAnimationShouldFallback(layout_object,
                                      is_in_block_fragmentation)) {
    SetCompositeClipPathStatus(layout_object.GetNode(),
                               CompositedPaintStatus::kNotComposited);
  }
}

// TODO(crbug.com/454365238): Fallback point for cc clip-path animations, should
// be annotated with a histogram.
void ClipPathClipper::FallbackClipPathAnimationDueToAbsentBounds(
    const LayoutObject& layout_object) {
  SetCompositeClipPathStatus(layout_object.GetNode(),
                             CompositedPaintStatus::kNotComposited);
}

gfx::RectF ClipPathClipper::CalcLocalReferenceBox(
    const LayoutObject& object,
    const ClipPathOperation::OperationType clip_path_operation,
    GeometryBox geometry_box) {
  if (object.IsSVGChild()) {
    // Use the object bounding box for url() references.
    if (clip_path_operation == ClipPathOperation::kReference) {
      geometry_box = GeometryBox::kFillBox;
    }
    gfx::RectF unzoomed_reference_box = SVGResources::ReferenceBoxForEffects(
        object, geometry_box, SVGResources::ForeignObjectQuirk::kDisabled);
    if (ClipPathClipper::UsesZoomedReferenceBox(object)) {
      return gfx::ScaleRect(unzoomed_reference_box,
                            object.StyleRef().EffectiveZoom());
    }
    return unzoomed_reference_box;
  }

  const auto& box = To<LayoutBoxModelObject>(object);
  PhysicalRect reference_box = BorderBoxRect(box);
  reference_box.Expand(
      GeometryBoxUtils::ReferenceBoxBorderBoxOutsets(geometry_box, box));
  return gfx::RectF(reference_box);
}

gfx::RectF ClipPathClipper::LocalReferenceBox(const LayoutObject& object) {
  ClipPathOperation* clip_path = object.StyleRef().ClipPath();
  GeometryBox geometry_box = GeometryBox::kBorderBox;

  if (const auto* shape = DynamicTo<ShapeClipPathOperation>(clip_path)) {
    geometry_box = shape->GetGeometryBox();
  } else if (const auto* box =
                 DynamicTo<GeometryBoxClipPathOperation>(clip_path)) {
    geometry_box = box->GetGeometryBox();
  }

  return CalcLocalReferenceBox(object, clip_path->GetType(), geometry_box);
}

std::optional<gfx::RectF> ClipPathClipper::LocalClipPathBoundingBox(
    const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().HasClipPath())
    return std::nullopt;

  gfx::RectF reference_box = LocalReferenceBox(object);
  ClipPathOperation& clip_path = *object.StyleRef().ClipPath();
  if (clip_path.GetType() == ClipPathOperation::kShape) {
    auto zoom = object.StyleRef().EffectiveZoom();

    bool uses_zoomed_reference_box = UsesZoomedReferenceBox(object);
    const gfx::RectF adjusted_reference_box =
        uses_zoomed_reference_box ? reference_box
                                  : gfx::ScaleRect(reference_box, zoom);
    const float path_scale = uses_zoomed_reference_box ? 1.f : 1.f / zoom;

    auto& shape = To<ShapeClipPathOperation>(clip_path);
    gfx::RectF bounding_box =
        shape.GetPath(adjusted_reference_box, zoom, path_scale).BoundingRect();

    bounding_box.Intersect(gfx::RectF(InfiniteIntRect()));
    return bounding_box;
  }

  if (IsA<GeometryBoxClipPathOperation>(clip_path)) {
    reference_box.Intersect(gfx::RectF(InfiniteIntRect()));
    return reference_box;
  }

  const auto& reference_clip = To<ReferenceClipPathOperation>(clip_path);
  if (reference_clip.IsLoading()) {
    return gfx::RectF();
  }

  LayoutSVGResourceClipper* clipper =
      ResolveElementReference(object, reference_clip);
  if (!clipper)
    return std::nullopt;

  gfx::RectF bounding_box = clipper->ResourceBoundingBox(reference_box);
  if (UsesZoomedReferenceBox(object) &&
      clipper->ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    bounding_box.Scale(object.StyleRef().EffectiveZoom());
    // With kSvgUnitTypeUserspaceonuse, the clip path layout is relative to
    // the current transform space, and the reference box is unused.
    // While SVG object has no concept of paint offset, HTML object's
    // local space is shifted by paint offset.
    if (UsesPaintOffset(object)) {
      bounding_box.Offset(reference_box.OffsetFromOrigin());
    }
  }

  bounding_box.Intersect(gfx::RectF(InfiniteIntRect()));
  return bounding_box;
}

static AffineTransform UserSpaceToClipPathTransform(
    const LayoutSVGResourceClipper& clipper,
    const gfx::RectF& reference_box,
    const LayoutObject& reference_box_object) {
  AffineTransform clip_path_transform;
  if (ClipPathClipper::UsesZoomedReferenceBox(reference_box_object)) {
    // If the <clipPath> is using "userspace on use" units, then the origin of
    // the coordinate system is the top-left of the reference box.
    if (clipper.ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
      clip_path_transform.Translate(reference_box.x(), reference_box.y());
    }
    clip_path_transform.Scale(reference_box_object.StyleRef().EffectiveZoom());
  }
  return clip_path_transform;
}

static Path GetPathWithObjectZoom(const ShapeClipPathOperation& shape,
                                  const gfx::RectF& reference_box,
                                  const LayoutObject& reference_box_object) {
  bool uses_zoomed_reference_box =
      ClipPathClipper::UsesZoomedReferenceBox(reference_box_object);
  float zoom = reference_box_object.StyleRef().EffectiveZoom();
  const gfx::RectF zoomed_reference_box =
      uses_zoomed_reference_box ? reference_box
                                : gfx::ScaleRect(reference_box, zoom);
  const float path_scale = uses_zoomed_reference_box ? 1.f : 1.f / zoom;

  return shape.GetPath(zoomed_reference_box, zoom, path_scale);
}

bool ClipPathClipper::HitTest(const LayoutObject& object,
                              const HitTestLocation& location) {
  return HitTest(object, LocalReferenceBox(object), object, location);
}

bool ClipPathClipper::HitTest(const LayoutObject& clip_path_owner,
                              const gfx::RectF& reference_box,
                              const LayoutObject& reference_box_object,
                              const HitTestLocation& location) {
  const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
  if (const auto* shape = DynamicTo<ShapeClipPathOperation>(clip_path)) {
    const Path path =
        GetPathWithObjectZoom(*shape, reference_box, reference_box_object);
    return location.Intersects(path);
  }
  if (const auto* box = DynamicTo<GeometryBoxClipPathOperation>(clip_path)) {
    return location.Intersects(
        RoundedReferenceBox(box->GetGeometryBox(), reference_box_object));
  }
  const auto& reference_clip = To<ReferenceClipPathOperation>(clip_path);
  if (reference_clip.IsLoading()) {
    return false;
  }
  const LayoutSVGResourceClipper* clipper =
      ResolveElementReference(clip_path_owner, reference_clip);
  if (!clipper) {
    return true;
  }
  // Transform the HitTestLocation to the <clipPath>s coordinate space - which
  // is not zoomed. Ditto for the reference box.
  const TransformedHitTestLocation unzoomed_location(
      location, UserSpaceToClipPathTransform(*clipper, reference_box,
                                             reference_box_object));
  const float zoom = reference_box_object.StyleRef().EffectiveZoom();
  const bool uses_zoomed_reference_box =
      UsesZoomedReferenceBox(reference_box_object);
  const gfx::RectF unzoomed_reference_box =
      uses_zoomed_reference_box ? gfx::ScaleRect(reference_box, 1.f / zoom)
                                : reference_box;
  return clipper->HitTestClipContent(unzoomed_reference_box,
                                     reference_box_object, *unzoomed_location);
}

static AffineTransform MaskToContentTransform(
    const LayoutSVGResourceClipper& resource_clipper,
    const gfx::RectF& reference_box,
    const LayoutObject& reference_box_object) {
  AffineTransform mask_to_content;
  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    if (ClipPathClipper::UsesZoomedReferenceBox(reference_box_object)) {
      if (UsesPaintOffset(reference_box_object)) {
        mask_to_content.Translate(reference_box.x(), reference_box.y());
      }
      mask_to_content.Scale(reference_box_object.StyleRef().EffectiveZoom());
    }
  }

  mask_to_content.PreConcat(
      resource_clipper.CalculateClipTransform(reference_box));
  return mask_to_content;
}

std::optional<Path> ClipPathClipper::PathBasedClipInternal(
    const LayoutObject& clip_path_owner,
    const gfx::RectF& reference_box,
    const LayoutObject& reference_box_object,
    const gfx::Vector2dF& clip_offset) {
  const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
  if (const auto* shape = DynamicTo<ShapeClipPathOperation>(clip_path)) {
    Path path =
        GetPathWithObjectZoom(*shape, reference_box, reference_box_object);
    if (!clip_offset.IsZero()) {
      path = PathBuilder(path).Translate(clip_offset).Finalize();
    }
    return path;
  }

  if (const auto* geometry_box_clip =
          DynamicTo<GeometryBoxClipPathOperation>(clip_path)) {
    auto box = RoundedReferenceBox(geometry_box_clip->GetGeometryBox(),
                                   reference_box_object);
    box.Move(clip_offset);
    return box.GetPath();
  }

  const auto& reference_clip = To<ReferenceClipPathOperation>(clip_path);
  if (reference_clip.IsLoading()) {
    return Path();
  }
  LayoutSVGResourceClipper* resource_clipper =
      ResolveElementReference(clip_path_owner, reference_clip);
  if (!resource_clipper) {
    return std::nullopt;
  }
  if (!RuntimeEnabledFeatures::ClipPathNestedRasterOptimizationEnabled()) {
    // If the current clip-path gets clipped itself, we fallback to masking.
    if (resource_clipper->StyleRef().HasClipPath()) {
      return std::nullopt;
    }
  }
  std::optional<Path> path = resource_clipper->AsPath();
  if (!path) {
    return path;
  }
  const auto clip_transform =
      AffineTransform::Translation(clip_offset.x(), clip_offset.y()) *
      MaskToContentTransform(*resource_clipper, reference_box,
                             reference_box_object);
  if (!clip_transform.IsIdentity()) {
    path = PathBuilder(*path).Transform(clip_transform).Finalize();
  }
  if (RuntimeEnabledFeatures::ClipPathNestedRasterOptimizationEnabled()) {
    if (resource_clipper->StyleRef().HasClipPath()) {
      std::optional<Path> nested_clip = PathBasedClipInternal(
          *resource_clipper, reference_box, reference_box_object, clip_offset);
      if (!nested_clip) {
        return std::nullopt;
      }
      // Avoid high-complexities since Skia Path ops can have O(N^2) behavior
      // (skbug.com/350478860).
      // TODO: Consider a different approach to avoid combining paths.
      constexpr int kMaxVerbs = 500;
      if (path->GetSkPath().countVerbs() +
              nested_clip->GetSkPath().countVerbs() >
          kMaxVerbs) {
        return std::nullopt;
      }
      SkPath clipped_path;
      if (!Op(path->GetSkPath(), nested_clip->GetSkPath(), kIntersect_SkPathOp,
              &clipped_path)) {
        return std::nullopt;
      }
      path = Path(clipped_path);
    }
  }
  return path;
}

void ClipPathClipper::PaintClipPathAsMaskImage(
    GraphicsContext& context,
    const LayoutObject& layout_object,
    const DisplayItemClient& display_item_client) {
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  DCHECK(properties);
  DCHECK(properties->ClipPathMask());
  DCHECK(properties->ClipPathMask()->OutputClip());
  PropertyTreeStateOrAlias property_tree_state(
      properties->ClipPathMask()->LocalTransformSpace(),
      *properties->ClipPathMask()->OutputClip(), *properties->ClipPathMask());
  ScopedPaintChunkProperties scoped_properties(
      context.GetPaintController(), property_tree_state, display_item_client,
      DisplayItem::kSVGClip);

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGClip))
    return;

  bool has_cc_clip_path_anim = ClipPathClipper::HasCompositeClipPathAnimation(
      layout_object, CompositedStateResolutionType::kReadCache);
  gfx::Rect clip_area_size =
      gfx::ToEnclosingRect(properties->MaskClip()->PaintClipRect().Rect());

  // If the given mask image rect is infinite, it means that the clip-path
  // animation on this element has clip-path none somewhere inside of it. To
  // prevent unbounded mask images and limit perf degradation in this case, we
  // clip by the cull rect here. Visually, this should be a NOP.
  if (has_cc_clip_path_anim &&
      clip_area_size.size() == InfiniteIntRect().size()) {
    clip_area_size = gfx::ToEnclosingRect(
        gfx::RectF(layout_object.FirstFragment().GetContentsCullRect().Rect()));
  }

  DrawingRecorder recorder(context, display_item_client, DisplayItem::kSVGClip,
                           clip_area_size);
  context.Save();

  // cc-side clip path animations deal with their own translations
  if (UsesPaintOffset(layout_object) && !has_cc_clip_path_anim) {
    PhysicalOffset paint_offset = layout_object.FirstFragment().PaintOffset();
    context.Translate(paint_offset.left, paint_offset.top);
  }

  if (has_cc_clip_path_anim) {
    if (!layout_object.GetFrame()) {
      return;
    }

    // clip_path can potentially be a nullptr instead of being none in certain
    // cases, like if there is a clip-path animation with a delay on an element
    // that is absolutely positioned.
    gfx::RectF reference_box;
    if (layout_object.StyleRef().ClipPath()) {
      reference_box = ClipPathClipper::LocalReferenceBox(layout_object);
    } else {
      // TODO(crbug.com/379052285): these assumptions are currently valid
      // because of value filters. Eventually, these should be removed when
      // proper geometry-box support is added.
      reference_box = CalcLocalReferenceBox(
          layout_object, ClipPathOperation::OperationType::kShape,
          GeometryBox::kBorderBox);
    }

    PaintWorkletBasedClip(context, layout_object, gfx::RectF(clip_area_size),
                          reference_box);

    // TODO(crbug.com/393260698): Use cached animation value rather than
    // re-running checks
    Animation* animation = GetClipPathAnimation(layout_object);
    CHECK(animation) << "Unable to find composited clip path animation";
    animation->OnPaintWorkletImageCreated();
  } else {
    gfx::RectF reference_box = LocalReferenceBox(layout_object);
    bool is_first = true;
    bool rest_of_the_chain_already_appled = false;
    const LayoutObject* current_object = &layout_object;
    while (!rest_of_the_chain_already_appled && current_object) {
      const auto* reference_clip =
          To<ReferenceClipPathOperation>(current_object->StyleRef().ClipPath());
      if (!reference_clip || reference_clip->IsLoading()) {
        break;
      }
      // We wouldn't have reached here if the current clip-path is a shape,
      // because it would have been applied as a path-based clip already.
      LayoutSVGResourceClipper* resource_clipper =
          ResolveElementReference(*current_object, *reference_clip);
      if (!resource_clipper)
        break;

      if (is_first) {
        context.Save();
      } else {
        context.BeginLayer(SkBlendMode::kDstIn);
      }

      if (resource_clipper->StyleRef().HasClipPath()) {
        // Try to apply nested clip-path as path-based clip.
        if (const std::optional<Path>& path =
                PathBasedClipInternal(*resource_clipper, reference_box,
                                      layout_object, gfx::Vector2dF())) {
          context.ClipPath(path->GetSkPath(), kAntiAliased);
          rest_of_the_chain_already_appled = true;
        }
      }
      context.ConcatCTM(MaskToContentTransform(*resource_clipper, reference_box,
                                               layout_object));
      context.DrawRecord(resource_clipper->CreatePaintRecord());

      if (is_first)
        context.Restore();
      else
        context.EndLayer();

      is_first = false;
      current_object = resource_clipper;
    }
  }
  context.Restore();
}

std::optional<Path> ClipPathClipper::PathBasedClip(
    const LayoutObject& clip_path_owner,
    const gfx::Vector2dF& clip_offset) {
  if (ClipPathClipper::HasCompositeClipPathAnimation(
          clip_path_owner, CompositedStateResolutionType::kReadCache)) {
    return std::nullopt;
  }

  return PathBasedClipInternal(clip_path_owner,
                               LocalReferenceBox(clip_path_owner),
                               clip_path_owner, clip_offset);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

namespace {

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

  SECURITY_DCHECK(!resource_clipper->SelfNeedsLayout());
  return resource_clipper;
}

}  // namespace

// Is the reference box (as returned by LocalReferenceBox) for |clip_path_owner|
// zoomed with EffectiveZoom()?
static bool UsesZoomedReferenceBox(const LayoutObject& clip_path_owner) {
  return !clip_path_owner.IsSVGChild() ||
         clip_path_owner.IsSVGForeignObjectIncludingNG();
}

CompositedPaintStatus CompositeClipPathStatus(Node* node) {
  Element* element = DynamicTo<Element>(node);
  if (!element)
    return CompositedPaintStatus::kNotComposited;

  ElementAnimations* element_animations = element->GetElementAnimations();
  DCHECK(element_animations);
  return element_animations->CompositedClipPathStatus();
}

void SetCompositeClipPathStatus(Node* node, bool is_compositable) {
  Element* element = DynamicTo<Element>(node);
  if (!element)
    return;

  ElementAnimations* element_animations = element->GetElementAnimations();
  DCHECK(element_animations || !is_compositable);
  if (element_animations) {
    element_animations->SetCompositedClipPathStatus(
        is_compositable ? CompositedPaintStatus::kComposited
                        : CompositedPaintStatus::kNotComposited);
  }
}

static bool HasCompositeClipPathAnimation(const LayoutObject& layout_object) {
  if (!RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled() ||
      !layout_object.StyleRef().HasCurrentClipPathAnimation())
    return false;

  CompositedPaintStatus status =
      CompositeClipPathStatus(layout_object.GetNode());

  if (status == CompositedPaintStatus::kComposited) {
    return true;
  } else if (status == CompositedPaintStatus::kNotComposited) {
    return false;
  }

  ClipPathPaintImageGenerator* generator =
      layout_object.GetFrame()->GetClipPathPaintImageGenerator();
  // TODO(crbug.com/686074): The generator may be null in tests.
  // Fix and remove this test-only branch.
  if (!generator) {
    SetCompositeClipPathStatus(layout_object.GetNode(), false);
    return false;
  }

  const Element* element = To<Element>(layout_object.GetNode());
  const Animation* animation = generator->GetAnimationIfCompositable(element);

  bool has_compositable_clip_path_animation =
      animation && (animation->CheckCanStartAnimationOnCompositor(nullptr) ==
                    CompositorAnimations::kNoFailure);
  SetCompositeClipPathStatus(layout_object.GetNode(),
                             has_compositable_clip_path_animation);
  return has_compositable_clip_path_animation;
}

static void PaintWorkletBasedClip(GraphicsContext& context,
                                  const LayoutObject& clip_path_owner,
                                  const gfx::RectF& reference_box,
                                  bool uses_zoomed_reference_box) {
  DCHECK(HasCompositeClipPathAnimation(clip_path_owner));
  DCHECK_EQ(clip_path_owner.StyleRef().ClipPath()->GetType(),
            ClipPathOperation::kShape);

  float zoom = uses_zoomed_reference_box
                   ? clip_path_owner.StyleRef().EffectiveZoom()
                   : 1;
  ClipPathPaintImageGenerator* generator =
      clip_path_owner.GetFrame()->GetClipPathPaintImageGenerator();

  // The bounding rect of the clip-path animation, relative to the layout
  // object.
  absl::optional<gfx::RectF> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(clip_path_owner);
  DCHECK(bounding_box);

  // Pixel snap bounding rect to allow for the proper painting of partially
  // opaque pixels
  *bounding_box = gfx::RectF(gfx::ToEnclosingRect(*bounding_box));

  // The mask image should be the same size as the bounding rect, but will have
  // an origin of 0,0 as it has its own coordinate space.
  gfx::RectF src_rect = gfx::RectF(bounding_box.value().size());
  gfx::RectF dst_rect = bounding_box.value();

  scoped_refptr<Image> paint_worklet_image = generator->Paint(
      zoom,
      /* Translate the reference box such that it is relative to the origin of
         the mask image, and not the origin of the layout object. This ensures
         the clip path remains within the bounds of the mask image and has the
         correct translation. */
      gfx::RectF(reference_box.origin() - dst_rect.origin().OffsetFromOrigin(),
                 reference_box.size()),

      dst_rect.size(), *clip_path_owner.GetNode());
  // Dark mode should always be disabled for clip mask.
  context.DrawImage(*paint_worklet_image, Image::kSyncDecode,
                    ImageAutoDarkMode::Disabled(), ImagePaintTimingInfo(),
                    dst_rect, &src_rect, SkBlendMode::kSrcOver,
                    kRespectImageOrientation);
}

gfx::RectF ClipPathClipper::LocalReferenceBox(const LayoutObject& object) {
  if (object.IsSVGChild())
    return SVGResources::ReferenceBoxForEffects(object);

  if (object.IsBox())
    return gfx::RectF(To<LayoutBox>(object).BorderBoxRect());

  return gfx::RectF(To<LayoutInline>(object).ReferenceBoxForClipPath());
}

absl::optional<gfx::RectF> ClipPathClipper::LocalClipPathBoundingBox(
    const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().HasClipPath())
    return absl::nullopt;

  gfx::RectF reference_box = LocalReferenceBox(object);
  ClipPathOperation& clip_path = *object.StyleRef().ClipPath();
  if (clip_path.GetType() == ClipPathOperation::kShape) {
    auto zoom = object.StyleRef().EffectiveZoom();

    bool uses_zoomed_reference_box = UsesZoomedReferenceBox(object);
    gfx::RectF adjusted_reference_box =
        uses_zoomed_reference_box ? reference_box
                                  : gfx::ScaleRect(reference_box, zoom);

    gfx::RectF bounding_box;
    if (HasCompositeClipPathAnimation(object)) {
      // For composite clip path animations, the bounding rect needs to contain
      // the *entire* animation, or the animation may be clipped.
      ClipPathPaintImageGenerator* generator =
          object.GetFrame()->GetClipPathPaintImageGenerator();
      bounding_box = generator->ClipAreaRect(*object.GetNode(),
                                             adjusted_reference_box, zoom);
    } else {
      auto& shape = To<ShapeClipPathOperation>(clip_path);
      bounding_box = shape.GetPath(adjusted_reference_box, zoom).BoundingRect();
    }

    if (!uses_zoomed_reference_box)
      bounding_box = gfx::ScaleRect(bounding_box, 1.f / zoom);
    bounding_box.Intersect(gfx::RectF(LayoutRect::InfiniteIntRect()));
    return bounding_box;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::kReference);
  LayoutSVGResourceClipper* clipper = ResolveElementReference(
      object, To<ReferenceClipPathOperation>(clip_path));
  if (!clipper)
    return absl::nullopt;

  gfx::RectF bounding_box = clipper->ResourceBoundingBox(reference_box);
  if (UsesZoomedReferenceBox(object) &&
      clipper->ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    bounding_box.Scale(clipper->StyleRef().EffectiveZoom());
    // With kSvgUnitTypeUserspaceonuse, the clip path layout is relative to
    // the current transform space, and the reference box is unused.
    // While SVG object has no concept of paint offset, HTML object's
    // local space is shifted by paint offset.
    bounding_box.Offset(reference_box.OffsetFromOrigin());
  }
  bounding_box.Intersect(gfx::RectF(LayoutRect::InfiniteIntRect()));
  return bounding_box;
}

static AffineTransform MaskToContentTransform(
    const LayoutSVGResourceClipper& resource_clipper,
    bool uses_zoomed_reference_box,
    const gfx::RectF& reference_box) {
  AffineTransform mask_to_content;
  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    if (uses_zoomed_reference_box) {
      mask_to_content.Translate(reference_box.x(), reference_box.y());
      mask_to_content.Scale(resource_clipper.StyleRef().EffectiveZoom());
    }
  }

  mask_to_content.PreConcat(
      resource_clipper.CalculateClipTransform(reference_box));
  return mask_to_content;
}

static absl::optional<Path> PathBasedClipInternal(
    const LayoutObject& clip_path_owner,
    bool uses_zoomed_reference_box,
    const gfx::RectF& reference_box) {
  const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
  if (const auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(clip_path)) {
    LayoutSVGResourceClipper* resource_clipper =
        ResolveElementReference(clip_path_owner, *reference_clip);
    if (!resource_clipper)
      return absl::nullopt;
    absl::optional<Path> path = resource_clipper->AsPath();
    if (!path)
      return path;
    path->Transform(MaskToContentTransform(
        *resource_clipper, uses_zoomed_reference_box, reference_box));
    return path;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::kShape);
  auto zoom = clip_path_owner.StyleRef().EffectiveZoom();
  auto& shape = To<ShapeClipPathOperation>(clip_path);
  if (uses_zoomed_reference_box)
    return shape.GetPath(reference_box, zoom);
  return shape.GetPath(gfx::ScaleRect(reference_box, zoom), zoom)
      .Transform(AffineTransform::MakeScale(1.f / zoom));
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

  DrawingRecorder recorder(
      context, display_item_client, DisplayItem::kSVGClip,
      gfx::ToEnclosingRect(properties->MaskClip()->PaintClipRect().Rect()));
  context.Save();
  PhysicalOffset paint_offset = layout_object.FirstFragment().PaintOffset();
  context.Translate(paint_offset.left, paint_offset.top);

  bool uses_zoomed_reference_box = UsesZoomedReferenceBox(layout_object);
  gfx::RectF reference_box = LocalReferenceBox(layout_object);

  if (HasCompositeClipPathAnimation(layout_object)) {
    if (!layout_object.GetFrame())
      return;

    PaintWorkletBasedClip(context, layout_object, reference_box,
                          uses_zoomed_reference_box);
  } else {
    bool is_first = true;
    bool rest_of_the_chain_already_appled = false;
    const LayoutObject* current_object = &layout_object;
    while (!rest_of_the_chain_already_appled && current_object) {
      const ClipPathOperation* clip_path =
          current_object->StyleRef().ClipPath();
      if (!clip_path)
        break;
      // We wouldn't have reached here if the current clip-path is a shape,
      // because it would have been applied as a path-based clip already.
      LayoutSVGResourceClipper* resource_clipper = ResolveElementReference(
          *current_object, To<ReferenceClipPathOperation>(*clip_path));
      if (!resource_clipper)
        break;

      if (is_first) {
        context.Save();
      } else {
        context.BeginLayer(SkBlendMode::kDstIn);
      }

      if (resource_clipper->StyleRef().HasClipPath()) {
        // Try to apply nested clip-path as path-based clip.
        if (const absl::optional<Path>& path = PathBasedClipInternal(
                *resource_clipper, uses_zoomed_reference_box, reference_box)) {
          context.ClipPath(path->GetSkPath(), kAntiAliased);
          rest_of_the_chain_already_appled = true;
        }
      }
      context.ConcatCTM(MaskToContentTransform(
          *resource_clipper, uses_zoomed_reference_box, reference_box));
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

bool ClipPathClipper::ShouldUseMaskBasedClip(const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().HasClipPath())
    return false;
  if (HasCompositeClipPathAnimation(object))
    return true;
  const auto* reference_clip =
      DynamicTo<ReferenceClipPathOperation>(object.StyleRef().ClipPath());
  if (!reference_clip)
    return false;
  LayoutSVGResourceClipper* resource_clipper =
      ResolveElementReference(object, *reference_clip);
  if (!resource_clipper)
    return false;
  return !resource_clipper->AsPath();
}

absl::optional<Path> ClipPathClipper::PathBasedClip(
    const LayoutObject& clip_path_owner,
    const bool is_in_block_fragmentation) {
  // TODO(crbug.com/1248622): Currently HasCompositeClipPathAnimation is called
  // multiple times, which is not efficient. Cache
  // HasCompositeClipPathAnimation value as part of fragment_data, similarly to
  // FragmentData::ClipPathPath().

  // If not all the fragments of this layout object have been populated yet, it
  // will be impossible to tell if a composited clip path animation is possible
  // or not based only on the layout object. Exclude the possibility if we're
  // fragmented.
  if (is_in_block_fragmentation)
    SetCompositeClipPathStatus(clip_path_owner.GetNode(), false);
  else if (HasCompositeClipPathAnimation(clip_path_owner))
    return absl::nullopt;

  return PathBasedClipInternal(clip_path_owner,
                               UsesZoomedReferenceBox(clip_path_owner),
                               LocalReferenceBox(clip_path_owner));
}

}  // namespace blink

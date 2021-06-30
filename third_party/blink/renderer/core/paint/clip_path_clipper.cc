// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"

#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

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
  if (resource_clipper) {
    SECURITY_DCHECK(!resource_clipper->NeedsLayout());
    resource_clipper->ClearInvalidationMask();
  }
  return resource_clipper;
}

}  // namespace

// Is the reference box (as returned by LocalReferenceBox) for |clip_path_owner|
// zoomed with EffectiveZoom()?
static bool UsesZoomedReferenceBox(const LayoutObject& clip_path_owner) {
  return !clip_path_owner.IsSVGChild() || clip_path_owner.IsSVGForeignObject();
}

static void PaintWorkletBasedClip(GraphicsContext& context,
                                  const LayoutObject& clip_path_owner,
                                  const FloatRect& reference_box,
                                  bool uses_zoomed_reference_box) {
  DCHECK(RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled());
  DCHECK_EQ(clip_path_owner.StyleRef().ClipPath()->GetType(),
            ClipPathOperation::SHAPE);

  float zoom = uses_zoomed_reference_box
                   ? clip_path_owner.StyleRef().EffectiveZoom()
                   : 1;
  ClipPathPaintImageGenerator* generator =
      clip_path_owner.GetFrame()->GetClipPathPaintImageGenerator();

  scoped_refptr<Image> paint_worklet_image =
      generator->Paint(zoom, reference_box, *clip_path_owner.GetNode());

  absl::optional<FloatRect> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(clip_path_owner);
  DCHECK(bounding_box);
  FloatRect src_rect(bounding_box.value());
  context.DrawImage(paint_worklet_image.get(), Image::kSyncDecode, src_rect,
                    &src_rect, clip_path_owner.StyleRef().DisableForceDark(),
                    SkBlendMode::kSrcOver, kRespectImageOrientation);
}

FloatRect ClipPathClipper::LocalReferenceBox(const LayoutObject& object) {
  if (object.IsSVGChild())
    return SVGResources::ReferenceBoxForEffects(object);

  if (object.IsBox())
    return FloatRect(To<LayoutBox>(object).BorderBoxRect());

  return FloatRect(To<LayoutInline>(object).ReferenceBoxForClipPath());
}

absl::optional<FloatRect> ClipPathClipper::LocalClipPathBoundingBox(
    const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().HasClipPath())
    return absl::nullopt;

  FloatRect reference_box = LocalReferenceBox(object);
  ClipPathOperation& clip_path = *object.StyleRef().ClipPath();
  if (clip_path.GetType() == ClipPathOperation::SHAPE) {
    auto zoom =
        UsesZoomedReferenceBox(object) ? object.StyleRef().EffectiveZoom() : 1;
    auto& shape = To<ShapeClipPathOperation>(clip_path);
    FloatRect bounding_box = shape.GetPath(reference_box, zoom).BoundingRect();
    bounding_box.Intersect(LayoutRect::InfiniteIntRect());
    return bounding_box;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
  LayoutSVGResourceClipper* clipper = ResolveElementReference(
      object, To<ReferenceClipPathOperation>(clip_path));
  if (!clipper)
    return absl::nullopt;

  FloatRect bounding_box = clipper->ResourceBoundingBox(reference_box);
  if (UsesZoomedReferenceBox(object) &&
      clipper->ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    bounding_box.Scale(clipper->StyleRef().EffectiveZoom());
    // With kSvgUnitTypeUserspaceonuse, the clip path layout is relative to
    // the current transform space, and the reference box is unused.
    // While SVG object has no concept of paint offset, HTML object's
    // local space is shifted by paint offset.
    bounding_box.MoveBy(reference_box.Location());
  }
  bounding_box.Intersect(LayoutRect::InfiniteIntRect());
  return bounding_box;
}

static AffineTransform MaskToContentTransform(
    const LayoutSVGResourceClipper& resource_clipper,
    bool uses_zoomed_reference_box,
    const FloatRect& reference_box) {
  AffineTransform mask_to_content;
  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    if (uses_zoomed_reference_box) {
      mask_to_content.Translate(reference_box.X(), reference_box.Y());
      mask_to_content.Scale(resource_clipper.StyleRef().EffectiveZoom());
    }
  }

  mask_to_content.Multiply(
      resource_clipper.CalculateClipTransform(reference_box));
  return mask_to_content;
}

static absl::optional<Path> PathBasedClipInternal(
    const LayoutObject& clip_path_owner,
    bool uses_zoomed_reference_box,
    const FloatRect& reference_box) {
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

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::SHAPE);
  auto& shape = To<ShapeClipPathOperation>(clip_path);
  float zoom = uses_zoomed_reference_box
                   ? clip_path_owner.StyleRef().EffectiveZoom()
                   : 1;
  return shape.GetPath(reference_box, zoom);
}

void ClipPathClipper::PaintClipPathAsMaskImage(
    GraphicsContext& context,
    const LayoutObject& layout_object,
    const DisplayItemClient& display_item_client,
    const PhysicalOffset& paint_offset) {
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  DCHECK(properties);
  DCHECK(properties->MaskClip());
  DCHECK(properties->ClipPathMask());
  PropertyTreeStateOrAlias property_tree_state(
      properties->MaskClip()->LocalTransformSpace(), *properties->MaskClip(),
      *properties->ClipPathMask());
  ScopedPaintChunkProperties scoped_properties(
      context.GetPaintController(), property_tree_state, display_item_client,
      DisplayItem::kSVGClip);

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGClip))
    return;

  DrawingRecorder recorder(
      context, display_item_client, DisplayItem::kSVGClip,
      EnclosingIntRect(properties->MaskClip()->UnsnappedClipRect().Rect()));
  context.Save();
  context.Translate(paint_offset.left, paint_offset.top);

  bool uses_zoomed_reference_box = UsesZoomedReferenceBox(layout_object);
  FloatRect reference_box = LocalReferenceBox(layout_object);
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled() &&
      layout_object.StyleRef().ClipPath()->GetType() ==
          ClipPathOperation::SHAPE) {
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

      if (is_first)
        context.Save();
      else
        context.BeginLayer(1.f, SkBlendMode::kDstIn);

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
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled() &&
      object.StyleRef().ClipPath()->GetType() == ClipPathOperation::SHAPE)
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
    const LayoutObject& clip_path_owner) {
  if (RuntimeEnabledFeatures::CompositeClipPathAnimationEnabled()) {
    const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
    if (clip_path.GetType() == ClipPathOperation::SHAPE)
      return absl::nullopt;
  }
  return PathBasedClipInternal(clip_path_owner,
                               UsesZoomedReferenceBox(clip_path_owner),
                               LocalReferenceBox(clip_path_owner));
}

}  // namespace blink

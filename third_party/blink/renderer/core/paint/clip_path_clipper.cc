// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
 public:
  void Lock(LayoutSVGResourceClipper& clipper) {
    DCHECK(!clipper.HasCycle());
    clipper.BeginClipExpansion();
    clippers_.push_back(&clipper);
  }
  ~SVGClipExpansionCycleHelper() {
    for (auto* clipper : clippers_)
      clipper->EndClipExpansion();
  }

 private:
  Vector<LayoutSVGResourceClipper*, 1> clippers_;
};

LayoutSVGResourceClipper* ResolveElementReference(
    const LayoutObject& layout_object,
    const ReferenceClipPathOperation& reference_clip_path_operation) {
  if (layout_object.IsSVGChild()) {
    // The reference will have been resolved in
    // SVGResources::buildResources, so we can just use the LayoutObject's
    // SVGResources.
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(layout_object);
    return resources ? resources->Clipper() : nullptr;
  }
  // TODO(fs): Doesn't work with external SVG references (crbug.com/109212.)
  SVGResource* resource = reference_clip_path_operation.Resource();
  LayoutSVGResourceContainer* container =
      resource ? resource->ResourceContainer() : nullptr;
  if (!container || container->ResourceType() != kClipperResourceType)
    return nullptr;
  return ToLayoutSVGResourceClipper(container);
}

}  // namespace

FloatRect ClipPathClipper::LocalReferenceBox(const LayoutObject& object) {
  if (object.IsSVGChild())
    return object.ObjectBoundingBox();

  if (object.IsBox())
    return FloatRect(ToLayoutBox(object).BorderBoxRect());

  SECURITY_DCHECK(object.IsLayoutInline());
  return FloatRect(ToLayoutInline(object).ReferenceBoxForClipPath());
}

base::Optional<FloatRect> ClipPathClipper::LocalClipPathBoundingBox(
    const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().ClipPath())
    return base::nullopt;

  FloatRect reference_box = LocalReferenceBox(object);
  ClipPathOperation& clip_path = *object.StyleRef().ClipPath();
  if (clip_path.GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& shape = To<ShapeClipPathOperation>(clip_path);
    if (!shape.IsValid())
      return base::nullopt;
    FloatRect bounding_box = shape.GetPath(reference_box).BoundingRect();
    bounding_box.Intersect(LayoutRect::InfiniteIntRect());
    return bounding_box;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
  LayoutSVGResourceClipper* clipper = ResolveElementReference(
      object, To<ReferenceClipPathOperation>(clip_path));
  if (!clipper)
    return base::nullopt;

  FloatRect bounding_box = clipper->ResourceBoundingBox(reference_box);
  if (!object.IsSVGChild() &&
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

// Note: Return resolved LayoutSVGResourceClipper for caller's convenience,
// if the clip path is a reference to SVG.
static bool IsClipPathOperationValid(
    const ClipPathOperation& clip_path,
    const LayoutObject& search_scope,
    LayoutSVGResourceClipper*& resource_clipper) {
  if (clip_path.GetType() == ClipPathOperation::SHAPE) {
    if (!To<ShapeClipPathOperation>(clip_path).IsValid())
      return false;
  } else {
    DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
    resource_clipper = ResolveElementReference(
        search_scope, To<ReferenceClipPathOperation>(clip_path));
    if (!resource_clipper)
      return false;
    SECURITY_DCHECK(!resource_clipper->NeedsLayout());
    resource_clipper->ClearInvalidationMask();
    if (resource_clipper->HasCycle())
      return false;
  }
  return true;
}

ClipPathClipper::ClipPathClipper(GraphicsContext& context,
                                 const LayoutObject& layout_object,
                                 const PhysicalOffset& paint_offset)
    : context_(context),
      layout_object_(layout_object),
      paint_offset_(paint_offset) {
  DCHECK(layout_object.StyleRef().ClipPath());
}

static AffineTransform MaskToContentTransform(
    const LayoutSVGResourceClipper& resource_clipper,
    bool is_svg_child,
    const FloatRect& reference_box) {
  AffineTransform mask_to_content;
  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    if (!is_svg_child) {
      mask_to_content.Translate(reference_box.X(), reference_box.Y());
      mask_to_content.Scale(resource_clipper.StyleRef().EffectiveZoom());
    }
  }

  mask_to_content.Multiply(
      resource_clipper.CalculateClipTransform(reference_box));
  return mask_to_content;
}

ClipPathClipper::~ClipPathClipper() {
  const auto* properties = layout_object_.FirstFragment().PaintProperties();
  if (!properties || !properties->ClipPath())
    return;
  ScopedPaintChunkProperties scoped_properties(
      context_.GetPaintController(),
      layout_object_.FirstFragment().ClipPathProperties(), layout_object_,
      DisplayItem::kSVGClip);

  bool is_svg_child = layout_object_.IsSVGChild();
  FloatRect reference_box = LocalReferenceBox(layout_object_);

  if (DrawingRecorder::UseCachedDrawingIfPossible(context_, layout_object_,
                                                  DisplayItem::kSVGClip))
    return;
  DrawingRecorder recorder(context_, layout_object_, DisplayItem::kSVGClip);
  context_.Save();
  context_.Translate(paint_offset_.left, paint_offset_.top);

  SVGClipExpansionCycleHelper locks;
  bool is_first = true;
  bool rest_of_the_chain_already_appled = false;
  const LayoutObject* current_object = &layout_object_;
  while (!rest_of_the_chain_already_appled && current_object) {
    const ClipPathOperation* clip_path = current_object->StyleRef().ClipPath();
    if (!clip_path)
      break;
    LayoutSVGResourceClipper* resource_clipper = nullptr;
    if (!IsClipPathOperationValid(*clip_path, *current_object,
                                  resource_clipper))
      break;

    if (is_first)
      context_.Save();
    else
      context_.BeginLayer(1.f, SkBlendMode::kDstIn);

    // We wouldn't have reached here if the current clip-path is a shape,
    // because it would have been applied as path-based clip already.
    DCHECK(resource_clipper);
    DCHECK_EQ(clip_path->GetType(), ClipPathOperation::REFERENCE);
    locks.Lock(*resource_clipper);
    if (resource_clipper->StyleRef().ClipPath()) {
      // Try to apply nested clip-path as path-based clip.
      bool unused;
      if (base::Optional<Path> path = PathBasedClip(
              *resource_clipper, is_svg_child, reference_box, unused)) {
        context_.ClipPath(path->GetSkPath(), kAntiAliased);
        rest_of_the_chain_already_appled = true;
      }
    }
    context_.ConcatCTM(
        MaskToContentTransform(*resource_clipper, is_svg_child, reference_box));
    context_.DrawRecord(resource_clipper->CreatePaintRecord());

    if (is_first)
      context_.Restore();
    else
      context_.EndLayer();

    is_first = false;
    current_object = resource_clipper;
  }
  context_.Restore();
}

base::Optional<Path> ClipPathClipper::PathBasedClip(
    const LayoutObject& clip_path_owner,
    bool is_svg_child,
    const FloatRect& reference_box,
    bool& is_valid) {
  const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
  LayoutSVGResourceClipper* resource_clipper = nullptr;
  is_valid =
      IsClipPathOperationValid(clip_path, clip_path_owner, resource_clipper);
  if (!is_valid)
    return base::nullopt;

  if (resource_clipper) {
    DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
    base::Optional<Path> path = resource_clipper->AsPath();
    if (!path)
      return path;
    path->Transform(
        MaskToContentTransform(*resource_clipper, is_svg_child, reference_box));
    return path;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::SHAPE);
  auto& shape = To<ShapeClipPathOperation>(clip_path);
  return base::Optional<Path>(shape.GetPath(reference_box));
}

}  // namespace blink

/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/svg_root_painter.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

LayoutSVGRoot::LayoutSVGRoot(SVGElement* node)
    : LayoutReplaced(node),
      needs_transform_update_(true),
      has_non_isolated_blending_descendants_(false),
      has_non_isolated_blending_descendants_dirty_(false) {}

LayoutSVGRoot::~LayoutSVGRoot() = default;

void LayoutSVGRoot::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  visitor->Trace(text_set_);
  LayoutReplaced::Trace(visitor);
}

void LayoutSVGRoot::UnscaledIntrinsicSizingInfo(
    const SVGRect* override_viewbox,
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  // https://www.w3.org/TR/SVG/coords.html#IntrinsicSizing

  auto* svg = To<SVGSVGElement>(GetNode());
  DCHECK(svg);

  std::optional<float> intrinsic_width = svg->IntrinsicWidth();
  std::optional<float> intrinsic_height = svg->IntrinsicHeight();
  intrinsic_sizing_info.size =
      gfx::SizeF(intrinsic_width.value_or(0), intrinsic_height.value_or(0));
  intrinsic_sizing_info.has_width = intrinsic_width.has_value();
  intrinsic_sizing_info.has_height = intrinsic_height.has_value();

  if (!intrinsic_sizing_info.size.IsEmpty()) {
    intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
  } else {
    const SVGRect& view_box =
        override_viewbox ? *override_viewbox : svg->CurrentViewBox();
    const gfx::SizeF view_box_size = view_box.Rect().size();
    if (!view_box_size.IsEmpty()) {
      // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
      intrinsic_sizing_info.aspect_ratio = view_box_size;
    }
  }
}

void LayoutSVGRoot::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  DCHECK(!ShouldApplySizeContainment());
  UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);

  intrinsic_sizing_info.size.Scale(StyleRef().EffectiveZoom());
}

bool LayoutSVGRoot::IsEmbeddedThroughSVGImage() const {
  NOT_DESTROYED();
  return SVGImage::IsInSVGImage(To<SVGSVGElement>(GetNode()));
}

bool LayoutSVGRoot::IsEmbeddedThroughFrameContainingSVGDocument() const {
  NOT_DESTROYED();
  if (!IsDocumentElement() || !GetNode()) {
    return false;
  }

  LocalFrame* frame = GetNode()->GetDocument().GetFrame();
  if (!frame || !frame->GetDocument()->IsSVGDocument())
    return false;

  if (frame->Owner() && frame->Owner()->IsRemote())
    return true;

  // If our frame has an owner layoutObject, we're embedded through eg.
  // object/embed/iframe, but we only negotiate if we're in an SVG document
  // inside a embedded object (object/embed).
  LayoutObject* owner_layout_object = frame->OwnerLayoutObject();
  return owner_layout_object && owner_layout_object->IsEmbeddedObject();
}

double LayoutSVGRoot::LogicalSizeScaleFactorForPercentageLengths() const {
  NOT_DESTROYED();
  CHECK(IsDocumentElement());
  if (!GetDocument().IsInOutermostMainFrame() ||
      GetDocument().GetLayoutView()->ShouldUsePaginatedLayout()) {
    return 1;
  }
  // This will return the zoom factor which is different from the typical usage
  // of "zoom factor" in blink (e.g., |LocalFrame::LayoutZoomFactor()|) which
  // includes CSS zoom and the device scale factor (if use-zoom-for-dsf is
  // enabled). For this special-case, we only want to include the user's zoom
  // factor, as all other types of zoom should not scale a percentage-sized svg.
  return GetFrame()->GetChromeClient().UserZoomFactor(GetFrame());
}

void LayoutSVGRoot::LayoutRoot(const PhysicalRect& content_rect) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  base::AutoReset<const PhysicalSize*> reset(&new_content_size_,
                                             &content_rect.size, nullptr);

  const PhysicalSize old_content_size = PhysicalContentBoxSize();

  // Whether we have a self-painting layer depends on whether there are
  // compositing descendants (see: |HasCompositingDescendants()| which is called
  // from |PaintLayer::UpdateSelfPaintingLayer()|). We cannot do this update in
  // StyleDidChange because descendants have not yet run StyleDidChange, so we
  // don't know their compositing reasons yet. A layout is scheduled when
  // |HasCompositingDescendants()| changes to ensure this is run.
  if (Layer())
    Layer()->UpdateSelfPaintingLayer();

  // The local-to-border-box transform is a function with the following as
  // input:
  //
  //  * effective zoom
  //  * contentWidth/Height
  //  * viewBox
  //  * border + padding
  //  * currentTranslate
  //  * currentScale
  //
  // Which means that |transformChange| will notice a change to the scale from
  // any of these.
  SVGTransformChange transform_change =
      BuildLocalToBorderBoxTransform(content_rect);

  // The scale factor from the local-to-border-box transform is all that our
  // scale-dependent descendants care about.
  const bool screen_scale_factor_changed =
      transform_change == SVGTransformChange::kFull;

  // selfNeedsLayout() will cover changes to one (or more) of viewBox,
  // current{Scale,Translate}, decorations and 'overflow'.
  const bool viewport_may_have_changed =
      SelfNeedsFullLayout() || old_content_size != content_rect.size;

  SVGLayoutInfo layout_info;
  layout_info.scale_factor_changed = screen_scale_factor_changed;
  layout_info.viewport_changed = viewport_may_have_changed;

  const SVGLayoutResult content_result = content_.Layout(layout_info);

  // Boundaries affects the mask clip. (Other resources handled elsewhere.)
  if (content_result.bounds_changed) {
    SetNeedsPaintPropertyUpdate();
  }
  needs_transform_update_ = false;

  // The scale of one or more of the SVG elements may have changed, content
  // (the entire SVG) could have moved or new content may have been exposed, so
  // mark the entire subtree as needing paint invalidation checking.
  if (transform_change != SVGTransformChange::kNone ||
      viewport_may_have_changed) {
    SetSubtreeShouldCheckForPaintInvalidation();
    SetNeedsPaintPropertyUpdate();
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
  }
}

void LayoutSVGRoot::RecalcVisualOverflow() {
  NOT_DESTROYED();
  LayoutReplaced::RecalcVisualOverflow();
  if (!ClipsToContentBox())
    AddContentsVisualOverflow(ComputeContentsVisualOverflow());
}

PhysicalRect LayoutSVGRoot::ComputeContentsVisualOverflow() const {
  NOT_DESTROYED();
  gfx::RectF content_visual_rect = VisualRectInLocalSVGCoordinates();
  content_visual_rect =
      local_to_border_box_transform_.MapRect(content_visual_rect);
  // Condition the visual overflow rect to avoid being clipped/culled
  // out if it is huge. This may sacrifice overflow, but usually only
  // overflow that would never be seen anyway.
  // To condition, we intersect with something that we oftentimes
  // consider to be "infinity".
  return Intersection(PhysicalRect::EnclosingRect(content_visual_rect),
                      PhysicalRect(InfiniteIntRect()));
}

void LayoutSVGRoot::PaintReplaced(const PaintInfo& paint_info,
                                  const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock())
    return;
  SVGRootPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutSVGRoot::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearEffects(*this);
  LayoutReplaced::WillBeDestroyed();
}

bool LayoutSVGRoot::IntrinsicSizeIsFontMetricsDependent() const {
  NOT_DESTROYED();
  const auto& svg = To<SVGSVGElement>(*GetNode());
  return svg.width()->CurrentValue()->IsFontRelative() ||
         svg.height()->CurrentValue()->IsFontRelative();
}

bool LayoutSVGRoot::StyleChangeAffectsIntrinsicSize(
    const ComputedStyle& old_style) const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  // If the writing mode changed from a horizontal mode to a vertical
  // mode, or vice versa, then our intrinsic dimensions will have
  // changed.
  if (old_style.IsHorizontalWritingMode() != style.IsHorizontalWritingMode())
    return true;
  // If our intrinsic dimensions depend on font metrics (by using 'em', 'ex' or
  // any other font-relative unit), any changes to the font may change said
  // dimensions.
  if (IntrinsicSizeIsFontMetricsDependent() &&
      old_style.GetFont() != style.GetFont())
    return true;
  return false;
}

void LayoutSVGRoot::IntrinsicSizingInfoChanged() {
  NOT_DESTROYED();
  SetIntrinsicLogicalWidthsDirty();

  // TODO(fs): Merge with IntrinsicSizeChanged()? (from LayoutReplaced)
  // Ignore changes to intrinsic dimensions if the <svg> is not in an SVG
  // document, or not embedded in a way that supports/allows size negotiation.
  if (!IsEmbeddedThroughFrameContainingSVGDocument())
    return;
  DCHECK(GetFrame()->Owner());
  GetFrame()->Owner()->IntrinsicSizingInfoChanged();
}

void LayoutSVGRoot::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);

  if (old_style && StyleChangeAffectsIntrinsicSize(*old_style))
    IntrinsicSizingInfoChanged();

  SVGResources::UpdateEffects(*this, diff, old_style);

  if (diff.TransformChanged()) {
    for (auto& svg_text : text_set_) {
      svg_text->SetNeedsLayout(layout_invalidation_reason::kStyleChange,
                               kMarkContainerChain);
      svg_text->SetNeedsTextMetricsUpdate();
    }
  }

  if (!Parent())
    return;
  if (diff.HasDifference())
    LayoutSVGResourceContainer::StyleChanged(*this, diff);
}

bool LayoutSVGRoot::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  NOT_DESTROYED();
  return SVGContentContainer::IsChildAllowed(*child);
}

void LayoutSVGRoot::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutReplaced::AddChild(child, before_child);

  bool should_isolate_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (should_isolate_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationRequired);
}

void LayoutSVGRoot::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutReplaced::RemoveChild(child);

  content_.MarkBoundsDirtyFromRemovedChild();

  bool had_non_isolated_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (had_non_isolated_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationNeedsUpdate);
}

bool LayoutSVGRoot::HasNonIsolatedBlendingDescendants() const {
  NOT_DESTROYED();
  if (has_non_isolated_blending_descendants_dirty_) {
    has_non_isolated_blending_descendants_ =
        content_.ComputeHasNonIsolatedBlendingDescendants();
    has_non_isolated_blending_descendants_dirty_ = false;
  }
  return has_non_isolated_blending_descendants_;
}

void LayoutSVGRoot::DescendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  NOT_DESTROYED();
  switch (state) {
    case kDescendantIsolationRequired:
      has_non_isolated_blending_descendants_ = true;
      has_non_isolated_blending_descendants_dirty_ = false;
      break;
    case kDescendantIsolationNeedsUpdate:
      has_non_isolated_blending_descendants_dirty_ = true;
      break;
  }
  SetNeedsPaintPropertyUpdate();
  if (Layer())
    Layer()->SetNeedsCompositingInputsUpdate();
}

void LayoutSVGRoot::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutReplaced::InsertedIntoTree();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
}

void LayoutSVGRoot::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
  LayoutReplaced::WillBeRemovedFromTree();
}

PositionWithAffinity LayoutSVGRoot::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  gfx::PointF absolute_point(point);
  absolute_point =
      local_to_border_box_transform_.Inverse().MapPoint(absolute_point);
  LayoutObject* closest_descendant =
      SVGLayoutSupport::FindClosestLayoutSVGText(this, absolute_point);

  if (!closest_descendant)
    return LayoutReplaced::PositionForPoint(point);

  LayoutObject* layout_object = closest_descendant;
  AffineTransform transform = layout_object->LocalToSVGParentTransform();
  PhysicalOffset location = To<LayoutBox>(layout_object)->PhysicalLocation();
  transform.Translate(location.left, location.top);
  while (layout_object) {
    layout_object = layout_object->Parent();
    if (layout_object->IsSVGRoot())
      break;
    transform = layout_object->LocalToSVGParentTransform() * transform;
  }

  absolute_point = transform.Inverse().MapPoint(absolute_point);

  return closest_descendant->PositionForPoint(
      PhysicalOffset::FromPointFRound(absolute_point));
}

// LayoutBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
SVGTransformChange LayoutSVGRoot::BuildLocalToBorderBoxTransform(
    const PhysicalRect& content_rect) {
  NOT_DESTROYED();
  SVGTransformChangeDetector change_detector(local_to_border_box_transform_);
  auto* svg = To<SVGSVGElement>(GetNode());
  DCHECK(svg);
  float scale = StyleRef().EffectiveZoom();
  gfx::SizeF content_size(content_rect.size.width / scale,
                          content_rect.size.height / scale);
  local_to_border_box_transform_ = svg->ViewBoxToViewTransform(content_size);

  gfx::Vector2dF translate = svg->CurrentTranslate();
  AffineTransform view_to_border_box_transform(
      scale, 0, 0, scale, content_rect.offset.left + translate.x(),
      content_rect.offset.top + translate.y());
  view_to_border_box_transform.Scale(svg->currentScale());
  local_to_border_box_transform_.PostConcat(view_to_border_box_transform);
  return change_detector.ComputeChange(local_to_border_box_transform_);
}

AffineTransform LayoutSVGRoot::LocalToSVGParentTransform() const {
  NOT_DESTROYED();
  PhysicalOffset location = PhysicalLocation();
  return AffineTransform::Translation(RoundToInt(location.left),
                                      RoundToInt(location.top)) *
         local_to_border_box_transform_;
}

gfx::RectF LayoutSVGRoot::ViewBoxRect() const {
  return To<SVGSVGElement>(*GetNode()).CurrentViewBoxRect();
}

gfx::SizeF LayoutSVGRoot::ViewportSize() const {
  const PhysicalSize& viewport_size =
      new_content_size_ ? *new_content_size_ : PhysicalContentBoxSize();
  const float zoom = StyleRef().EffectiveZoom();
  return gfx::SizeF(viewport_size.width / zoom, viewport_size.height / zoom);
}

// This method expects local CSS box coordinates.
// Callers with local SVG viewport coordinates should first apply the
// localToBorderBoxTransform to convert from SVG viewport coordinates to local
// CSS box coordinates.
void LayoutSVGRoot::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                       TransformState& transform_state,
                                       MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  LayoutReplaced::MapLocalToAncestor(ancestor, transform_state, mode);
}

bool LayoutSVGRoot::HitTestChildren(HitTestResult& result,
                                    const HitTestLocation& hit_test_location,
                                    const PhysicalOffset& accumulated_offset,
                                    HitTestPhase phase) {
  NOT_DESTROYED();
  HitTestLocation local_border_box_location(hit_test_location,
                                            -accumulated_offset);
  TransformedHitTestLocation local_location(local_border_box_location,
                                            LocalToBorderBoxTransform());
  if (!local_location) {
    return false;
  }
  return content_.HitTest(result, *local_location, phase);
}

bool LayoutSVGRoot::IsInSelfHitTestingPhase(HitTestPhase phase) const {
  // Only hit-test the root <svg> container during the background
  // phase. (Hit-testing during the foreground phase would make us miss for
  // instance backgrounds of children inside <foreignObject>.)
  return phase == HitTestPhase::kSelfBlockBackground;
}

void LayoutSVGRoot::IntersectChildren(HitTestResult& result,
                                      const HitTestLocation& location) const {
  content_.HitTest(result, location, HitTestPhase::kForeground);
}

void LayoutSVGRoot::AddSvgTextDescendant(LayoutSVGText& svg_text) {
  NOT_DESTROYED();
  DCHECK(!text_set_.Contains(&svg_text));
  text_set_.insert(&svg_text);
}

void LayoutSVGRoot::RemoveSvgTextDescendant(LayoutSVGText& svg_text) {
  NOT_DESTROYED();
  DCHECK(text_set_.Contains(&svg_text));
  text_set_.erase(&svg_text);
}

PaintLayerType LayoutSVGRoot::LayerTypeRequired() const {
  NOT_DESTROYED();
  auto layer_type_required = LayoutReplaced::LayerTypeRequired();
  if (layer_type_required == kNoPaintLayer) {
    // Force a paint layer so the parent layer will know if there are
    // non-isolated descendants with blend mode.
    layer_type_required = kForcedPaintLayer;
  }
  return layer_type_required;
}

OverflowClipAxes LayoutSVGRoot::ComputeOverflowClipAxes() const {
  NOT_DESTROYED();

  // svg document roots are always clipped. When the svg is stand-alone
  // (isDocumentElement() == true) the viewport clipping should always be
  // applied, noting that the window scrollbars should be hidden if
  // overflow=hidden.
  if (IsDocumentElement())
    return kOverflowClipBothAxis;

  // Use the default code-path which computes overflow based on `overflow`,
  // `overflow-clip-margin` and paint containment if all these properties are
  // respected on svg elements similar to other replaced elements.
  if (RespectsCSSOverflow())
    return LayoutReplaced::ComputeOverflowClipAxes();

  // the outermost svg is clipped if auto.
  if (StyleRef().OverflowX() == EOverflow::kHidden ||
      StyleRef().OverflowX() == EOverflow::kAuto ||
      StyleRef().OverflowX() == EOverflow::kScroll ||
      StyleRef().OverflowX() == EOverflow::kClip)
    return kOverflowClipBothAxis;

  return LayoutReplaced::ComputeOverflowClipAxes();
}

}  // namespace blink

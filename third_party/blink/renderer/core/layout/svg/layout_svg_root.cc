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

#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
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
      object_bounding_box_valid_(false),
      is_layout_size_changed_(false),
      did_screen_scale_factor_change_(false),
      needs_boundaries_or_transform_update_(true),
      has_box_decoration_background_(false),
      has_non_isolated_blending_descendants_(false),
      has_non_isolated_blending_descendants_dirty_(false),
      has_descendant_with_compositing_reason_(false),
      has_descendant_with_compositing_reason_dirty_(false) {
  auto* svg = To<SVGSVGElement>(node);
  DCHECK(svg);

  LayoutSize intrinsic_size(svg->IntrinsicWidth(), svg->IntrinsicHeight());
  if (!svg->HasIntrinsicWidth())
    intrinsic_size.SetWidth(LayoutUnit(kDefaultWidth));
  if (!svg->HasIntrinsicHeight())
    intrinsic_size.SetHeight(LayoutUnit(kDefaultHeight));
  SetIntrinsicSize(intrinsic_size);
}

LayoutSVGRoot::~LayoutSVGRoot() = default;

void LayoutSVGRoot::UnscaledIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  // https://www.w3.org/TR/SVG/coords.html#IntrinsicSizing

  auto* svg = To<SVGSVGElement>(GetNode());
  DCHECK(svg);

  intrinsic_sizing_info.size =
      FloatSize(svg->IntrinsicWidth(), svg->IntrinsicHeight());
  intrinsic_sizing_info.has_width = svg->HasIntrinsicWidth();
  intrinsic_sizing_info.has_height = svg->HasIntrinsicHeight();

  if (const base::Optional<IntSize>& aspect_ratio = StyleRef().AspectRatio()) {
    intrinsic_sizing_info.aspect_ratio.SetWidth(aspect_ratio->Width());
    intrinsic_sizing_info.aspect_ratio.SetHeight(aspect_ratio->Height());
  } else if (!intrinsic_sizing_info.size.IsEmpty()) {
    intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
  } else {
    FloatSize view_box_size = svg->viewBox()->CurrentValue()->Value().Size();
    if (!view_box_size.IsEmpty()) {
      // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
      intrinsic_sizing_info.aspect_ratio = view_box_size;
    }
  }

  if (!IsHorizontalWritingMode())
    intrinsic_sizing_info.Transpose();
}

void LayoutSVGRoot::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  DCHECK(!ShouldApplySizeContainment());
  UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);

  intrinsic_sizing_info.size.Scale(StyleRef().EffectiveZoom());
}

bool LayoutSVGRoot::IsEmbeddedThroughSVGImage() const {
  return SVGImage::IsInSVGImage(To<SVGSVGElement>(GetNode()));
}

bool LayoutSVGRoot::IsEmbeddedThroughFrameContainingSVGDocument() const {
  if (!GetNode())
    return false;

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

LayoutUnit LayoutSVGRoot::ComputeReplacedLogicalWidth(
    ShouldComputePreferred should_compute_preferred) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!container_size_.IsEmpty())
    return container_size_.Width();

  if (IsEmbeddedThroughFrameContainingSVGDocument())
    return ContainingBlock()->AvailableLogicalWidth();

  LayoutUnit width =
      LayoutReplaced::ComputeReplacedLogicalWidth(should_compute_preferred);
  if (StyleRef().LogicalWidth().IsPercentOrCalc())
    width *= LogicalSizeScaleFactorForPercentageLengths();
  return width;
}

LayoutUnit LayoutSVGRoot::ComputeReplacedLogicalHeight(
    LayoutUnit estimated_used_width) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!container_size_.IsEmpty())
    return container_size_.Height();

  if (IsEmbeddedThroughFrameContainingSVGDocument())
    return ContainingBlock()->AvailableLogicalHeight(
        kIncludeMarginBorderPadding);

  const Length& logical_height = StyleRef().LogicalHeight();
  if (IsDocumentElement() && logical_height.IsPercentOrCalc()) {
    LayoutUnit height = ValueForLength(
        logical_height,
        GetDocument().GetLayoutView()->ViewLogicalHeightForPercentages());
    height *= LogicalSizeScaleFactorForPercentageLengths();
    return height;
  }

  return LayoutReplaced::ComputeReplacedLogicalHeight(estimated_used_width);
}

double LayoutSVGRoot::LogicalSizeScaleFactorForPercentageLengths() const {
  if (!IsDocumentElement() || !GetDocument().IsInMainFrame())
    return 1;
  if (GetDocument().GetLayoutView()->ShouldUsePrintingLayout())
    return 1;
  // This will return the zoom factor which is different from the typical usage
  // of "zoom factor" in blink (e.g., |LocalFrame::PageZoomFactor()|) which
  // includes CSS zoom and the device scale factor (if use-zoom-for-dsf is
  // enabled). For this special-case, we only want to include the user's zoom
  // factor, as all other types of zoom should not scale a percentage-sized svg.
  return GetFrame()->GetChromeClient().UserZoomFactor();
}

void LayoutSVGRoot::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutSize old_size = Size();
  UpdateLogicalWidth();
  UpdateLogicalHeight();

  // Whether we have a self-painting layer depends on whether there are
  // compositing descendants (see: |HasCompositingDescendants()| which is called
  // from |PaintLayer::UpdateSelfPaintingLayer()|). We cannot do this update in
  // StyleDidChange because descendants have not yet run StyleDidChange, so we
  // don't know their compositing reasons yet. A layout is scheduled when
  // |HasCompositingDescendants()| changes to ensure this is run.
  if (Layer() && RuntimeEnabledFeatures::CompositeSVGEnabled())
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
  SVGTransformChange transform_change = BuildLocalToBorderBoxTransform();

  // The scale factor from the local-to-border-box transform is all that our
  // scale-dependent descendants care about.
  did_screen_scale_factor_change_ =
      transform_change == SVGTransformChange::kFull;

  SVGLayoutSupport::LayoutResourcesIfNeeded(*this);

  // selfNeedsLayout() will cover changes to one (or more) of viewBox,
  // current{Scale,Translate}, decorations and 'overflow'.
  const bool viewport_may_have_changed =
      SelfNeedsLayout() || old_size != Size();

  auto* svg = To<SVGSVGElement>(GetNode());
  DCHECK(svg);
  // When hasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  is_layout_size_changed_ =
      viewport_may_have_changed && svg->HasRelativeLengths();

  SVGLayoutSupport::LayoutChildren(FirstChild(), false,
                                   did_screen_scale_factor_change_,
                                   is_layout_size_changed_);

  if (needs_boundaries_or_transform_update_) {
    UpdateCachedBoundaries();
    needs_boundaries_or_transform_update_ = false;
  }

  const auto& old_overflow_rect = VisualOverflowRect();
  ClearSelfNeedsLayoutOverflowRecalc();
  ClearLayoutOverflow();

  // The scale of one or more of the SVG elements may have changed, content
  // (the entire SVG) could have moved or new content may have been exposed, so
  // mark the entire subtree as needing paint invalidation checking.
  if (transform_change != SVGTransformChange::kNone ||
      viewport_may_have_changed || old_overflow_rect != VisualOverflowRect()) {
    SetSubtreeShouldCheckForPaintInvalidation();
    SetNeedsPaintPropertyUpdate();
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
  }

  UpdateAfterLayout();
  has_box_decoration_background_ = IsDocumentElement()
                                       ? StyleRef().HasBoxDecorationBackground()
                                       : HasBoxDecorationBackground();

  ClearNeedsLayout();
}

bool LayoutSVGRoot::ShouldApplyViewportClip() const {
  // the outermost svg is clipped if auto, and svg document roots are always
  // clipped. When the svg is stand-alone (isDocumentElement() == true) the
  // viewport clipping should always be applied, noting that the window
  // scrollbars should be hidden if overflow=hidden.
  return StyleRef().OverflowX() == EOverflow::kHidden ||
         StyleRef().OverflowX() == EOverflow::kAuto ||
         StyleRef().OverflowX() == EOverflow::kScroll || IsDocumentElement();
}

void LayoutSVGRoot::RecalcVisualOverflow() {
  LayoutReplaced::RecalcVisualOverflow();
  UpdateCachedBoundaries();
  if (!ShouldApplyViewportClip())
    AddContentsVisualOverflow(ComputeContentsVisualOverflow());
}

LayoutRect LayoutSVGRoot::ComputeContentsVisualOverflow() const {
  FloatRect content_visual_rect = VisualRectInLocalSVGCoordinates();
  content_visual_rect =
      local_to_border_box_transform_.MapRect(content_visual_rect);
  // Condition the visual overflow rect to avoid being clipped/culled
  // out if it is huge. This may sacrifice overflow, but usually only
  // overflow that would never be seen anyway.
  // To condition, we intersect with something that we oftentimes
  // consider to be "infinity".
  return Intersection(EnclosingLayoutRect(content_visual_rect),
                      LayoutRect(LayoutRect::InfiniteIntRect()));
}

void LayoutSVGRoot::PaintReplaced(const PaintInfo& paint_info,
                                  const PhysicalOffset& paint_offset) const {
  if (ChildPaintBlockedByDisplayLock())
    return;
  SVGRootPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutSVGRoot::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(To<SVGSVGElement>(*GetNode()), Style());
  LayoutReplaced::WillBeDestroyed();
}

bool LayoutSVGRoot::IntrinsicSizeIsFontMetricsDependent() const {
  const auto& svg = To<SVGSVGElement>(*GetNode());
  return svg.width()->CurrentValue()->IsFontRelative() ||
         svg.height()->CurrentValue()->IsFontRelative();
}

bool LayoutSVGRoot::StyleChangeAffectsIntrinsicSize(
    const ComputedStyle& old_style) const {
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
  if (diff.NeedsFullLayout())
    SetNeedsBoundariesUpdate();
  if (diff.NeedsPaintInvalidation()) {
    // Box decorations may have appeared/disappeared - recompute status.
    has_box_decoration_background_ = StyleRef().HasBoxDecorationBackground();
  }

  if (old_style && StyleChangeAffectsIntrinsicSize(*old_style))
    IntrinsicSizingInfoChanged();

  LayoutReplaced::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(To<SVGSVGElement>(*GetNode()),
                                         old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

bool LayoutSVGRoot::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  return child->IsSVG() && !(child->IsSVGInline() || child->IsSVGInlineText());
}

void LayoutSVGRoot::AddChild(LayoutObject* child, LayoutObject* before_child) {
  LayoutReplaced::AddChild(child, before_child);
  SVGResourcesCache::ClientWasAddedToTree(*child);

  bool should_isolate_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (should_isolate_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationRequired);
}

void LayoutSVGRoot::RemoveChild(LayoutObject* child) {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);
  LayoutReplaced::RemoveChild(child);

  bool had_non_isolated_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (had_non_isolated_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationNeedsUpdate);
}

bool LayoutSVGRoot::HasNonIsolatedBlendingDescendants() const {
  if (has_non_isolated_blending_descendants_dirty_) {
    has_non_isolated_blending_descendants_ =
        SVGLayoutSupport::ComputeHasNonIsolatedBlendingDescendants(this);
    has_non_isolated_blending_descendants_dirty_ = false;
  }
  return has_non_isolated_blending_descendants_;
}

void LayoutSVGRoot::DescendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
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
  LayoutReplaced::InsertedIntoTree();
  SVGResourcesCache::ClientWasAddedToTree(*this);
}

void LayoutSVGRoot::WillBeRemovedFromTree() {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*this);
  LayoutReplaced::WillBeRemovedFromTree();
}

PositionWithAffinity LayoutSVGRoot::PositionForPoint(
    const PhysicalOffset& point) const {
  FloatPoint absolute_point = FloatPoint(point);
  absolute_point =
      local_to_border_box_transform_.Inverse().MapPoint(absolute_point);
  LayoutObject* closest_descendant =
      SVGLayoutSupport::FindClosestLayoutSVGText(this, absolute_point);

  if (!closest_descendant)
    return LayoutReplaced::PositionForPoint(point);

  LayoutObject* layout_object = closest_descendant;
  AffineTransform transform = layout_object->LocalToSVGParentTransform();
  transform.Translate(ToLayoutSVGText(layout_object)->Location().X(),
                      ToLayoutSVGText(layout_object)->Location().Y());
  while (layout_object) {
    layout_object = layout_object->Parent();
    if (layout_object->IsSVGRoot())
      break;
    transform = layout_object->LocalToSVGParentTransform() * transform;
  }

  absolute_point = transform.Inverse().MapPoint(absolute_point);

  return closest_descendant->PositionForPoint(
      PhysicalOffset::FromFloatPointRound(absolute_point));
}

// LayoutBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
SVGTransformChange LayoutSVGRoot::BuildLocalToBorderBoxTransform() {
  SVGTransformChangeDetector change_detector(local_to_border_box_transform_);
  auto* svg = To<SVGSVGElement>(GetNode());
  DCHECK(svg);
  float scale = StyleRef().EffectiveZoom();
  local_to_border_box_transform_ = svg->ViewBoxToViewTransform(
      ContentWidth() / scale, ContentHeight() / scale);

  FloatPoint translate = svg->CurrentTranslate();
  LayoutSize border_and_padding(BorderLeft() + PaddingLeft(),
                                BorderTop() + PaddingTop());
  AffineTransform view_to_border_box_transform(
      scale, 0, 0, scale, border_and_padding.Width() + translate.X(),
      border_and_padding.Height() + translate.Y());
  view_to_border_box_transform.Scale(svg->currentScale());
  local_to_border_box_transform_.PreMultiply(view_to_border_box_transform);
  return change_detector.ComputeChange(local_to_border_box_transform_);
}

AffineTransform LayoutSVGRoot::LocalToSVGParentTransform() const {
  return AffineTransform::Translation(RoundToInt(Location().X()),
                                      RoundToInt(Location().Y())) *
         local_to_border_box_transform_;
}

// This method expects local CSS box coordinates.
// Callers with local SVG viewport coordinates should first apply the
// localToBorderBoxTransform to convert from SVG viewport coordinates to local
// CSS box coordinates.
void LayoutSVGRoot::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                       TransformState& transform_state,
                                       MapCoordinatesFlags mode) const {
  LayoutReplaced::MapLocalToAncestor(ancestor, transform_state, mode);
}

const LayoutObject* LayoutSVGRoot::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  return LayoutReplaced::PushMappingToContainer(ancestor_to_stop_at,
                                                geometry_map);
}

void LayoutSVGRoot::UpdateCachedBoundaries() {
  SVGLayoutSupport::ComputeContainerBoundingBoxes(
      this, object_bounding_box_, object_bounding_box_valid_,
      stroke_bounding_box_, visual_rect_in_local_svg_coordinates_);
}

bool LayoutSVGRoot::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                HitTestAction hit_test_action) {
  HitTestLocation local_border_box_location(hit_test_location,
                                            -accumulated_offset);

  // Only test SVG content if the point is in our content box, or in case we
  // don't clip to the viewport, the visual overflow rect.
  // FIXME: This should be an intersection when rect-based hit tests are
  // supported by nodeAtFloatPoint.
  bool skip_children = (result.GetHitTestRequest().GetStopNode() == this);
  if (!skip_children &&
      (local_border_box_location.Intersects(PhysicalContentBoxRect()) ||
       (!ShouldApplyViewportClip() &&
        local_border_box_location.Intersects(PhysicalVisualOverflowRect())))) {
    TransformedHitTestLocation local_location(local_border_box_location,
                                              LocalToBorderBoxTransform());
    if (local_location) {
      PhysicalOffset accumulated_offset_for_children;
      if (SVGLayoutSupport::HitTestChildren(
              LastChild(), result, *local_location,
              accumulated_offset_for_children, hit_test_action))
        return true;
    }
  }

  // If we didn't early exit above, we've just hit the container <svg> element.
  // Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
  if ((hit_test_action == kHitTestBlockBackground ||
       hit_test_action == kHitTestChildBlockBackground) &&
      VisibleToHitTestRequest(result.GetHitTestRequest())) {
    // Only return true here, if the last hit testing phase 'BlockBackground'
    // (or 'ChildBlockBackground' - depending on context) is executed.
    // If we'd return true in the 'Foreground' phase, hit testing would stop
    // immediately. For SVG only trees this doesn't matter.
    // Though when we have a <foreignObject> subtree we need to be able to
    // detect hits on the background of a <div> element.
    // If we'd return true here in the 'Foreground' phase, we are not able to
    // detect these hits anymore.
    PhysicalRect bounds_rect(accumulated_offset, Size());
    if (hit_test_location.Intersects(bounds_rect)) {
      UpdateHitTestResult(result, local_border_box_location.Point());
      if (result.AddNodeToListBasedTestResult(GetNode(), hit_test_location,
                                              bounds_rect) == kStopHitTesting)
        return true;
    }
  }

  return false;
}

void LayoutSVGRoot::NotifyDescendantCompositingReasonsChanged() {
  if (has_descendant_with_compositing_reason_dirty_)
    return;
  has_descendant_with_compositing_reason_dirty_ = true;
  SetNeedsLayout(layout_invalidation_reason::kSvgChanged);
}

PaintLayerType LayoutSVGRoot::LayerTypeRequired() const {
  auto layer_type_required = LayoutReplaced::LayerTypeRequired();
  if (layer_type_required == kNoPaintLayer) {
    // Force a paint layer so,
    // 1) In CompositeSVG mode, a GraphicsLayer can be created if there are
    // directly-composited descendants.
    // 2) The parent layer will know if there are non-isolated descendants with
    // blend mode.
    layer_type_required = kForcedPaintLayer;
  }
  return layer_type_required;
}

CompositingReasons LayoutSVGRoot::AdditionalCompositingReasons() const {
  return RuntimeEnabledFeatures::CompositeSVGEnabled() &&
                 HasDescendantWithCompositingReason()
             ? CompositingReason::kSVGRoot
             : CompositingReason::kNone;
}

bool LayoutSVGRoot::HasDescendantWithCompositingReason() const {
  if (has_descendant_with_compositing_reason_dirty_) {
    has_descendant_with_compositing_reason_ = false;
    for (const LayoutObject* object = FirstChild(); object;
         // Do not consider descendants of <foreignObject>.
         object = object->IsSVGForeignObject()
                      ? object->NextInPreOrderAfterChildren(this)
                      : object->NextInPreOrder(this)) {
      DCHECK(object->IsSVGChild());
      if (CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(
              *object) != CompositingReason::kNone) {
        has_descendant_with_compositing_reason_ = true;
        break;
      }
    }
    has_descendant_with_compositing_reason_dirty_ = false;
  }
  return has_descendant_with_compositing_reason_;
}

}  // namespace blink

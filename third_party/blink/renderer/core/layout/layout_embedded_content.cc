/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

LayoutEmbeddedContent::LayoutEmbeddedContent(HTMLFrameOwnerElement* element)
    : LayoutReplaced(element) {
  DCHECK(element);
  SetInline(false);
}

void LayoutEmbeddedContent::WillBeDestroyed() {
  NOT_DESTROYED();
  if (auto* frame_owner = GetFrameOwnerElement())
    frame_owner->SetEmbeddedContentView(nullptr);

  LayoutReplaced::WillBeDestroyed();

  ClearNode();
}

FrameView* LayoutEmbeddedContent::ChildFrameView() const {
  NOT_DESTROYED();
  return DynamicTo<FrameView>(GetEmbeddedContentView());
}

LayoutView* LayoutEmbeddedContent::ChildLayoutView() const {
  NOT_DESTROYED();
  if (HTMLFrameOwnerElement* owner_element = GetFrameOwnerElement()) {
    if (Document* content_document = owner_element->contentDocument())
      return content_document->GetLayoutView();
  }
  return nullptr;
}

WebPluginContainerImpl* LayoutEmbeddedContent::Plugin() const {
  NOT_DESTROYED();
  EmbeddedContentView* embedded_content_view = GetEmbeddedContentView();
  if (embedded_content_view && embedded_content_view->IsPluginView())
    return To<WebPluginContainerImpl>(embedded_content_view);
  return nullptr;
}

EmbeddedContentView* LayoutEmbeddedContent::GetEmbeddedContentView() const {
  NOT_DESTROYED();
  if (auto* frame_owner = GetFrameOwnerElement())
    return frame_owner->OwnedEmbeddedContentView();
  return nullptr;
}

const std::optional<PhysicalSize> LayoutEmbeddedContent::FrozenFrameSize()
    const {
  // The `<fencedframe>` element can freeze the child frame size when navigated.
  if (const auto* fenced_frame = DynamicTo<HTMLFencedFrameElement>(GetNode()))
    return fenced_frame->FrozenFrameSize();

  return std::nullopt;
}

AffineTransform LayoutEmbeddedContent::EmbeddedContentTransform() const {
  auto frozen_size = FrozenFrameSize();
  if (!frozen_size || frozen_size->IsEmpty()) {
    const PhysicalOffset content_box_offset = PhysicalContentBoxOffset();
    return AffineTransform().Translate(content_box_offset.left,
                                       content_box_offset.top);
  }

  AffineTransform translate_and_scale;
  auto replaced_rect = ReplacedContentRect();
  translate_and_scale.Translate(replaced_rect.X(), replaced_rect.Y());
  translate_and_scale.Scale(replaced_rect.Width() / frozen_size->width,
                            replaced_rect.Height() / frozen_size->height);
  return translate_and_scale;
}

PhysicalOffset LayoutEmbeddedContent::EmbeddedContentFromBorderBox(
    const PhysicalOffset& offset) const {
  gfx::PointF point(offset);
  return PhysicalOffset::FromPointFRound(
      EmbeddedContentTransform().Inverse().MapPoint(point));
}

gfx::PointF LayoutEmbeddedContent::EmbeddedContentFromBorderBox(
    const gfx::PointF& point) const {
  return EmbeddedContentTransform().Inverse().MapPoint(point);
}

PhysicalOffset LayoutEmbeddedContent::BorderBoxFromEmbeddedContent(
    const PhysicalOffset& offset) const {
  gfx::PointF point(offset);
  return PhysicalOffset::FromPointFRound(
      EmbeddedContentTransform().MapPoint(point));
}

gfx::Rect LayoutEmbeddedContent::BorderBoxFromEmbeddedContent(
    const gfx::Rect& rect) const {
  return EmbeddedContentTransform().MapRect(rect);
}

PaintLayerType LayoutEmbeddedContent::LayerTypeRequired() const {
  NOT_DESTROYED();
  PaintLayerType type = LayoutReplaced::LayerTypeRequired();
  if (type != kNoPaintLayer)
    return type;
  return kForcedPaintLayer;
}

bool LayoutEmbeddedContent::PointOverResizer(
    const HitTestResult& result,
    const HitTestLocation& location,
    const PhysicalOffset& accumulated_offset) const {
  NOT_DESTROYED();
  if (const auto* scrollable_area = GetScrollableArea()) {
    const HitTestRequest::HitTestRequestType hit_type =
        result.GetHitTestRequest().GetType();
    const blink::ResizerHitTestType resizer_type =
        hit_type & HitTestRequest::kTouchEvent ? kResizerForTouch
                                               : kResizerForPointer;
    return scrollable_area->IsAbsolutePointInResizeControl(
        ToRoundedPoint(location.Point() - accumulated_offset), resizer_type);
  }
  return false;
}

void LayoutEmbeddedContent::PropagateZoomFactor(double zoom_factor) {
  if (GetDocument().StandardizedBrowserZoomEnabled()) {
    const auto* fenced_frame = DynamicTo<HTMLFencedFrameElement>(GetNode());
    if (!fenced_frame) {
      if (auto* embedded_content_view = GetEmbeddedContentView()) {
        embedded_content_view->ZoomFactorChanged(zoom_factor);
      }
    }
  }
}

bool LayoutEmbeddedContent::NodeAtPointOverEmbeddedContentView(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  bool had_result = result.InnerNode();
  bool inside = LayoutReplaced::NodeAtPoint(result, hit_test_location,
                                            accumulated_offset, phase);

  // Check to see if we are really over the EmbeddedContentView itself (and not
  // just in the border/padding area or the resizer area).
  if ((inside || hit_test_location.IsRectBasedTest()) && !had_result &&
      result.InnerNode() == GetNode()) {
    bool is_over_content_view =
        PhysicalContentBoxRect().Contains(result.LocalPoint()) &&
        !result.IsOverResizer();
    result.SetIsOverEmbeddedContentView(is_over_content_view);
  }
  return inside;
}

bool LayoutEmbeddedContent::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  auto* local_frame_view = DynamicTo<LocalFrameView>(ChildFrameView());
  bool skip_contents =
      (result.GetHitTestRequest().GetStopNode() == this ||
       !result.GetHitTestRequest().AllowsChildFrameContent() ||
       PointOverResizer(result, hit_test_location, accumulated_offset));

  if (!local_frame_view || skip_contents) {
    return NodeAtPointOverEmbeddedContentView(result, hit_test_location,
                                              accumulated_offset, phase);
  }

  // A hit test can never hit an off-screen element; only off-screen iframes are
  // throttled; therefore, hit tests can skip descending into throttled iframes.
  // We also check the document lifecycle state because the frame may have been
  // throttled at the time lifecycle updates happened, in which case it will not
  // be up-to-date and we can't hit test it.
  if (local_frame_view->ShouldThrottleRendering() ||
      !local_frame_view->GetFrame().GetDocument() ||
      local_frame_view->GetFrame().GetDocument()->Lifecycle().GetState() <
          DocumentLifecycle::kPrePaintClean) {
    return NodeAtPointOverEmbeddedContentView(result, hit_test_location,
                                              accumulated_offset, phase);
  }

  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (phase == HitTestPhase::kForeground) {
    auto* child_layout_view = local_frame_view->GetLayoutView();

    if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
        child_layout_view) {
      PhysicalOffset content_offset(BorderLeft() + PaddingLeft(),
                                    BorderTop() + PaddingTop());
      HitTestLocation new_hit_test_location(
          hit_test_location, -accumulated_offset - content_offset);
      HitTestRequest new_hit_test_request(
          result.GetHitTestRequest().GetType() |
              HitTestRequest::kChildFrameHitTest,
          result.GetHitTestRequest().GetStopNode());
      HitTestResult child_frame_result(new_hit_test_request,
                                       new_hit_test_location);

      // The frame's layout and style must be up to date if we reach here.
      bool is_inside_child_frame = child_layout_view->HitTestNoLifecycleUpdate(
          new_hit_test_location, child_frame_result);

      if (result.GetHitTestRequest().ListBased()) {
        result.Append(child_frame_result);
      } else if (is_inside_child_frame) {
        // Force the result not to be cacheable because the parent frame should
        // not cache this result; as it won't be notified of changes in the
        // child.
        child_frame_result.SetCacheable(false);
        result = child_frame_result;
      }

      // Don't trust |isInsideChildFrame|. For rect-based hit-test, returns
      // true only when the hit test rect is totally within the iframe,
      // i.e. nodeAtPointOverEmbeddedContentView() also returns true.
      // Use a temporary HitTestResult because we don't want to collect the
      // iframe element itself if the hit-test rect is totally within the
      // iframe.
      if (is_inside_child_frame) {
        if (!hit_test_location.IsRectBasedTest())
          return true;
        HitTestResult point_over_embedded_content_view_result = result;
        bool point_over_embedded_content_view =
            NodeAtPointOverEmbeddedContentView(
                point_over_embedded_content_view_result, hit_test_location,
                accumulated_offset, phase);
        if (point_over_embedded_content_view)
          return true;
        result = point_over_embedded_content_view_result;
        return false;
      }
    }
  }

  return NodeAtPointOverEmbeddedContentView(result, hit_test_location,
                                            accumulated_offset, phase);
}

void LayoutEmbeddedContent::StyleDidChange(StyleDifference diff,
                                           const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);
  const ComputedStyle& new_style = StyleRef();

  if (Frame* frame = GetFrameOwnerElement()->ContentFrame())
    frame->UpdateInertIfPossible();

  if (EmbeddedContentView* embedded_content_view = GetEmbeddedContentView()) {
    if (new_style.UsedVisibility() != EVisibility::kVisible) {
      embedded_content_view->Hide();
    } else {
      embedded_content_view->Show();
    }
  }

  auto* frame_owner = GetFrameOwnerElement();
  if (!frame_owner)
    return;

  if (old_style &&
      new_style.UsedColorScheme() != old_style->UsedColorScheme()) {
    frame_owner->SetColorScheme(new_style.UsedColorScheme());
  }
  if (!old_style || new_style.EffectiveZoom() != old_style->EffectiveZoom()) {
    PropagateZoomFactor(new_style.EffectiveZoom());
  }

  if (old_style &&
      new_style.VisibleToHitTesting() == old_style->VisibleToHitTesting()) {
    return;
  }

  if (auto* frame = frame_owner->ContentFrame())
    frame->UpdateVisibleToHitTesting();
}

void LayoutEmbeddedContent::PaintReplaced(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock())
    return;
  EmbeddedContentPainter(*this).PaintReplaced(paint_info, paint_offset);
}

CursorDirective LayoutEmbeddedContent::GetCursor(const PhysicalOffset& point,
                                                 ui::Cursor& cursor) const {
  NOT_DESTROYED();
  if (Plugin()) {
    // A plugin is responsible for setting the cursor when the pointer is over
    // it.
    return kDoNotSetCursor;
  }
  return LayoutReplaced::GetCursor(point, cursor);
}

PhysicalRect LayoutEmbeddedContent::ReplacedContentRectFrom(
    const PhysicalRect& base_content_rect) const {
  NOT_DESTROYED();
  PhysicalRect content_rect = base_content_rect;

  // IFrames set as the root scroller should get their size from their parent.
  // When scrolling starts so as to hide the URL bar, IFRAME wouldn't resize to
  // match the now expanded size of the viewport until the scrolling stops. This
  // makes sure the |ReplacedContentRect| matches the expanded viewport even
  // before IFRAME resizes, for clipping to work correctly.
  if (ChildFrameView() && View() && IsEffectiveRootScroller()) {
    content_rect.offset = PhysicalOffset();
    content_rect.size = View()->ViewRect().size;
  }

  if (const std::optional<PhysicalSize> frozen_size = FrozenFrameSize()) {
    // TODO(kojii): Setting the `offset` to non-zero values breaks
    // hit-testing/inputs. Even different size is suspicious, as the input
    // system forwards mouse events to the child frame even when the mouse is
    // outside of the child frame. Revisit this when the input system supports
    // different |ReplacedContentRect| from |PhysicalContentBoxRect|.
    PhysicalSize frozen_layout_size = *frozen_size;
    content_rect =
        ComputeReplacedContentRect(base_content_rect, &frozen_layout_size);
  }

  // We don't propagate sub-pixel into sub-frame layout, in other words, the
  // rect is snapped at the document boundary, and sub-pixel movement could
  // cause the sub-frame to layout due to the 1px snap difference. In order to
  // avoid that, the size of sub-frame is rounded in advance.
  return PreSnappedRectForPersistentSizing(content_rect);
}

void LayoutEmbeddedContent::UpdateOnEmbeddedContentViewChange() {
  NOT_DESTROYED();
  if (!Style())
    return;

  if (EmbeddedContentView* embedded_content_view = GetEmbeddedContentView()) {
    if (!NeedsLayout()) {
      UpdateGeometry(*embedded_content_view);
    }
    if (Style()) {
      PropagateZoomFactor(StyleRef().EffectiveZoom());
      if (StyleRef().UsedVisibility() != EVisibility::kVisible) {
        embedded_content_view->Hide();
      } else {
        embedded_content_view->Show();
      }
    }
  }

  // One of the reasons of the following is that the layout tree in the new
  // embedded content view may have already had some paint property and paint
  // invalidation flags set, and we need to propagate the flags into the host
  // view. Adding, changing and removing are also significant changes to the
  // tree so setting the flags ensures the required updates.
  SetNeedsPaintPropertyUpdate();
  SetShouldDoFullPaintInvalidation();
}

void LayoutEmbeddedContent::UpdateGeometry(
    EmbeddedContentView& embedded_content_view) {
  NOT_DESTROYED();
  // TODO(wangxianzhu): We reset subpixel accumulation at some boundaries, so
  // the following code is incorrect when some ancestors are such boundaries.
  // What about multicol? Need a LayoutBox function to query sub-pixel
  // accumulation.
  PhysicalRect replaced_rect = ReplacedContentRect();
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 gfx::PointF(),
                                 gfx::QuadF(gfx::RectF(replaced_rect)));
  MapLocalToAncestor(nullptr, transform_state, 0);
  transform_state.Flatten();
  PhysicalOffset absolute_location =
      PhysicalOffset::FromPointFRound(transform_state.LastPlanarPoint());
  PhysicalRect absolute_replaced_rect = replaced_rect;
  absolute_replaced_rect.Move(absolute_location);
  gfx::RectF absolute_bounding_box =
      transform_state.LastPlanarQuad().BoundingBox();
  gfx::Rect frame_rect(gfx::Point(),
                       ToPixelSnappedRect(absolute_replaced_rect).size());
  // Normally the location of the frame rect is ignored by the painter, but
  // currently it is still used by a family of coordinate conversion function in
  // LocalFrameView. This is incorrect because coordinate conversion
  // needs to take transform and into account. A few callers still use the
  // family of conversion function, including but not exhaustive:
  // LocalFrameView::updateViewportIntersectionIfNeeded()
  // RemoteFrameView::frameRectsChanged().
  // WebPluginContainerImpl::reportGeometry()
  // TODO(trchen): Remove this hack once we fixed all callers.
  frame_rect.set_origin(gfx::ToRoundedPoint(absolute_bounding_box.origin()));

  // As an optimization, we don't include the root layer's scroll offset in the
  // frame rect.  As a result, we don't need to recalculate the frame rect every
  // time the root layer scrolls; however, each implementation of
  // EmbeddedContentView::FrameRect() must add the root layer's scroll offset
  // into its position.
  // TODO(szager): Refactor this functionality into EmbeddedContentView, rather
  // than reimplementing in each concrete subclass.
  LayoutView* layout_view = View();
  if (layout_view && layout_view->IsScrollContainer()) {
    // Floored because the PixelSnappedScrollOffset returns a ScrollOffset
    // which is a float-type but frame_rect in a content view is an gfx::Rect.
    // We may want to reevaluate the use of pixel snapping that since scroll
    // offsets/layout can be fractional.
    frame_rect.Offset(layout_view->PixelSnappedScrolledContentOffset());
  }

  embedded_content_view.SetFrameRect(frame_rect);
}

bool LayoutEmbeddedContent::IsThrottledFrameView() const {
  NOT_DESTROYED();
  if (auto* local_frame_view = DynamicTo<LocalFrameView>(ChildFrameView()))
    return local_frame_view->ShouldThrottleRendering();
  return false;
}

}  // namespace blink

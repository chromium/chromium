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
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

LayoutEmbeddedContent::LayoutEmbeddedContent(HTMLFrameOwnerElement* element)
    : LayoutReplaced(element),
      // Reference counting is used to prevent the part from being destroyed
      // while inside the EmbeddedContentView code, which might not be able to
      // handle that.
      ref_count_(1) {
  DCHECK(element);
  SetInline(false);
}

void LayoutEmbeddedContent::Release() {
  NOT_DESTROYED();
  if (--ref_count_ <= 0)
    delete this;
}

void LayoutEmbeddedContent::WillBeDestroyed() {
  NOT_DESTROYED();
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->Remove(this);

  if (auto* frame_owner = GetFrameOwnerElement())
    frame_owner->SetEmbeddedContentView(nullptr);

  LayoutReplaced::WillBeDestroyed();
}

void LayoutEmbeddedContent::DeleteThis() {
  NOT_DESTROYED();
  // We call clearNode here because LayoutEmbeddedContent is ref counted. This
  // call to destroy may not actually destroy the layout object. We can keep it
  // around because of references from the LocalFrameView class. (The actual
  // destruction of the class happens in PostDestroy() which is called from
  // Release()).
  //
  // But, we've told the system we've destroyed the layoutObject, which happens
  // when the DOM node is destroyed. So there is a good chance the DOM node this
  // object points too is invalid, so we have to clear the node so we make sure
  // we don't access it in the future.
  ClearNode();
  Release();
}

LayoutEmbeddedContent::~LayoutEmbeddedContent() {
  DCHECK_LE(ref_count_, 0);
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

PaintLayerType LayoutEmbeddedContent::LayerTypeRequired() const {
  NOT_DESTROYED();
  if (AdditionalCompositingReasons())
    return kNormalPaintLayer;

  PaintLayerType type = LayoutReplaced::LayerTypeRequired();
  if (type != kNoPaintLayer)
    return type;

  // We can't check layout_view->Layer()->GetCompositingReasons() here because
  // we're only in style update, so haven't run compositing update yet.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (LayoutView* child_layout_view = ChildLayoutView()) {
      if (child_layout_view->AdditionalCompositingReasons())
        return kNormalPaintLayer;
    }
  }

  return kForcedPaintLayer;
}

bool LayoutEmbeddedContent::ContentDocumentContainsGraphicsLayer() const {
  NOT_DESTROYED();
  if (PaintLayerCompositor* inner_compositor =
          PaintLayerCompositor::FrameContentsCompositor(*this)) {
    return inner_compositor->StaleInCompositingMode();
  }
  return false;
}

bool LayoutEmbeddedContent::NodeAtPointOverEmbeddedContentView(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  NOT_DESTROYED();
  bool had_result = result.InnerNode();
  bool inside = LayoutReplaced::NodeAtPoint(result, hit_test_location,
                                            accumulated_offset, action);

  // Check to see if we are really over the EmbeddedContentView itself (and not
  // just in the border/padding area).
  if ((inside || hit_test_location.IsRectBasedTest()) && !had_result &&
      result.InnerNode() == GetNode()) {
    result.SetIsOverEmbeddedContentView(
        PhysicalContentBoxRect().Contains(result.LocalPoint()));
  }
  return inside;
}

bool LayoutEmbeddedContent::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  NOT_DESTROYED();
  auto* local_frame_view = DynamicTo<LocalFrameView>(ChildFrameView());
  bool skip_contents = (result.GetHitTestRequest().GetStopNode() == this ||
                        !result.GetHitTestRequest().AllowsChildFrameContent());
  if (!local_frame_view || skip_contents) {
    return NodeAtPointOverEmbeddedContentView(result, hit_test_location,
                                              accumulated_offset, action);
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
                                              accumulated_offset, action);
  }

  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (action == kHitTestForeground) {
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
      child_frame_result.SetInertNode(result.InertNode());

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
                accumulated_offset, action);
        if (point_over_embedded_content_view)
          return true;
        result = point_over_embedded_content_view_result;
        return false;
      }
    }
  }

  return NodeAtPointOverEmbeddedContentView(result, hit_test_location,
                                            accumulated_offset, action);
}

CompositingReasons LayoutEmbeddedContent::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  WebPluginContainerImpl* plugin_view = Plugin();
  if (plugin_view && plugin_view->CcLayer())
    return CompositingReason::kPlugin;
  if (auto* element = GetFrameOwnerElement()) {
    if (Frame* content_frame = element->ContentFrame()) {
      if (content_frame->IsRemoteFrame())
        return CompositingReason::kIFrame;
    }
  }
  return CompositingReason::kNone;
}

void LayoutEmbeddedContent::StyleDidChange(StyleDifference diff,
                                           const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);

  if (EmbeddedContentView* embedded_content_view = GetEmbeddedContentView()) {
    if (StyleRef().Visibility() != EVisibility::kVisible) {
      embedded_content_view->Hide();
    } else {
      embedded_content_view->Show();
    }
  }

  if (old_style &&
      StyleRef().VisibleToHitTesting() == old_style->VisibleToHitTesting()) {
    return;
  }

  auto* frame_owner = GetFrameOwnerElement();
  if (!frame_owner)
    return;

  auto* frame = frame_owner->ContentFrame();
  if (!frame)
    return;

  frame->UpdateVisibleToHitTesting();
}

void LayoutEmbeddedContent::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  UpdateAfterLayout();
  ClearNeedsLayout();
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

PhysicalRect LayoutEmbeddedContent::ReplacedContentRect() const {
  NOT_DESTROYED();
  PhysicalRect content_rect = PhysicalContentBoxRect();
  // IFrames set as the root scroller should get their size from their parent.
  if (ChildFrameView() && View() && IsEffectiveRootScroller()) {
    content_rect.offset = PhysicalOffset();
    content_rect.size = View()->ViewRect().size;
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
    if (!NeedsLayout())
      UpdateGeometry(*embedded_content_view);

    if (StyleRef().Visibility() != EVisibility::kVisible)
      embedded_content_view->Hide();
    else
      embedded_content_view->Show();
  }

  // One of the reasons of the following is that the layout tree in the new
  // embedded content view may have already had some paint property and paint
  // invalidation flags set, and we need to propagate the flags into the host
  // view. Adding, changing and removing are also significant changes to the
  // tree so setting the flags ensures the required updates.
  SetNeedsPaintPropertyUpdate();
  SetShouldDoFullPaintInvalidation();
  // Showing/hiding the embedded content view and changing the view between null
  // and non-null affect compositing (see: PaintLayerCompositor::CanBeComposited
  // and RootShouldAlwaysComposite).
  if (HasLayer())
    Layer()->SetNeedsCompositingInputsUpdate();
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
                                 FloatPoint(),
                                 FloatQuad(FloatRect(replaced_rect)));
  MapLocalToAncestor(nullptr, transform_state, 0);
  transform_state.Flatten();
  PhysicalOffset absolute_location =
      PhysicalOffset::FromFloatPointRound(transform_state.LastPlanarPoint());
  PhysicalRect absolute_replaced_rect = replaced_rect;
  absolute_replaced_rect.Move(absolute_location);
  FloatRect absolute_bounding_box =
      transform_state.LastPlanarQuad().BoundingBox();
  IntRect frame_rect(IntPoint(),
                     PixelSnappedIntRect(absolute_replaced_rect).Size());
  // Normally the location of the frame rect is ignored by the painter, but
  // currently it is still used by a family of coordinate conversion function in
  // LocalFrameView. This is incorrect because coordinate conversion
  // needs to take transform and into account. A few callers still use the
  // family of conversion function, including but not exhaustive:
  // LocalFrameView::updateViewportIntersectionIfNeeded()
  // RemoteFrameView::frameRectsChanged().
  // WebPluginContainerImpl::reportGeometry()
  // TODO(trchen): Remove this hack once we fixed all callers.
  frame_rect.SetLocation(RoundedIntPoint(absolute_bounding_box.Location()));

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
    // which is a float-type but frame_rect in a content view is an IntRect. We
    // may want to reevaluate the use of pixel snapping that since scroll
    // offsets/layout can be fractional.
    frame_rect.MoveBy(layout_view->PixelSnappedScrolledContentOffset());
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

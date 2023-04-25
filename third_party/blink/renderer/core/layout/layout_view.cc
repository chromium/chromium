/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_view.h"

#include <inttypes.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/view_fragmentation_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/view_painter.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/quad_f.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#endif

namespace blink {

namespace {

class HitTestLatencyRecorder {
 public:
  HitTestLatencyRecorder(bool allows_child_frame_content)
      : start_(base::TimeTicks::Now()),
        allows_child_frame_content_(allows_child_frame_content) {}

  ~HitTestLatencyRecorder() {
    base::TimeDelta duration = base::TimeTicks::Now() - start_;
    if (allows_child_frame_content_) {
      DEFINE_STATIC_LOCAL(CustomCountHistogram, recursive_latency_histogram,
                          ("Event.Latency.HitTestRecursive", 0, 10000000, 100));
      recursive_latency_histogram.CountMicroseconds(duration);
    } else {
      DEFINE_STATIC_LOCAL(CustomCountHistogram, latency_histogram,
                          ("Event.Latency.HitTest", 0, 10000000, 100));
      latency_histogram.CountMicroseconds(duration);
    }
  }

 private:
  base::TimeTicks start_;
  bool allows_child_frame_content_;
};

}  // namespace

LayoutView::LayoutView(ContainerNode* document)
    : LayoutBlockFlow(document),
      frame_view_(To<Document>(document)->View()),
      layout_quote_head_(nullptr),
      layout_counter_count_(0),
      hit_test_count_(0),
      hit_test_cache_hits_(0),
      hit_test_cache_(MakeGarbageCollected<HitTestCache>()),
      autosize_h_scrollbar_mode_(mojom::blink::ScrollbarMode::kAuto),
      autosize_v_scrollbar_mode_(mojom::blink::ScrollbarMode::kAuto) {
  // init LayoutObject attributes
  SetInline(false);

  SetIntrinsicLogicalWidthsDirty(kMarkOnlyThis);

  SetPositionState(EPosition::kAbsolute);  // to 0,0 :)

  // Update the cached bit here since the Document is made the effective root
  // scroller before we've created the layout tree.
  if (GetDocument().GetRootScrollerController().EffectiveRootScroller() ==
      GetDocument()) {
    SetIsEffectiveRootScroller(true);
  }
}

LayoutView::~LayoutView() = default;

void LayoutView::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  visitor->Trace(fragmentation_context_);
  visitor->Trace(layout_quote_head_);
  visitor->Trace(svg_text_descendants_);
  visitor->Trace(hit_test_cache_);
  LayoutBlockFlow::Trace(visitor);
}

bool LayoutView::HitTest(const HitTestLocation& location,
                         HitTestResult& result) {
  NOT_DESTROYED();
  // We have to recursively update layout/style here because otherwise, when the
  // hit test recurses into a child document, it could trigger a layout on the
  // parent document, which can destroy PaintLayer that are higher up in the
  // call stack, leading to crashes.
  // Note that Document::UpdateLayout calls its parent's UpdateLayout.
  // Note that if an iframe has its render pipeline throttled, it will not
  // update layout here, and it will also not propagate the hit test into the
  // iframe's inner document.
  if (!GetFrameView()->UpdateLifecycleToPrePaintClean(
          DocumentUpdateReason::kHitTest))
    return false;

  // This means the LayoutView is not updated for PrePaint above, probably
  // because the frame is detached.
  if (!FirstFragment().HasLocalBorderBoxProperties())
    return false;

  HitTestLatencyRecorder hit_test_latency_recorder(
      result.GetHitTestRequest().AllowsChildFrameContent());
  return HitTestNoLifecycleUpdate(location, result);
}

bool LayoutView::HitTestNoLifecycleUpdate(const HitTestLocation& location,
                                          HitTestResult& result) {
  NOT_DESTROYED();
  TRACE_EVENT_BEGIN0("blink,devtools.timeline", "HitTest");
  hit_test_count_++;

  uint64_t dom_tree_version = GetDocument().DomTreeVersion();
  HitTestResult cache_result = result;
  bool hit_layer = false;
  if (hit_test_cache_->LookupCachedResult(location, cache_result,
                                          dom_tree_version)) {
    hit_test_cache_hits_++;
    hit_layer = true;
    result = cache_result;
  } else {
    LocalFrameView* frame_view = GetFrameView();
    PhysicalRect hit_test_area;
    if (frame_view) {
      // Start with a rect sized to the frame, to ensure we include the
      // scrollbars.
      hit_test_area.size = PhysicalSize(frame_view->Size());
      if (result.GetHitTestRequest().IgnoreClipping()) {
        hit_test_area.Unite(
            frame_view->DocumentToFrame(PhysicalRect(DocumentRect())));
      }
    }

    hit_layer = Layer()->HitTest(location, result, hit_test_area);

    // If hitTestResult include scrollbar, innerNode should be the parent of the
    // scrollbar.
    if (result.GetScrollbar()) {
      // Clear innerNode if we hit a scrollbar whose ScrollableArea isn't
      // associated with a LayoutBox so we aren't hitting some random element
      // below too.
      result.SetInnerNode(nullptr);
      result.SetURLElement(nullptr);
      ScrollableArea* scrollable_area =
          result.GetScrollbar()->GetScrollableArea();
      if (scrollable_area && scrollable_area->GetLayoutBox() &&
          scrollable_area->GetLayoutBox()->GetNode()) {
        Node* node = scrollable_area->GetLayoutBox()->GetNode();

        // If scrollbar belongs to Document, we should set innerNode to the
        // <html> element to match other browser.
        if (node->IsDocumentNode())
          node = node->GetDocument().documentElement();

        result.SetInnerNode(node);
        result.SetURLElement(node->EnclosingLinkEventParentOrSelf());
      }
    }

    if (hit_layer)
      hit_test_cache_->AddCachedResult(location, result, dom_tree_version);
  }

  TRACE_EVENT_END1("blink,devtools.timeline", "HitTest", "endData",
                   [&](perfetto::TracedValue context) {
                     inspector_hit_test_event::EndData(
                         std::move(context), result.GetHitTestRequest(),
                         location, result);
                   });
  return hit_layer;
}

void LayoutView::ClearHitTestCache() {
  NOT_DESTROYED();
  hit_test_cache_->Clear();
  auto* object = GetFrame()->OwnerLayoutObject();
  if (object)
    object->View()->ClearHitTestCache();
}

void LayoutView::ComputeLogicalHeight(
    LayoutUnit logical_height,
    LayoutUnit,
    LogicalExtentComputedValues& computed_values) const {
  NOT_DESTROYED();
  computed_values.extent_ = LayoutUnit(ViewLogicalHeightForBoxSizing());
}

bool LayoutView::IsChildAllowed(LayoutObject* child,
                                const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsBox();
}

bool LayoutView::CanHaveChildren() const {
  NOT_DESTROYED();
  FrameOwner* owner = GetFrame()->Owner();
  if (!owner)
    return true;
  // Although it is not spec compliant, many websites intentionally call
  // Window.print() on display:none iframes. https://crbug.com/819327.
  if (GetDocument().Printing())
    return true;
  // A PluginDocument needs a layout tree during loading, even if it is inside a
  // display: none iframe.  This is because WebLocalFrameImpl::DidFinish expects
  // the PluginDocument's <embed> element to have an EmbeddedContentView, which
  // it acquires during LocalFrameView::UpdatePlugins, which operates on the
  // <embed> element's layout object (LayoutEmbeddedObject).
  if (IsA<PluginDocument>(GetDocument()) ||
      GetDocument().IsForExternalHandler())
    return true;
  return !owner->IsDisplayNone();
}

bool LayoutView::ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
  NOT_DESTROYED();
  LocalFrame& frame = GetFrameView()->GetFrame();
  // See crbug.com/249860
  if (frame.IsOutermostMainFrame()) {
    Settings* settings = GetDocument().GetSettings();
    if (!settings || !settings->GetPlaceRTLScrollbarsOnLeftSideInMainFrame())
      return false;
  }
  // <body> inherits 'direction' from <html>, so checking style on the body is
  // sufficient.
  if (Element* body = GetDocument().body()) {
    if (LayoutObject* body_layout_object = body->GetLayoutObject()) {
      return body_layout_object->StyleRef()
          .ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
    }
  }
  return false;
}

void LayoutView::UpdateLayout() {
  NOT_DESTROYED();
  if (!GetDocument().Printing()) {
    page_size_ = PhysicalSize();
  }

  if (PageLogicalHeight() && ShouldUsePrintingLayout()) {
    intrinsic_logical_widths_ = LogicalWidth();
    if (!fragmentation_context_) {
      fragmentation_context_ =
          MakeGarbageCollected<ViewFragmentationContext>(*this);
    }
  } else if (fragmentation_context_) {
    fragmentation_context_.Clear();
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The font code in FontPlatformData does not have a direct connection to the
  // document, the frame or anything from which we could retrieve the device
  // scale factor. After using zoom for DSF, the GraphicsContext does only ever
  // have a DSF of 1 on Linux. In order for the font code to be aware of an up
  // to date DSF when layout happens, we plumb this through to the FontCache, so
  // that we can correctly retrieve RenderStyleForStrike from out of
  // process. crbug.com/845468
  LocalFrame& frame = GetFrameView()->GetFrame();
  ChromeClient& chrome_client = frame.GetChromeClient();
  FontCache::SetDeviceScaleFactor(
      chrome_client.GetScreenInfo(frame).device_scale_factor);
#endif

  LayoutBlockFlow::UpdateLayout();
  ClearNeedsLayout();
}

PhysicalRect LayoutView::LocalVisualRectIgnoringVisibility() const {
  NOT_DESTROYED();
  PhysicalRect rect = PhysicalVisualOverflowRect();
  rect.Unite(PhysicalRect(rect.offset, ViewRect().size));
  return rect;
}

void LayoutView::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                    TransformState& transform_state,
                                    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (!ancestor && !(mode & kIgnoreTransforms) &&
      ShouldUseTransformFromContainer(nullptr)) {
    gfx::Transform t;
    GetTransformFromContainer(nullptr, PhysicalOffset(), t);
    transform_state.ApplyTransform(t);
  }

  if (ancestor == this)
    return;

  if (mode & kTraverseDocumentBoundaries) {
    auto* parent_doc_layout_object = GetFrame()->OwnerLayoutObject();
    if (parent_doc_layout_object) {
      transform_state.Move(
          parent_doc_layout_object->PhysicalContentBoxOffset());
      parent_doc_layout_object->MapLocalToAncestor(ancestor, transform_state,
                                                   mode);
    } else {
      DCHECK(!ancestor);
      if (mode & kApplyRemoteMainFrameTransform)
        GetFrameView()->MapLocalToRemoteMainFrame(transform_state);
    }
  }
}

void LayoutView::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                    TransformState& transform_state,
                                    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  if (this != ancestor && (mode & kTraverseDocumentBoundaries)) {
    if (auto* parent_doc_layout_object = GetFrame()->OwnerLayoutObject()) {
      // A LayoutView is a containing block for fixed-position elements, so
      // don't carry this state across frames.
      parent_doc_layout_object->MapAncestorToLocal(ancestor, transform_state,
                                                   mode);

      transform_state.Move(
          parent_doc_layout_object->PhysicalContentBoxOffset());
    } else {
      DCHECK(!ancestor);
      // Note that MapLocalToRemoteMainFrame is correct here because
      // transform_state will be set to kUnapplyInverseTransformDirection.
      if (mode & kApplyRemoteMainFrameTransform)
        GetFrameView()->MapLocalToRemoteMainFrame(transform_state);
    }
  } else {
    DCHECK(this == ancestor || !ancestor);
  }
}

LogicalSize LayoutView::InitialContainingBlockSize() const {
  return LogicalSize(LayoutUnit(ViewLogicalWidthForBoxSizing()),
                     LayoutUnit(ViewLogicalHeightForBoxSizing()));
}

TrackedDescendantsMap& LayoutView::SvgTextDescendantsMap() {
  if (!svg_text_descendants_)
    svg_text_descendants_ = MakeGarbageCollected<TrackedDescendantsMap>();
  return *svg_text_descendants_;
}

void LayoutView::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  NOTREACHED_NORETURN();
}

void LayoutView::PaintBoxDecorationBackground(const PaintInfo& paint_info,
                                              const PhysicalOffset&) const {
  NOT_DESTROYED();
  NOTREACHED_NORETURN();
}

void LayoutView::InvalidatePaintForViewAndDescendants() {
  NOT_DESTROYED();
  SetSubtreeShouldDoFullPaintInvalidation();
}

bool LayoutView::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();

  // Apply our transform if we have one (because of full page zooming).
  if (Layer() && Layer()->Transform()) {
    transform_state.ApplyTransform(Layer()->CurrentTransform(),
                                   TransformState::kFlattenTransform);
  }

  transform_state.Flatten();

  if (ancestor == this)
    return true;

  Element* owner = GetDocument().LocalOwner();
  if (!owner) {
    PhysicalRect rect = PhysicalRect::EnclosingRect(
        transform_state.LastPlanarQuad().BoundingBox());
    bool retval = GetFrameView()->MapToVisualRectInRemoteRootFrame(
        rect, !(visual_rect_flags & kDontApplyMainFrameOverflowClip));
    transform_state.SetQuad(gfx::QuadF(gfx::RectF(rect)));
    return retval;
  }

  if (LayoutBox* obj = owner->GetLayoutBox()) {
    PhysicalRect rect = PhysicalRect::EnclosingRect(
        transform_state.LastPlanarQuad().BoundingBox());
    PhysicalRect view_rectangle = ViewRect();
    if (visual_rect_flags & kEdgeInclusive) {
      if (!rect.InclusiveIntersect(view_rectangle)) {
        transform_state.SetQuad(gfx::QuadF(gfx::RectF(rect)));
        return false;
      }
    } else {
      rect.Intersect(view_rectangle);
    }

    // Frames are painted at rounded-int position. Since we cannot efficiently
    // compute the subpixel offset of painting at this point in a a bottom-up
    // walk, round to the enclosing int rect, which will enclose the actual
    // visible rect.
    rect.ExpandEdgesToPixelBoundaries();

    // Adjust for frame border.
    rect.Move(obj->PhysicalContentBoxOffset());
    transform_state.SetQuad(gfx::QuadF(gfx::RectF(rect)));

    return obj->MapToVisualRectInAncestorSpaceInternal(
        ancestor, transform_state, visual_rect_flags);
  }

  // This can happen, e.g., if the iframe element has display:none.
  transform_state.SetQuad(gfx::QuadF(gfx::RectF()));
  return false;
}

PhysicalOffset LayoutView::OffsetForFixedPosition() const {
  NOT_DESTROYED();
  return IsScrollContainer() ? ScrolledContentOffset() : PhysicalOffset();
}

PhysicalOffset LayoutView::PixelSnappedOffsetForFixedPosition() const {
  NOT_DESTROYED();
  return PhysicalOffset(ToFlooredPoint(OffsetForFixedPosition()));
}

void LayoutView::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                               MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  quads.push_back(LocalRectToAbsoluteQuad(
      PhysicalRect(PhysicalOffset(), GetScrollableArea()->Size()), mode));
}

void LayoutView::CommitPendingSelection() {
  NOT_DESTROYED();
  TRACE_EVENT0("blink", "LayoutView::commitPendingSelection");
  DCHECK(!NeedsLayout());
  frame_view_->GetFrame().Selection().CommitAppearanceIfNeeded();
}

bool LayoutView::ShouldUsePrintingLayout(const Document& document) {
  if (!document.Printing())
    return false;
  const LocalFrameView* frame_view = document.View();
  if (!frame_view)
    return false;
  return frame_view->GetFrame().ShouldUsePrintingLayout();
}

PhysicalRect LayoutView::ViewRect() const {
  NOT_DESTROYED();
  if (ShouldUsePrintingLayout())
    return PhysicalRect(PhysicalOffset(), Size());

  if (!frame_view_)
    return PhysicalRect();

  // TODO(bokan): This shouldn't be just for the outermost main frame, we
  // should do it for all frames. crbug.com/1311518.
  if (frame_view_->GetFrame().IsOutermostMainFrame()) {
    if (auto* transition =
            ViewTransitionUtils::GetActiveTransition(GetDocument());
        transition && transition->IsRootTransitioning()) {
      // If we're capturing a transition snapshot, the root transition
      // needs to produce the snapshot at a known stable size, excluding
      // all insetting UI like mobile URL bars and virtual keyboards.

      // This adjustment should always be an expansion of the current
      // viewport.
      DCHECK_GE(transition->GetSnapshotRootSize().width(),
                frame_view_->Size().width());
      DCHECK_GE(transition->GetSnapshotRootSize().height(),
                frame_view_->Size().height());

      return PhysicalRect(
          PhysicalOffset(transition->GetFrameToSnapshotRootOffset()),
          PhysicalSize(transition->GetSnapshotRootSize()));
    }
  }

  return PhysicalRect(PhysicalOffset(), PhysicalSize(frame_view_->Size()));
}

PhysicalRect LayoutView::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  NOT_DESTROYED();
  PhysicalRect rect = ViewRect();
  if (rect.IsEmpty()) {
    return LayoutBox::OverflowClipRect(location,
                                       overlay_scrollbar_clip_behavior);
  }

  rect.offset += location;

  // When capturing the root snapshot for a transition, we paint the
  // background color where the scrollbar would be so keep the clip rect
  // the full ViewRect size.
  auto* transition = ViewTransitionUtils::GetActiveTransition(GetDocument());
  bool is_in_transition = transition && transition->IsRootTransitioning();
  if (IsScrollContainer() && !is_in_transition)
    ExcludeScrollbars(rect, overlay_scrollbar_clip_behavior);

  return rect;
}

void LayoutView::SetAutosizeScrollbarModes(mojom::blink::ScrollbarMode h_mode,
                                           mojom::blink::ScrollbarMode v_mode) {
  NOT_DESTROYED();
  DCHECK_EQ(v_mode == mojom::blink::ScrollbarMode::kAuto,
            h_mode == mojom::blink::ScrollbarMode::kAuto);
  autosize_v_scrollbar_mode_ = v_mode;
  autosize_h_scrollbar_mode_ = h_mode;
}

void LayoutView::CalculateScrollbarModes(
    mojom::blink::ScrollbarMode& h_mode,
    mojom::blink::ScrollbarMode& v_mode) const {
  NOT_DESTROYED();
#define RETURN_SCROLLBAR_MODE(mode) \
  {                                 \
    h_mode = v_mode = mode;         \
    return;                         \
  }

  // FrameViewAutoSizeInfo manually controls the appearance of the main frame's
  // scrollbars so defer to those if we're in AutoSize mode.
  if (AutosizeVerticalScrollbarMode() != mojom::blink::ScrollbarMode::kAuto ||
      AutosizeHorizontalScrollbarMode() != mojom::blink::ScrollbarMode::kAuto) {
    h_mode = AutosizeHorizontalScrollbarMode();
    v_mode = AutosizeVerticalScrollbarMode();

    if (h_mode == mojom::blink::ScrollbarMode::kAlwaysOff &&
        v_mode == mojom::blink::ScrollbarMode::kAlwaysOff) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          ScrollbarDisableReason::kAutosizeMode);
    }
    return;
  }

  LocalFrame* frame = GetFrame();
  if (!frame) {
    // GetFrame() returns null if either Document::dom_window_ or
    // DOMWindow::frame_ is null.
    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
        "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
        !GetDocument().domWindow() ? ScrollbarDisableReason::kNullDomWindow
                                   : ScrollbarDisableReason::kNullFrame);

    RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
  }

  // ClipsContent() is false means that the client wants to paint the whole
  // contents of the frame without scrollbars, which is for printing etc.
  ScrollbarDisableReason reason;
  if (!frame->ClipsContent(&reason)) {
    bool disable_scrollbars = true;
#if BUILDFLAG(IS_ANDROID)
    // However, Android WebView has a setting recordFullDocument. When it's set
    // to true, ClipsContent() is false here, while WebView still expects blink
    // to provide scrolling mechanism. The flag can be set through WebView API,
    // or is forced if the app's target SDK version < LOLLIPOP.
    // Synchronous compositing indicates Android WebView.
    if (Platform::Current()
            ->IsSynchronousCompositingEnabledForAndroidWebView() &&
        !GetDocument().IsPrintingOrPaintingPreview()) {
      disable_scrollbars = false;
    }
#endif
    if (disable_scrollbars) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          reason);
      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
    }
  }

  if (FrameOwner* owner = frame->Owner()) {
    // Setting scrolling="no" on an iframe element disables scrolling.
    if (owner->ScrollbarMode() == mojom::blink::ScrollbarMode::kAlwaysOff) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          ScrollbarDisableReason::kIframeScrollingNo);

      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
    }
  }

  Document& document = GetDocument();
  if (Node* body = document.body()) {
    // Framesets can't scroll.
    if (body->GetLayoutObject() && body->GetLayoutObject()->IsFrameSet()) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          ScrollbarDisableReason::kFrameSet);

      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
    }
  }

  if (LocalFrameView* frameView = GetFrameView()) {
    // Scrollbars can be disabled by LocalFrameView::setCanHaveScrollbars.
    if (!frameView->CanHaveScrollbars()) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          ScrollbarDisableReason::kFrameViewCanHaveScrollbarsFalse);

      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
    }
  }

  Element* viewport_defining_element = document.ViewportDefiningElement();
  if (!viewport_defining_element)
    RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAuto);

  LayoutObject* viewport = viewport_defining_element->GetLayoutObject();
  if (!viewport)
    RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAuto);

  const ComputedStyle* style = viewport->Style();
  if (!style)
    RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAuto);

  if (viewport->IsSVGRoot()) {
    // Don't allow overflow to affect <img> and css backgrounds
    if (To<LayoutSVGRoot>(viewport)->IsEmbeddedThroughSVGImage())
      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAuto);

    // FIXME: evaluate if we can allow overflow for these cases too.
    // Overflow is always hidden when stand-alone SVG documents are embedded.
    if (To<LayoutSVGRoot>(viewport)
            ->IsEmbeddedThroughFrameContainingSVGDocument()) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
          "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
          ScrollbarDisableReason::kSVGRoot);

      RETURN_SCROLLBAR_MODE(mojom::blink::ScrollbarMode::kAlwaysOff);
    }
  }

  h_mode = v_mode = mojom::blink::ScrollbarMode::kAuto;

  EOverflow overflow_x = style->OverflowX();
  EOverflow overflow_y = style->OverflowY();

  bool should_ignore_overflow_hidden = false;
  if (Settings* settings = document.GetSettings()) {
    if (settings->GetIgnoreMainFrameOverflowHiddenQuirk() &&
        frame->IsMainFrame())
      should_ignore_overflow_hidden = true;
  }
  if (!should_ignore_overflow_hidden) {
    if (overflow_x == EOverflow::kHidden || overflow_x == EOverflow::kClip)
      h_mode = mojom::blink::ScrollbarMode::kAlwaysOff;
    if (overflow_y == EOverflow::kHidden || overflow_y == EOverflow::kClip)
      v_mode = mojom::blink::ScrollbarMode::kAlwaysOff;
  }

  if (overflow_x == EOverflow::kScroll)
    h_mode = mojom::blink::ScrollbarMode::kAlwaysOn;
  if (overflow_y == EOverflow::kScroll)
    v_mode = mojom::blink::ScrollbarMode::kAlwaysOn;

  if (h_mode == mojom::blink::ScrollbarMode::kAlwaysOff &&
      v_mode == mojom::blink::ScrollbarMode::kAlwaysOff) {
    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.scrollbars"),
        "CalculateScrollbarModes", TRACE_EVENT_SCOPE_THREAD, "disable_reason",
        ScrollbarDisableReason::kOverflowHidden);
  }

#undef RETURN_SCROLLBAR_MODE
}

PhysicalRect LayoutView::DocumentRect() const {
  NOT_DESTROYED();
  return FlipForWritingMode(LayoutOverflowRect());
}

gfx::Size LayoutView::GetLayoutSize(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  NOT_DESTROYED();
  if (ShouldUsePrintingLayout())
    return ToFlooredSize(page_size_);

  if (!frame_view_)
    return gfx::Size();

  gfx::Size result = frame_view_->GetLayoutSize();
  if (scrollbar_inclusion == kExcludeScrollbars &&
      frame_view_->LayoutViewport()) {
    result = frame_view_->LayoutViewport()->ExcludeScrollbars(result);
  }
  return result;
}

int LayoutView::ViewLogicalWidth(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  NOT_DESTROYED();
  return StyleRef().IsHorizontalWritingMode() ? ViewWidth(scrollbar_inclusion)
                                              : ViewHeight(scrollbar_inclusion);
}

int LayoutView::ViewLogicalHeight(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  NOT_DESTROYED();
  return StyleRef().IsHorizontalWritingMode() ? ViewHeight(scrollbar_inclusion)
                                              : ViewWidth(scrollbar_inclusion);
}

LayoutUnit LayoutView::ViewLogicalHeightForPercentages() const {
  NOT_DESTROYED();
  if (ShouldUsePrintingLayout())
    return PageLogicalHeight();
  return LayoutUnit(ViewLogicalHeight());
}

const LayoutBox& LayoutView::RootBox() const {
  NOT_DESTROYED();
  Element* document_element = GetDocument().documentElement();
  DCHECK(document_element);
  DCHECK(document_element->GetLayoutObject());
  return To<LayoutBox>(*document_element->GetLayoutObject());
}

void LayoutView::UpdateAfterLayout() {
  NOT_DESTROYED();
  // Unlike every other layer, the root PaintLayer takes its size from the
  // layout viewport size.  The call to AdjustViewSize() will update the
  // frame's contents size, which will also update the page's minimum scale
  // factor.  The call to ResizeAfterLayout() will calculate the layout viewport
  // size based on the page minimum scale factor, and then update the
  // LocalFrameView with the new size.
  LocalFrame& frame = GetFrameView()->GetFrame();
  if (!GetDocument().Printing())
    GetFrameView()->AdjustViewSize();
  if (frame.IsMainFrame())
    frame.GetChromeClient().ResizeAfterLayout();
  if (IsScrollContainer())
    GetScrollableArea()->ClampScrollOffsetAfterOverflowChange();
  LayoutBlockFlow::UpdateAfterLayout();
}

void LayoutView::UpdateHitTestResult(HitTestResult& result,
                                     const PhysicalOffset& point) const {
  NOT_DESTROYED();
  if (result.InnerNode())
    return;

  Node* node = GetDocument().documentElement();
  if (node) {
    PhysicalOffset adjusted_point = point;
    if (const auto* layout_box = node->GetLayoutBox())
      adjusted_point -= layout_box->PhysicalLocation();
    if (IsScrollContainer()) {
      adjusted_point += PhysicalOffset(PixelSnappedScrolledContentOffset());
    }
    result.SetNodeAndPosition(node, adjusted_point);
  }
}

bool LayoutView::BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const {
  NOT_DESTROYED();
  // The base background color applies to the main frame only.
  return GetFrame()->IsMainFrame() &&
         !frame_view_->BaseBackgroundColor().HasAlpha();
}

gfx::SizeF LayoutView::ViewportSizeForViewportUnits() const {
  NOT_DESTROYED();
  return GetFrameView() ? GetFrameView()->ViewportSizeForViewportUnits()
                        : gfx::SizeF();
}

gfx::SizeF LayoutView::SmallViewportSizeForViewportUnits() const {
  NOT_DESTROYED();
  return GetFrameView() ? GetFrameView()->SmallViewportSizeForViewportUnits()
                        : gfx::SizeF();
}

gfx::SizeF LayoutView::LargeViewportSizeForViewportUnits() const {
  NOT_DESTROYED();
  return GetFrameView() ? GetFrameView()->LargeViewportSizeForViewportUnits()
                        : gfx::SizeF();
}

gfx::SizeF LayoutView::DynamicViewportSizeForViewportUnits() const {
  NOT_DESTROYED();
  return GetFrameView() ? GetFrameView()->DynamicViewportSizeForViewportUnits()
                        : gfx::SizeF();
}

void LayoutView::WillBeDestroyed() {
  NOT_DESTROYED();
  // TODO(wangxianzhu): This is a workaround of crbug.com/570706.
  // Should find and fix the root cause.
  if (PaintLayer* layer = Layer())
    layer->SetNeedsRepaint();
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutView::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutBlockFlow::UpdateFromStyle();

  // LayoutView of the main frame is responsible for painting base background.
  if (GetFrameView()->ShouldPaintBaseBackgroundColor())
    SetHasBoxDecorationBackground(true);
}

void LayoutView::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  LocalFrame& frame = GetFrameView()->GetFrame();
  VisualViewport& visual_viewport = frame.GetPage()->GetVisualViewport();
  if (frame.IsMainFrame() && visual_viewport.IsActiveViewport()) {
    // |VisualViewport::UsedColorScheme| depends on the LayoutView's used
    // color scheme.
    if (!old_style || old_style->UsedColorScheme() !=
                          visual_viewport.UsedColorSchemeScrollbars()) {
      visual_viewport.UsedColorSchemeChanged();
    }
  }
}

RecalcLayoutOverflowResult LayoutView::RecalcLayoutOverflow() {
  NOT_DESTROYED();
  if (!NeedsLayoutOverflowRecalc())
    return RecalcLayoutOverflowResult();

  auto result = LayoutBlockFlow::RecalcLayoutOverflow();
  if (result.layout_overflow_changed) {
    if (NeedsLayout())
      return result;
    if (GetFrameView()->VisualViewportSuppliesScrollbars())
      SetShouldCheckForPaintInvalidation();
    GetFrameView()->AdjustViewSize();
    SetNeedsPaintPropertyUpdate();
  }
  return result;
}

PhysicalRect LayoutView::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect(gfx::Rect(0, 0, ViewWidth(kIncludeScrollbars),
                                ViewHeight(kIncludeScrollbars)));
}

CompositingReasons LayoutView::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  // TODO(lfg): Audit for portals
  const LocalFrame& frame = frame_view_->GetFrame();
  if (frame.OwnerLayoutObject() && frame.IsCrossOriginToParentOrOuterDocument())
    return CompositingReason::kIFrame;
  return CompositingReason::kNone;
}

void LayoutView::UpdateMarkersAndCountersAfterStyleChange(
    LayoutObject* container) {
  NOT_DESTROYED();
  if (!needs_marker_counter_update_)
    return;

  DCHECK(!container ||
         (container->View() == this && container->IsDescendantOf(this) &&
          GetDocument().GetStyleEngine().InContainerQueryStyleRecalc()))
      << "The container parameter is currently only for scoping updates for "
         "container query style recalcs";

  needs_marker_counter_update_ = false;
  if (!HasLayoutCounters() && !HasLayoutListItems())
    return;

  // For container queries style recalc, we know the counter styles didn't
  // change outside the container. Hence, we can start the update traversal from
  // the container.
  LayoutObject* start = container ? container : this;
  // Additionally, if the container contains style, we know counters inside the
  // container cannot affect counters outside the container, which means we can
  // limit the traversal to the container subtree.
  LayoutObject* stay_within =
      container && container->ShouldApplyStyleContainment() ? container
                                                            : nullptr;

  for (LayoutObject* layout_object = start; layout_object;
       layout_object = layout_object->NextInPreOrder(stay_within)) {
    if (auto* ng_list_item = DynamicTo<LayoutNGListItem>(layout_object)) {
      ng_list_item->UpdateCounterStyle();
    } else if (auto* inline_list_item =
                   DynamicTo<LayoutNGInlineListItem>(layout_object)) {
      inline_list_item->UpdateCounterStyle();
    } else if (auto* counter = DynamicTo<LayoutCounter>(layout_object)) {
      counter->UpdateCounter();
    }
  }
}

bool LayoutView::HasTickmarks() const {
  NOT_DESTROYED();
  return GetDocument().Markers().PossiblyHasTextMatchMarkers();
}

Vector<gfx::Rect> LayoutView::GetTickmarks() const {
  NOT_DESTROYED();
  return GetDocument().Markers().LayoutRectsForTextMatchMarkers();
}

}  // namespace blink

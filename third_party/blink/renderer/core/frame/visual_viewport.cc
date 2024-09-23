/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/visual_viewport.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

OverscrollType ComputeOverscrollType() {
  if (!Platform::Current()->IsElasticOverscrollEnabled())
    return OverscrollType::kNone;
  return OverscrollType::kTransform;
}

}  // anonymous namespace

VisualViewport::VisualViewport(Page& owner)
    : ScrollableArea(owner.GetAgentGroupScheduler().CompositorTaskRunner()),
      page_(&owner),
      scale_(1),
      is_pinch_gesture_active_(false),
      browser_controls_adjustment_(0),
      needs_paint_property_update_(true),
      overscroll_type_(ComputeOverscrollType()) {
  UniqueObjectId unique_id = NewUniqueObjectId();
  page_scale_element_id_ = CompositorElementIdFromUniqueObjectId(
      unique_id, CompositorElementIdNamespace::kPrimary);
  scroll_element_id_ = CompositorElementIdFromUniqueObjectId(
      unique_id, CompositorElementIdNamespace::kScroll);
  Reset();
}

const TransformPaintPropertyNode*
VisualViewport::GetDeviceEmulationTransformNode() const {
  return device_emulation_transform_node_.Get();
}

const TransformPaintPropertyNode*
VisualViewport::GetOverscrollElasticityTransformNode() const {
  return overscroll_elasticity_transform_node_.Get();
}

const TransformPaintPropertyNode* VisualViewport::GetPageScaleNode() const {
  return page_scale_node_.Get();
}

const TransformPaintPropertyNode* VisualViewport::GetScrollTranslationNode()
    const {
  return scroll_translation_node_.Get();
}

const ScrollPaintPropertyNode* VisualViewport::GetScrollNode() const {
  return scroll_node_.Get();
}

const TransformPaintPropertyNode*
VisualViewport::TransformNodeForViewportScrollbars() const {
  // Viewport scrollbars don't move with elastic overscroll or scale with
  // page scale.
  if (overscroll_elasticity_transform_node_)
    return overscroll_elasticity_transform_node_->UnaliasedParent();
  if (page_scale_node_)
    return page_scale_node_->UnaliasedParent();
  return nullptr;
}

PaintPropertyChangeType VisualViewport::UpdatePaintPropertyNodesIfNeeded(
    PaintPropertyTreeBuilderFragmentContext& context) {
  DCHECK(IsActiveViewport());
  PaintPropertyChangeType change = PaintPropertyChangeType::kUnchanged;

  if (!scroll_layer_)
    CreateLayers();

  if (!needs_paint_property_update_)
    return change;

  needs_paint_property_update_ = false;

  auto* transform_parent = context.current.transform;
  auto* scroll_parent = context.current.scroll;
  auto* clip_parent = context.current.clip;
  auto* effect_parent = context.current_effect;

  DCHECK(transform_parent);
  DCHECK(scroll_parent);
  DCHECK(clip_parent);
  DCHECK(effect_parent);

  {
    const auto& device_emulation_transform =
        GetChromeClient()->GetDeviceEmulationTransform();
    if (!device_emulation_transform.IsIdentity()) {
      TransformPaintPropertyNode::State state{{device_emulation_transform}};
      state.in_subtree_of_page_scale = false;
      if (!device_emulation_transform_node_) {
        device_emulation_transform_node_ = TransformPaintPropertyNode::Create(
            *transform_parent, std::move(state));
        change = PaintPropertyChangeType::kNodeAddedOrRemoved;
      } else {
        change = std::max(change, device_emulation_transform_node_->Update(
                                      *transform_parent, std::move(state)));
      }
      transform_parent = device_emulation_transform_node_.Get();
    } else if (device_emulation_transform_node_) {
      device_emulation_transform_node_ = nullptr;
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }

  if (overscroll_type_ == OverscrollType::kTransform) {
    DCHECK(!transform_parent->Unalias().IsInSubtreeOfPageScale());

    TransformPaintPropertyNode::State state;
    state.in_subtree_of_page_scale = false;
    // TODO(crbug.com/877794) Should create overscroll elasticity transform node
    // based on settings.
    if (!overscroll_elasticity_transform_node_) {
      overscroll_elasticity_transform_node_ =
          TransformPaintPropertyNode::Create(*transform_parent,
                                             std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, overscroll_elasticity_transform_node_->Update(
                                    *transform_parent, std::move(state)));
    }
  } else {
    DCHECK(!overscroll_elasticity_transform_node_);
  }

  {
    auto* parent = overscroll_elasticity_transform_node_
                       ? overscroll_elasticity_transform_node_.Get()
                       : transform_parent;
    DCHECK(!parent->Unalias().IsInSubtreeOfPageScale());

    TransformPaintPropertyNode::State state;
    if (scale_ != 1.f)
      state.transform_and_origin.matrix = gfx::Transform::MakeScale(scale_);
    state.in_subtree_of_page_scale = false;
    state.direct_compositing_reasons = CompositingReason::kViewport;
    state.compositor_element_id = page_scale_element_id_;

    if (!page_scale_node_) {
      page_scale_node_ =
          TransformPaintPropertyNode::Create(*parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      auto effective_change_type =
          page_scale_node_->Update(*parent, std::move(state));
      // As an optimization, attempt to directly update the compositor
      // scale translation node and return kChangedOnlyCompositedValues which
      // avoids an expensive PaintArtifactCompositor update.
      if (effective_change_type ==
          PaintPropertyChangeType::kChangedOnlySimpleValues) {
        if (auto* paint_artifact_compositor = GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdatePageScaleTransform(
                  *page_scale_node_);
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            page_scale_node_->CompositorSimpleValuesUpdated();
          }
        }
      }
      change = std::max(change, effective_change_type);
    }
  }

  {
    ScrollPaintPropertyNode::State state;
    state.container_rect = gfx::Rect(size_);
    state.contents_size = ContentsSize();

    state.user_scrollable_horizontal =
        UserInputScrollable(kHorizontalScrollbar);
    state.user_scrollable_vertical = UserInputScrollable(kVerticalScrollbar);
    state.max_scroll_offset_affected_by_page_scale = true;
    state.compositor_element_id = GetScrollElementId();

    if (IsActiveViewport()) {
      if (const Document* document = LocalMainFrame().GetDocument()) {
        bool uses_default_root_scroller =
            &document->GetRootScrollerController().EffectiveRootScroller() ==
            document;

        // All position: fixed elements will chain scrolling directly up to the
        // visual viewport's scroll node. In the case of a default root scroller
        // (i.e. the LayoutView), we actually want to scroll the "full
        // viewport". i.e. scrolling from the position: fixed element should
        // cause the page to scroll. This is not the case when we have a
        // different root scroller. We set
        // |prevent_viewport_scrolling_from_inner| so the compositor can know to
        // use the correct chaining behavior. This would be better fixed by
        // setting the correct scroll_tree_index in PAC::Update on the fixed
        // layer but that's a larger change. See https://crbug.com/977954 for
        // details.
        state.prevent_viewport_scrolling_from_inner =
            !uses_default_root_scroller;
      }
    }

    if (!scroll_node_) {
      scroll_node_ =
          ScrollPaintPropertyNode::Create(*scroll_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change,
                        scroll_node_->Update(*scroll_parent, std::move(state)));
    }
  }

  {
    TransformPaintPropertyNode::State state{
        {gfx::Transform::MakeTranslation(-offset_)}};
    state.scroll = scroll_node_;
    state.direct_compositing_reasons = CompositingReason::kViewport;
    if (!scroll_translation_node_) {
      scroll_translation_node_ = TransformPaintPropertyNode::Create(
          *page_scale_node_, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      auto effective_change_type =
          scroll_translation_node_->Update(*page_scale_node_, std::move(state));
      // As an optimization, attempt to directly update the compositor
      // translation node and return kChangedOnlyCompositedValues which avoids
      // an expensive PaintArtifactCompositor update.
      if (effective_change_type ==
          PaintPropertyChangeType::kChangedOnlySimpleValues) {
        if (auto* paint_artifact_compositor = GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdateScrollOffsetTransform(
                  *scroll_translation_node_);
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            scroll_translation_node_->CompositorSimpleValuesUpdated();
          }
        }
      }
    }
  }

  if (scrollbar_layer_horizontal_) {
    EffectPaintPropertyNode::State state;
    state.local_transform_space = transform_parent;
    state.direct_compositing_reasons =
        CompositingReason::kActiveOpacityAnimation;
    state.compositor_element_id =
        GetScrollbarElementId(ScrollbarOrientation::kHorizontalScrollbar);
    if (!horizontal_scrollbar_effect_node_) {
      horizontal_scrollbar_effect_node_ =
          EffectPaintPropertyNode::Create(*effect_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, horizontal_scrollbar_effect_node_->Update(
                                    *effect_parent, std::move(state)));
    }
  }

  if (scrollbar_layer_vertical_) {
    EffectPaintPropertyNode::State state;
    state.local_transform_space = transform_parent;
    state.direct_compositing_reasons =
        CompositingReason::kActiveOpacityAnimation;
    state.compositor_element_id =
        GetScrollbarElementId(ScrollbarOrientation::kVerticalScrollbar);
    if (!vertical_scrollbar_effect_node_) {
      vertical_scrollbar_effect_node_ =
          EffectPaintPropertyNode::Create(*effect_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, vertical_scrollbar_effect_node_->Update(
                                    *effect_parent, std::move(state)));
    }
  }

  parent_property_tree_state_ = TraceablePropertyTreeStateOrAlias(
      *transform_parent, *clip_parent, *effect_parent);

  if (change == PaintPropertyChangeType::kNodeAddedOrRemoved &&
      IsActiveViewport()) {
    DCHECK(LocalMainFrame().View());
    LocalMainFrame().View()->SetVisualViewportOrOverlayNeedsRepaint();
  }

  return change;
}

VisualViewport::~VisualViewport() = default;

void VisualViewport::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(parent_property_tree_state_);
  visitor->Trace(device_emulation_transform_node_);
  visitor->Trace(overscroll_elasticity_transform_node_);
  visitor->Trace(page_scale_node_);
  visitor->Trace(scroll_translation_node_);
  visitor->Trace(scroll_node_);
  visitor->Trace(horizontal_scrollbar_effect_node_);
  visitor->Trace(vertical_scrollbar_effect_node_);
  ScrollableArea::Trace(visitor);
}

void VisualViewport::EnqueueScrollEvent() {
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->EnqueueVisualViewportScrollEvent();
}

void VisualViewport::EnqueueResizeEvent() {
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->EnqueueVisualViewportResizeEvent();
}

void VisualViewport::SetSize(const gfx::Size& size) {
  if (size_ == size)
    return;

  TRACE_EVENT2("blink", "VisualViewport::setSize", "width", size.width(),
               "height", size.height());
  size_ = size;

  TRACE_EVENT_INSTANT1("loading", "viewport", TRACE_EVENT_SCOPE_THREAD, "data",
                       ViewportToTracedValue());

  if (!IsActiveViewport())
    return;

  needs_paint_property_update_ = true;

  // Need to re-compute sizes for the overlay scrollbars.
  if (scrollbar_layer_horizontal_ && LocalMainFrame().View()) {
    DCHECK(scrollbar_layer_vertical_);
    UpdateScrollbarLayer(kHorizontalScrollbar);
    UpdateScrollbarLayer(kVerticalScrollbar);
    LocalMainFrame().View()->SetVisualViewportOrOverlayNeedsRepaint();
  }

  EnqueueResizeEvent();
}

void VisualViewport::Reset() {
  SetScaleAndLocation(1, is_pinch_gesture_active_, gfx::PointF());
}

void VisualViewport::MainFrameDidChangeSize() {
  if (!IsActiveViewport())
    return;

  TRACE_EVENT0("blink", "VisualViewport::mainFrameDidChangeSize");

  // In unit tests we may not have initialized the layer tree.
  if (scroll_layer_)
    scroll_layer_->SetBounds(ContentsSize());

  needs_paint_property_update_ = true;
  ClampToBoundaries();
}

gfx::RectF VisualViewport::VisibleRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  if (!IsActiveViewport())
    return gfx::RectF(gfx::PointF(), gfx::SizeF(size_));

  gfx::SizeF visible_size(size_);

  if (scrollbar_inclusion == kExcludeScrollbars)
    visible_size = gfx::SizeF(ExcludeScrollbars(size_));

  visible_size.Enlarge(0, browser_controls_adjustment_);
  visible_size.Scale(1 / scale_);

  return gfx::RectF(ScrollPosition(), visible_size);
}

gfx::PointF VisualViewport::ViewportCSSPixelsToRootFrame(
    const gfx::PointF& point) const {
  // Note, this is in CSS Pixels so we don't apply scale.
  gfx::PointF point_in_root_frame = point;
  point_in_root_frame += GetScrollOffset();
  return point_in_root_frame;
}

void VisualViewport::SetLocation(const gfx::PointF& new_location) {
  SetScaleAndLocation(scale_, is_pinch_gesture_active_, new_location);
}

void VisualViewport::Move(const ScrollOffset& delta) {
  SetLocation(gfx::PointAtOffsetFromOrigin(offset_ + delta));
}

void VisualViewport::SetScale(float scale) {
  SetScaleAndLocation(scale, is_pinch_gesture_active_,
                      gfx::PointAtOffsetFromOrigin(offset_));
}

double VisualViewport::OffsetLeft() const {
  // Offset{Left|Top} and Width|Height are used by the DOMVisualViewport to
  // expose values to JS. We'll only ever ask the visual viewport for these
  // values for the outermost main frame. All other cases are based on layout
  // of subframes.
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  return VisibleRect().x() / LocalMainFrame().LayoutZoomFactor();
}

double VisualViewport::OffsetTop() const {
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  return VisibleRect().y() / LocalMainFrame().LayoutZoomFactor();
}

double VisualViewport::Width() const {
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  return VisibleWidthCSSPx();
}

double VisualViewport::Height() const {
  DCHECK(IsActiveViewport());
  if (Document* document = LocalMainFrame().GetDocument())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  return VisibleHeightCSSPx();
}

double VisualViewport::ScaleForVisualViewport() const {
  return Scale();
}

void VisualViewport::SetScaleAndLocation(float scale,
                                         bool is_pinch_gesture_active,
                                         const gfx::PointF& location) {
  if (DidSetScaleOrLocation(scale, is_pinch_gesture_active, location)) {
    // In remote or nested main frame cases, the visual viewport is inert so it
    // cannot be moved or scaled. This is enforced by setting page scale
    // constraints.
    DCHECK(IsActiveViewport());
    NotifyRootFrameViewport();
  }
}

double VisualViewport::VisibleWidthCSSPx() const {
  if (!IsActiveViewport())
    return VisibleRect().width();

  float zoom = LocalMainFrame().LayoutZoomFactor();
  float width_css_px = VisibleRect().width() / zoom;
  return width_css_px;
}

double VisualViewport::VisibleHeightCSSPx() const {
  if (!IsActiveViewport())
    return VisibleRect().height();

  float zoom = LocalMainFrame().LayoutZoomFactor();
  float height_css_px = VisibleRect().height() / zoom;
  return height_css_px;
}

bool VisualViewport::DidSetScaleOrLocation(float scale,
                                           bool is_pinch_gesture_active,
                                           const gfx::PointF& location) {
  if (!IsActiveViewport()) {
    is_pinch_gesture_active_ = is_pinch_gesture_active;
    // The VisualViewport in an embedded widget must always be 1.0 or else
    // event targeting will fail.
    DCHECK(scale == 1.f);
    scale_ = scale;
    offset_ = ScrollOffset();
    return false;
  }

  bool values_changed = false;

  bool notify_page_scale_factor_changed =
      is_pinch_gesture_active_ != is_pinch_gesture_active;
  is_pinch_gesture_active_ = is_pinch_gesture_active;
  if (std::isfinite(scale)) {
    float clamped_scale = GetPage()
                              .GetPageScaleConstraintsSet()
                              .FinalConstraints()
                              .ClampToConstraints(scale);
    if (clamped_scale != scale_) {
      scale_ = clamped_scale;
      values_changed = true;
      notify_page_scale_factor_changed = true;
      EnqueueResizeEvent();
    }
  }
  if (notify_page_scale_factor_changed)
    GetPage().GetChromeClient().PageScaleFactorChanged();

  ScrollOffset clamped_offset = ClampScrollOffset(location.OffsetFromOrigin());

  // TODO(bokan): If the offset is invalid, we might end up in an infinite
  // recursion as we reenter this function on clamping. It would be cleaner to
  // avoid reentrancy but for now just prevent the stack overflow.
  // crbug.com/702771.
  if (!std::isfinite(clamped_offset.x()) ||
      !std::isfinite(clamped_offset.y())) {
    return false;
  }

  if (clamped_offset != offset_) {
    DCHECK(LocalMainFrame().View());

    offset_ = clamped_offset;
    GetScrollAnimator().SetCurrentOffset(offset_);

    // SVG runs with accelerated compositing disabled so no
    // ScrollingCoordinator.
    if (auto* coordinator = GetPage().GetScrollingCoordinator()) {
      if (scroll_layer_)
        coordinator->UpdateCompositorScrollOffset(LocalMainFrame(), *this);
    }

    EnqueueScrollEvent();

    LocalMainFrame().View()->DidChangeScrollOffset();
    values_changed = true;
  }

  if (!values_changed)
    return false;

  probe::DidChangeViewport(&LocalMainFrame());
  LocalMainFrame().Loader().SaveScrollState();

  ClampToBoundaries();

  needs_paint_property_update_ = true;
  if (notify_page_scale_factor_changed) {
    TRACE_EVENT_INSTANT1("loading", "viewport", TRACE_EVENT_SCOPE_THREAD,
                         "data", ViewportToTracedValue());
  }
  return true;
}

void VisualViewport::CreateLayers() {
  DCHECK(IsActiveViewport());

  if (scroll_layer_)
    return;

  if (!GetPage().GetSettings().GetAcceleratedCompositingEnabled())
    return;

  DCHECK(!scrollbar_layer_horizontal_);
  DCHECK(!scrollbar_layer_vertical_);

  needs_paint_property_update_ = true;

  scroll_layer_ = cc::Layer::Create();
  scroll_layer_->SetBounds(ContentsSize());
  scroll_layer_->SetElementId(GetScrollElementId());

  InitializeScrollbars();

  if (IsActiveViewport()) {
    ScrollingCoordinator* coordinator = GetPage().GetScrollingCoordinator();
    DCHECK(coordinator);
    coordinator->UpdateCompositorScrollOffset(LocalMainFrame(), *this);
  }
}

void VisualViewport::InitializeScrollbars() {
  DCHECK(IsActiveViewport());
  // Do nothing if we haven't created the layer tree yet.
  if (!scroll_layer_)
    return;

  needs_paint_property_update_ = true;

  scrollbar_layer_horizontal_ = nullptr;
  scrollbar_layer_vertical_ = nullptr;
  if (VisualViewportSuppliesScrollbars() &&
      !GetPage().GetSettings().GetHideScrollbars()) {
    UpdateScrollbarLayer(kHorizontalScrollbar);
    UpdateScrollbarLayer(kVerticalScrollbar);
  }

  // Ensure existing LocalFrameView scrollbars are removed if the visual
  // viewport scrollbars are now supplied, or created if the visual viewport no
  // longer supplies scrollbars.
  if (IsActiveViewport()) {
    if (LocalFrameView* frame_view = LocalMainFrame().View())
      frame_view->SetVisualViewportOrOverlayNeedsRepaint();
  }
}

EScrollbarWidth VisualViewport::CSSScrollbarWidth() const {
  DCHECK(IsActiveViewport());
  if (Document* main_document = LocalMainFrame().GetDocument())
    return main_document->GetLayoutView()->StyleRef().UsedScrollbarWidth();

  return EScrollbarWidth::kAuto;
}

std::optional<blink::Color> VisualViewport::CSSScrollbarThumbColor() const {
  DCHECK(IsActiveViewport());
  if (Document* main_document = LocalMainFrame().GetDocument()) {
    return main_document->GetLayoutView()
        ->StyleRef()
        .ScrollbarThumbColorResolved();
  }

  return std::nullopt;
}

void VisualViewport::DropCompositorScrollDeltaNextCommit() {
  if (auto* paint_artifact_compositor = GetPaintArtifactCompositor()) {
    paint_artifact_compositor->DropCompositorScrollDeltaNextCommit(
        scroll_element_id_);
  }
}

int VisualViewport::ScrollbarThickness() const {
  DCHECK(IsActiveViewport());
  return ScrollbarThemeOverlayMobile::GetInstance().ScrollbarThickness(
      ScaleFromDIP(), CSSScrollbarWidth());
}

void VisualViewport::UpdateScrollbarLayer(ScrollbarOrientation orientation) {
  DCHECK(IsActiveViewport());
  bool is_horizontal = orientation == kHorizontalScrollbar;
  scoped_refptr<cc::SolidColorScrollbarLayer>& scrollbar_layer =
      is_horizontal ? scrollbar_layer_horizontal_ : scrollbar_layer_vertical_;
  if (!scrollbar_layer) {
    auto& theme = ScrollbarThemeOverlayMobile::GetInstance();
    float scale = ScaleFromDIP();
    int thumb_thickness = theme.ThumbThickness(scale, CSSScrollbarWidth());
    int scrollbar_margin = theme.ScrollbarMargin(scale, CSSScrollbarWidth());
    cc::ScrollbarOrientation cc_orientation =
        orientation == kHorizontalScrollbar
            ? cc::ScrollbarOrientation::kHorizontal
            : cc::ScrollbarOrientation::kVertical;
    scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
        cc_orientation, thumb_thickness, scrollbar_margin,
        /*is_left_side_vertical_scrollbar*/ false);
    scrollbar_layer->SetElementId(GetScrollbarElementId(orientation));
    scrollbar_layer->SetScrollElementId(scroll_layer_->element_id());
    scrollbar_layer->SetIsDrawable(true);
  }

  scrollbar_layer->SetBounds(
      orientation == kHorizontalScrollbar
          ? gfx::Size(size_.width() - ScrollbarThickness(),
                      ScrollbarThickness())
          : gfx::Size(ScrollbarThickness(),
                      size_.height() - ScrollbarThickness()));

  UpdateScrollbarColor(*scrollbar_layer);
}

bool VisualViewport::VisualViewportSuppliesScrollbars() const {
  return IsActiveViewport() && GetPage().GetSettings().GetViewportEnabled();
}

const Document* VisualViewport::GetDocument() const {
  return IsActiveViewport() ? LocalMainFrame().GetDocument() : nullptr;
}

CompositorElementId VisualViewport::GetScrollElementId() const {
  return scroll_element_id_;
}

bool VisualViewport::ScrollAnimatorEnabled() const {
  return GetPage().GetSettings().GetScrollAnimatorEnabled();
}

ChromeClient* VisualViewport::GetChromeClient() const {
  return &GetPage().GetChromeClient();
}

SmoothScrollSequencer* VisualViewport::GetSmoothScrollSequencer() const {
  if (!IsActiveViewport())
    return nullptr;
  return LocalMainFrame().GetSmoothScrollSequencer();
}

bool VisualViewport::SetScrollOffset(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type,
    mojom::blink::ScrollBehavior scroll_behavior,
    ScrollCallback on_finish) {
  // We clamp the offset here, because the ScrollAnimator may otherwise be
  // set to a non-clamped offset by ScrollableArea::setScrollOffset,
  // which may lead to incorrect scrolling behavior in RootFrameViewport down
  // the line.
  // TODO(eseckler): Solve this instead by ensuring that ScrollableArea and
  // ScrollAnimator are kept in sync. This requires that ScrollableArea always
  // stores fractional offsets and that truncation happens elsewhere, see
  // crbug.com/626315.
  ScrollOffset new_scroll_offset = ClampScrollOffset(offset);
  return ScrollableArea::SetScrollOffset(new_scroll_offset, scroll_type,
                                         scroll_behavior, std::move(on_finish));
}

bool VisualViewport::SetScrollOffset(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type,
    mojom::blink::ScrollBehavior scroll_behavior) {
  return SetScrollOffset(offset, scroll_type, scroll_behavior,
                         ScrollCallback());
}

PhysicalOffset VisualViewport::LocalToScrollOriginOffset() const {
  return {};
}

PhysicalRect VisualViewport::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const PhysicalBoxStrut& scroll_margin,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  if (!IsActiveViewport())
    return rect_in_absolute;

  ScrollOffset new_scroll_offset =
      ClampScrollOffset(scroll_into_view_util::GetScrollOffsetToExpose(
          *this, rect_in_absolute, scroll_margin, *params->align_x.get(),
          *params->align_y.get()));

  if (new_scroll_offset != GetScrollOffset()) {
    if (params->is_for_scroll_sequence) {
      if (RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled()) {
        SetScrollOffset(new_scroll_offset, params->type, params->behavior);
      } else {
        DCHECK(params->type == mojom::blink::ScrollType::kProgrammatic ||
               params->type == mojom::blink::ScrollType::kUser);
        CHECK(GetSmoothScrollSequencer());
        GetSmoothScrollSequencer()->QueueAnimation(this, new_scroll_offset,
                                                   params->behavior);
      }
    } else {
      SetScrollOffset(new_scroll_offset, params->type, params->behavior,
                      ScrollCallback());
    }
  }

  return rect_in_absolute;
}

int VisualViewport::ScrollSize(ScrollbarOrientation orientation) const {
  gfx::Vector2d scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.x()
                                               : scroll_dimensions.y();
}

gfx::Vector2d VisualViewport::MinimumScrollOffsetInt() const {
  return gfx::Vector2d();
}

gfx::Vector2d VisualViewport::MaximumScrollOffsetInt() const {
  return gfx::ToFlooredVector2d(MaximumScrollOffset());
}

ScrollOffset VisualViewport::MaximumScrollOffset() const {
  return MaximumScrollOffsetAtScale(scale_);
}

ScrollOffset VisualViewport::MaximumScrollOffsetAtScale(float scale) const {
  if (!IsActiveViewport())
    return ScrollOffset();

  // TODO(bokan): We probably shouldn't be storing the bounds in a float.
  // crbug.com/470718.
  gfx::SizeF frame_view_size(ContentsSize());

  if (browser_controls_adjustment_) {
    float min_scale =
        GetPage().GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
    frame_view_size.Enlarge(0, browser_controls_adjustment_ / min_scale);
  }

  frame_view_size.Scale(scale);
  frame_view_size = gfx::SizeF(ToFlooredSize(frame_view_size));

  gfx::SizeF viewport_size(size_);
  viewport_size.Enlarge(0, ceilf(browser_controls_adjustment_));

  gfx::SizeF max_position = frame_view_size - viewport_size;
  max_position.Scale(1 / scale);
  return ScrollOffset(max_position.width(), max_position.height());
}

gfx::Point VisualViewport::ClampDocumentOffsetAtScale(const gfx::Point& offset,
                                                      float scale) {
  DCHECK(IsActiveViewport());

  LocalFrameView* view = LocalMainFrame().View();
  if (!view)
    return gfx::Point();

  gfx::SizeF scaled_size(ExcludeScrollbars(size_));
  scaled_size.Scale(1 / scale);

  gfx::Size visual_viewport_max =
      gfx::ToFlooredSize(gfx::SizeF(ContentsSize()) - scaled_size);
  gfx::Vector2d max =
      view->LayoutViewport()->MaximumScrollOffsetInt() +
      gfx::Vector2d(visual_viewport_max.width(), visual_viewport_max.height());
  gfx::Vector2d min =
      view->LayoutViewport()
          ->MinimumScrollOffsetInt();  // VisualViewportMin should be (0, 0)

  gfx::Point clamped = offset;
  clamped.SetToMin(gfx::PointAtOffsetFromOrigin(max));
  clamped.SetToMax(gfx::PointAtOffsetFromOrigin(min));
  return clamped;
}

void VisualViewport::SetBrowserControlsAdjustment(float adjustment) {
  DCHECK(IsActiveViewport());
  DCHECK(LocalMainFrame().IsOutermostMainFrame());

  if (browser_controls_adjustment_ == adjustment)
    return;

  browser_controls_adjustment_ = adjustment;
  EnqueueResizeEvent();
}

float VisualViewport::BrowserControlsAdjustment() const {
  DCHECK(!browser_controls_adjustment_ || IsActiveViewport());
  return browser_controls_adjustment_;
}

bool VisualViewport::UserInputScrollable(ScrollbarOrientation) const {
  // User input scrollable is used to block scrolling from the visual viewport.
  // If the viewport isn't active we don't have to do anything special.
  if (!IsActiveViewport())
    return true;

  // If there is a non-root fullscreen element, prevent the viewport from
  // scrolling.
  if (Document* main_document = LocalMainFrame().GetDocument()) {
    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(*main_document);
    if (fullscreen_element)
      return false;
  }
  return true;
}

gfx::Size VisualViewport::ContentsSize() const {
  if (!IsActiveViewport())
    return gfx::Size();

  LocalFrameView* frame_view = LocalMainFrame().View();
  if (!frame_view)
    return gfx::Size();

  return frame_view->Size();
}

gfx::Rect VisualViewport::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  return ToEnclosingRect(VisibleRect(scrollbar_inclusion));
}

scoped_refptr<base::SingleThreadTaskRunner> VisualViewport::GetTimerTaskRunner()
    const {
  DCHECK(IsActiveViewport());
  return LocalMainFrame().GetTaskRunner(TaskType::kInternalDefault);
}

mojom::blink::ColorScheme VisualViewport::UsedColorSchemeScrollbars() const {
  DCHECK(IsActiveViewport());
  if (Document* main_document = LocalMainFrame().GetDocument())
    return main_document->GetLayoutView()->StyleRef().UsedColorScheme();

  return mojom::blink::ColorScheme::kLight;
}

void VisualViewport::UpdateScrollOffset(const ScrollOffset& position,
                                        mojom::blink::ScrollType scroll_type) {
  if (!DidSetScaleOrLocation(scale_, is_pinch_gesture_active_,
                             gfx::PointAtOffsetFromOrigin(position))) {
    return;
  }
  if (IsExplicitScrollType(scroll_type))
    NotifyRootFrameViewport();
}

cc::Layer* VisualViewport::LayerForScrolling() const {
  DCHECK(!scroll_layer_ || IsActiveViewport());
  return scroll_layer_.get();
}

cc::Layer* VisualViewport::LayerForHorizontalScrollbar() const {
  DCHECK(!scrollbar_layer_horizontal_ || IsActiveViewport());
  return scrollbar_layer_horizontal_.get();
}

cc::Layer* VisualViewport::LayerForVerticalScrollbar() const {
  DCHECK(!scrollbar_layer_vertical_ || IsActiveViewport());
  return scrollbar_layer_vertical_.get();
}

RootFrameViewport* VisualViewport::GetRootFrameViewport() const {
  if (!IsActiveViewport())
    return nullptr;

  LocalFrameView* frame_view = LocalMainFrame().View();
  if (!frame_view)
    return nullptr;

  return frame_view->GetRootFrameViewport();
}

bool VisualViewport::IsActiveViewport() const {
  Frame* main_frame = GetPage().MainFrame();
  if (!main_frame)
    return false;

  // If the main frame is remote, we're inside a remote subframe which
  // shouldn't have an active visual viewport.
  if (!main_frame->IsLocalFrame())
    return false;

  // Only the outermost main frame should have an active viewport.
  return main_frame->IsOutermostMainFrame();
}

LocalFrame& VisualViewport::LocalMainFrame() const {
  DCHECK(IsActiveViewport());
  return *To<LocalFrame>(GetPage().MainFrame());
}

gfx::Size VisualViewport::ExcludeScrollbars(const gfx::Size& size) const {
  if (!IsActiveViewport())
    return size;

  gfx::Size excluded_size = size;
  if (RootFrameViewport* root_frame_viewport = GetRootFrameViewport()) {
    excluded_size.Enlarge(-root_frame_viewport->VerticalScrollbarWidth(),
                          -root_frame_viewport->HorizontalScrollbarHeight());
  }
  return excluded_size;
}

bool VisualViewport::ScheduleAnimation() {
  DCHECK(IsActiveViewport());

  LocalFrameView* frame_view = LocalMainFrame().View();
  DCHECK(frame_view);
  GetPage().GetChromeClient().ScheduleAnimation(frame_view);
  return true;
}

void VisualViewport::ClampToBoundaries() {
  SetLocation(gfx::PointAtOffsetFromOrigin(offset_));
}

gfx::RectF VisualViewport::ViewportToRootFrame(
    const gfx::RectF& rect_in_viewport) const {
  gfx::RectF rect_in_root_frame = rect_in_viewport;
  rect_in_root_frame.Scale(1 / Scale());
  rect_in_root_frame.Offset(GetScrollOffset());
  return rect_in_root_frame;
}

gfx::Rect VisualViewport::ViewportToRootFrame(
    const gfx::Rect& rect_in_viewport) const {
  // FIXME: How to snap to pixels?
  return ToEnclosingRect(ViewportToRootFrame(gfx::RectF(rect_in_viewport)));
}

gfx::RectF VisualViewport::RootFrameToViewport(
    const gfx::RectF& rect_in_root_frame) const {
  gfx::RectF rect_in_viewport = rect_in_root_frame;
  rect_in_viewport.Offset(-GetScrollOffset());
  rect_in_viewport.Scale(Scale());
  return rect_in_viewport;
}

gfx::Rect VisualViewport::RootFrameToViewport(
    const gfx::Rect& rect_in_root_frame) const {
  // FIXME: How to snap to pixels?
  return ToEnclosingRect(RootFrameToViewport(gfx::RectF(rect_in_root_frame)));
}

gfx::PointF VisualViewport::ViewportToRootFrame(
    const gfx::PointF& point_in_viewport) const {
  gfx::PointF point_in_root_frame = point_in_viewport;
  point_in_root_frame.Scale(1 / Scale());
  point_in_root_frame += GetScrollOffset();
  return point_in_root_frame;
}

gfx::PointF VisualViewport::RootFrameToViewport(
    const gfx::PointF& point_in_root_frame) const {
  gfx::PointF point_in_viewport = point_in_root_frame;
  point_in_viewport -= GetScrollOffset();
  point_in_viewport.Scale(Scale());
  return point_in_viewport;
}

gfx::Point VisualViewport::ViewportToRootFrame(
    const gfx::Point& point_in_viewport) const {
  // FIXME: How to snap to pixels?
  return gfx::ToFlooredPoint(
      ViewportToRootFrame(gfx::PointF(point_in_viewport)));
}

gfx::Point VisualViewport::RootFrameToViewport(
    const gfx::Point& point_in_root_frame) const {
  // FIXME: How to snap to pixels?
  return gfx::ToFlooredPoint(
      RootFrameToViewport(gfx::PointF(point_in_root_frame)));
}

bool VisualViewport::ShouldDisableDesktopWorkarounds() const {
  DCHECK(IsActiveViewport());

  LocalFrameView* frame_view = LocalMainFrame().View();
  if (!frame_view)
    return false;

  if (!LocalMainFrame().GetSettings()->GetViewportEnabled())
    return false;

  // A document is considered adapted to small screen UAs if one of these holds:
  // 1. The author specified viewport has a constrained width that is equal to
  //    the initial viewport width.
  // 2. The author has disabled viewport zoom.
  const PageScaleConstraints& constraints =
      GetPage().GetPageScaleConstraintsSet().PageDefinedConstraints();

  return frame_view->GetLayoutSize().width() == size_.width() ||
         (constraints.minimum_scale == constraints.maximum_scale &&
          constraints.minimum_scale != -1);
}

cc::AnimationHost* VisualViewport::GetCompositorAnimationHost() const {
  DCHECK(IsActiveViewport());
  DCHECK(GetChromeClient());
  return GetChromeClient()->GetCompositorAnimationHost(LocalMainFrame());
}

cc::AnimationTimeline* VisualViewport::GetCompositorAnimationTimeline() const {
  DCHECK(IsActiveViewport());
  DCHECK(GetChromeClient());
  return GetChromeClient()->GetScrollAnimationTimeline(LocalMainFrame());
}

void VisualViewport::NotifyRootFrameViewport() const {
  DCHECK(IsActiveViewport());

  if (!GetRootFrameViewport())
    return;

  GetRootFrameViewport()->DidUpdateVisualViewport();
}

ScrollbarTheme& VisualViewport::GetPageScrollbarTheme() const {
  return GetPage().GetScrollbarTheme();
}

PaintArtifactCompositor* VisualViewport::GetPaintArtifactCompositor() const {
  DCHECK(IsActiveViewport());

  LocalFrameView* frame_view = LocalMainFrame().View();
  if (!frame_view)
    return nullptr;

  return frame_view->GetPaintArtifactCompositor();
}

std::unique_ptr<TracedValue> VisualViewport::ViewportToTracedValue() const {
  auto value = std::make_unique<TracedValue>();
  gfx::Rect viewport = VisibleContentRect();
  value->SetInteger("x", ClampTo<int>(roundf(viewport.x())));
  value->SetInteger("y", ClampTo<int>(roundf(viewport.y())));
  value->SetInteger("width", ClampTo<int>(roundf(viewport.width())));
  value->SetInteger("height", ClampTo<int>(roundf(viewport.height())));
  value->SetString("frameID",
                   IdentifiersFactory::FrameId(GetPage().MainFrame()));
  value->SetBoolean("isActive", IsActiveViewport());
  return value;
}

void VisualViewport::DisposeImpl() {
  scroll_layer_.reset();
  scrollbar_layer_horizontal_.reset();
  scrollbar_layer_vertical_.reset();
  device_emulation_transform_node_ = nullptr;
  overscroll_elasticity_transform_node_ = nullptr;
  page_scale_node_ = nullptr;
  scroll_translation_node_ = nullptr;
  scroll_node_ = nullptr;
  horizontal_scrollbar_effect_node_ = nullptr;
  vertical_scrollbar_effect_node_ = nullptr;
}

void VisualViewport::Paint(GraphicsContext& context) const {
  if (!IsActiveViewport())
    return;

  // TODO(crbug.com/1015625): Avoid scroll_layer_.
  if (scroll_layer_) {
    PropertyTreeStateOrAlias state(parent_property_tree_state_);
    state.SetTransform(*scroll_translation_node_);
    DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client, "Inner Viewport Scroll Layer");
    RecordForeignLayer(context, *client,
                       DisplayItem::kForeignLayerViewportScroll, scroll_layer_,
                       gfx::Point(), &state);
  }

  if (scrollbar_layer_horizontal_) {
    PropertyTreeStateOrAlias state(parent_property_tree_state_);
    state.SetEffect(*horizontal_scrollbar_effect_node_);
    DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client,
                                      "Inner Viewport Horizontal Scrollbar");
    RecordForeignLayer(
        context, *client, DisplayItem::kForeignLayerViewportScrollbar,
        scrollbar_layer_horizontal_,
        gfx::Point(0, size_.height() - ScrollbarThickness()), &state);
  }

  if (scrollbar_layer_vertical_) {
    PropertyTreeStateOrAlias state(parent_property_tree_state_);
    state.SetEffect(*vertical_scrollbar_effect_node_);
    DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client,
                                      "Inner Viewport Vertical Scrollbar");
    RecordForeignLayer(
        context, *client, DisplayItem::kForeignLayerViewportScrollbar,
        scrollbar_layer_vertical_,
        gfx::Point(size_.width() - ScrollbarThickness(), 0), &state);
  }
}

void VisualViewport::UsedColorSchemeChanged() {
  DCHECK(IsActiveViewport());
  // The scrollbar overlay color theme depends on the used color scheme.
  RecalculateOverlayScrollbarColorScheme();
}

void VisualViewport::ScrollbarColorChanged() {
  DCHECK(IsActiveViewport());
  if (scrollbar_layer_horizontal_) {
    DCHECK(scrollbar_layer_vertical_);
    UpdateScrollbarColor(*scrollbar_layer_horizontal_);
    UpdateScrollbarColor(*scrollbar_layer_vertical_);
  }
}

void VisualViewport::UpdateScrollbarColor(cc::SolidColorScrollbarLayer& layer) {
  auto& theme = ScrollbarThemeOverlayMobile::GetInstance();
  layer.SetColor(
      CSSScrollbarThumbColor().value_or(theme.DefaultColor()).toSkColor4f());
}

}  // namespace blink

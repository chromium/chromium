/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {
namespace {
const int kCaretPadding = 10;
const float kIdealPaddingRatio = 0.3f;

// Returns a rect which is offset and scaled accordingly to |base_rect|'s
// location and size.
FloatRect NormalizeRect(const IntRect& to_normalize, const IntRect& base_rect) {
  FloatRect result(to_normalize);
  result.SetLocation(
      FloatPoint(to_normalize.Location() + (-base_rect.Location())));
  result.Scale(1.0 / base_rect.Width(), 1.0 / base_rect.Height());
  return result;
}

}  // namespace

// WebFrameWidget ------------------------------------------------------------

static CreateWebViewFrameWidgetFunction g_create_web_view_frame_widget =
    nullptr;

void InstallCreateWebViewFrameWidgetHook(
    CreateWebViewFrameWidgetFunction create_widget) {
  g_create_web_view_frame_widget = create_widget;
}

WebFrameWidget* WebFrameWidget::CreateForMainFrame(
    WebWidgetClient* client,
    WebLocalFrame* main_frame,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool hidden,
    bool never_composited) {
  DCHECK(client) << "A valid WebWidgetClient must be supplied.";
  DCHECK(!main_frame->Parent());  // This is the main frame.

  // Grabs the WebViewImpl associated with the |main_frame|, which will then
  // be wrapped by the WebViewFrameWidget, with calls being forwarded to the
  // |main_frame|'s WebViewImpl.
  // Note: this can't DCHECK that the view's main frame points to
  // |main_frame|, as provisional frames violate this precondition.
  WebLocalFrameImpl& main_frame_impl = To<WebLocalFrameImpl>(*main_frame);
  DCHECK(main_frame_impl.ViewImpl());
  WebViewImpl& web_view_impl = *main_frame_impl.ViewImpl();

  WebViewFrameWidget* widget = nullptr;
  if (g_create_web_view_frame_widget) {
    widget = g_create_web_view_frame_widget(
        util::PassKey<WebFrameWidget>(), *client, web_view_impl,
        std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
        std::move(mojo_widget_host), std::move(mojo_widget),
        main_frame->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, is_for_nested_main_frame, hidden, never_composited);
  } else {
    // Note: this isn't a leak, as the object has a self-reference that the
    // caller needs to release by calling Close().
    // TODO(dcheng): Remove the special bridge class for main frame widgets.
    widget = MakeGarbageCollected<WebViewFrameWidget>(
        util::PassKey<WebFrameWidget>(), *client, web_view_impl,
        std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
        std::move(mojo_widget_host), std::move(mojo_widget),
        main_frame->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, is_for_nested_main_frame, hidden, never_composited);
  }
  widget->BindLocalRoot(*main_frame);
  return widget;
}

WebFrameWidget* WebFrameWidget::CreateForChildLocalRoot(
    WebWidgetClient* client,
    WebLocalFrame* local_root,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited) {
  DCHECK(client) << "A valid WebWidgetClient must be supplied.";
  DCHECK(local_root->Parent());  // This is not the main frame.
  // Frames whose direct ancestor is a remote frame are local roots. Verify this
  // is one. Other frames should be using the widget for their nearest local
  // root.
  DCHECK(local_root->Parent()->IsWebRemoteFrame());

  // Note: this isn't a leak, as the object has a self-reference that the
  // caller needs to release by calling Close().
  auto* widget = MakeGarbageCollected<WebFrameWidgetImpl>(
      util::PassKey<WebFrameWidget>(), *client,
      std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
      std::move(mojo_widget_host), std::move(mojo_widget),
      local_root->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
      frame_sink_id, hidden, never_composited);
  widget->BindLocalRoot(*local_root);
  return widget;
}

WebFrameWidgetImpl::WebFrameWidgetImpl(
    util::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited)
    : WebFrameWidgetBase(client,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget),
                         std::move(task_runner),
                         frame_sink_id,
                         hidden,
                         never_composited,
                         /*is_for_child_local_root=*/true,
                         /*is_for_nested_main_frame=*/false),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {}

WebFrameWidgetImpl::~WebFrameWidgetImpl() = default;

// WebWidget ------------------------------------------------------------------

void WebFrameWidgetImpl::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  GetPage()->WillCloseAnimationHost(LocalRootImpl()->GetFrame()->View());

  WebFrameWidgetBase::Close(std::move(cleanup_runner));

  self_keep_alive_.Clear();
}

gfx::Size WebFrameWidgetImpl::Size() {
  return size_.value_or(gfx::Size());
}

void WebFrameWidgetImpl::Resize(const gfx::Size& new_size) {
  if (size_ && *size_ == new_size)
    return;

  if (did_suspend_parsing_) {
    did_suspend_parsing_ = false;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->ResumeParser();
  }

  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  if (!view)
    return;

  size_ = new_size;

  UpdateMainFrameLayoutSize();

  view->Resize(WebSize(*size_));

  // FIXME: In WebViewImpl this layout was a precursor to setting the minimum
  // scale limit.  It is not clear if this is necessary for frame-level widget
  // resize.
  if (view->NeedsLayout())
    view->UpdateLayout();

  // FIXME: Investigate whether this is needed; comment from eseidel suggests
  // that this function is flawed.
  // TODO(kenrb): It would probably make more sense to check whether lifecycle
  // updates are throttled in the root's LocalFrameView, but for OOPIFs that
  // doesn't happen. Need to investigate if OOPIFs can be throttled during
  // load.
  if (LocalRootImpl()->GetFrame()->GetDocument()->IsLoadCompleted()) {
    // FIXME: This is wrong. The LocalFrameView is responsible sending a
    // resizeEvent as part of layout. Layout is also responsible for sending
    // invalidations to the embedder. This method and all callers may be wrong.
    // -- eseidel.
    if (LocalRootImpl()->GetFrameView()) {
      // Enqueues the resize event.
      LocalRootImpl()->GetFrame()->GetDocument()->EnqueueResizeEvent();
    }

    // Pass the limits even though this is for subframes, as the limits will
    // be needed in setting the raster scale. We set this value when setting
    // up the compositor, but need to update it when the limits of the
    // WebViewImpl have changed.
    // TODO(wjmaclean): This is updating when the size of the *child frame*
    // have changed which are completely independent of the WebView, and in an
    // OOPIF where the main frame is remote, are these limits even useful?
    SetPageScaleStateAndLimits(1.f, false /* is_pinch_gesture_active */,
                               View()->MinimumPageScaleFactor(),
                               View()->MaximumPageScaleFactor());
  }
}

void WebFrameWidgetImpl::UpdateMainFrameLayoutSize() {
  if (!LocalRootImpl())
    return;

  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  if (!view)
    return;

  gfx::Size layout_size = *size_;

  view->SetLayoutSize(WebSize(layout_size));
}

bool WebFrameWidgetImpl::ShouldHandleImeEvents() {
  // TODO(ekaramad): WebViewWidgetImpl returns true only if it has focus.
  // We track page focus in all RenderViews on the page but
  // the RenderWidgets corresponding to child local roots do not get the
  // update. For now, this method returns true when the RenderWidget is for a
  // child local frame, i.e., IME events will be processed regardless of page
  // focus. We should revisit this after page focus for OOPIFs has been fully
  // resolved (https://crbug.com/689777).
  return LocalRootImpl();
}

bool WebFrameWidgetImpl::ScrollFocusedEditableElementIntoView() {
  Element* element = FocusedElement();
  if (!element || !WebElement(element).IsEditable())
    return false;

  if (!element->GetLayoutObject())
    return false;

  PhysicalRect rect_to_scroll;
  auto params = ScrollAlignment::CreateScrollIntoViewParams();
  GetScrollParamsForFocusedEditableElement(*element, rect_to_scroll, params);
  element->GetLayoutObject()->ScrollRectToVisible(rect_to_scroll,
                                                  std::move(params));
  return true;
}

void WebFrameWidgetImpl::FocusChanged(bool enable) {
  if (enable)
    GetPage()->GetFocusController().SetActive(true);
  GetPage()->GetFocusController().SetFocused(enable);
  if (enable) {
    LocalFrame* focused_frame = GetPage()->GetFocusController().FocusedFrame();
    if (focused_frame) {
      Element* element = focused_frame->GetDocument()->FocusedElement();
      if (element && focused_frame->Selection()
                         .ComputeVisibleSelectionInDOMTreeDeprecated()
                         .IsNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focused_frame->GetDocument()->UpdateStyleAndLayoutTree();
        if (element->IsTextControl()) {
          element->UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
        } else if (HasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focused_frame->Selection().SetSelectionAndEndTyping(
              SelectionInDOMTree::Builder().Collapse(position).Build());
        }
      }
    }
  } else {
    LocalFrame* focused_frame = FocusedLocalFrameInWidget();
    if (focused_frame) {
      // Finish an ongoing composition to delete the composition node.
      if (focused_frame->GetInputMethodController().HasComposition()) {
        // TODO(editing-dev): The use of
        // UpdateStyleAndLayout needs to be audited.
        // See http://crbug.com/590369 for more details.
        focused_frame->GetDocument()->UpdateStyleAndLayout(
            DocumentUpdateReason::kFocus);

        focused_frame->GetInputMethodController().FinishComposingText(
            InputMethodController::kKeepSelection);
      }
    }
  }
}

void WebFrameWidgetImpl::HandleMouseLeave(LocalFrame& main_frame,
                                          const WebMouseEvent& event) {
  // FIXME: WebWidget doesn't have the method below.
  // m_client->setMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(main_frame, event);
}

WebInputEventResult WebFrameWidgetImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  DCHECK(Client());
  WebInputEventResult event_result = WebInputEventResult::kNotHandled;
  bool event_cancelled = false;
  base::Optional<ContextMenuAllowedScope> maybe_context_menu_scope;

  WebViewImpl* view_impl = View();
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureScrollUpdate:
    case WebInputEvent::Type::kGestureTap:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapDown:
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // WebViewImpl takes additional steps to avoid the following GestureTap
      // from re-opening the popup being closed here, but since GestureTap will
      // unconditionally close the current popup here, it is not used/needed.
      // TODO(wjmaclean): We should maybe mirror what WebViewImpl does, the
      // HandleGestureEvent() needs to happen inside or before the GestureTap
      // case to do so.
      View()->CancelPagePopup();
      break;
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGestureShowPress:
      break;
    case WebInputEvent::Type::kGestureDoubleTap:
      if (GetPage()->GetChromeClient().DoubleTapToZoomEnabled() &&
          view_impl->MinimumPageScaleFactor() !=
              view_impl->MaximumPageScaleFactor()) {
        LocalFrame* frame = LocalRootImpl()->GetFrame();
        WebGestureEvent scaled_event =
            TransformWebGestureEvent(frame->View(), event);
        IntPoint pos_in_local_frame_root =
            FlooredIntPoint(scaled_event.PositionInRootFrame());
        auto block_bounds =
            gfx::Rect(ComputeBlockBound(pos_in_local_frame_root, false));

        // This sends the tap point and bounds to the main frame renderer via
        // the browser, where their coordinates will be transformed into the
        // main frame's coordinate space.
        GetAssociatedFrameWidgetHost()->AnimateDoubleTapZoomInMainFrame(
            pos_in_local_frame_root, block_bounds);
      }
      event_result = WebInputEventResult::kHandledSystem;
      DidHandleGestureEvent(event, event_cancelled);
      return event_result;
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
      GetPage()->GetContextMenuController().ClearContextMenu();
      maybe_context_menu_scope.emplace();
      break;
    default:
      NOTREACHED();
  }
  LocalFrame* frame = LocalRootImpl()->GetFrame();
  WebGestureEvent scaled_event = TransformWebGestureEvent(frame->View(), event);
  event_result = frame->GetEventHandler().HandleGestureEvent(scaled_event);
  DidHandleGestureEvent(event, event_cancelled);
  return event_result;
}

PaintLayerCompositor* WebFrameWidgetImpl::Compositor() const {
  LocalFrame* frame = LocalRootImpl()->GetFrame();
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->GetLayoutView())
    return nullptr;

  return frame->GetDocument()->GetLayoutView()->Compositor();
}

void WebFrameWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  if (!layer) {
    // This notifies the WebFrameWidgetImpl that its LocalFrame tree is being
    // detached.
    return;
  }

  // WebFrameWidgetImpl is used for child frames, which always have a
  // transparent background color.
  widget_base_->LayerTreeHost()->set_background_color(SK_ColorTRANSPARENT);
  // Pass the limits even though this is for subframes, as the limits will
  // be needed in setting the raster scale.
  SetPageScaleStateAndLimits(1.f, false /* is_pinch_gesture_active */,
                             View()->MinimumPageScaleFactor(),
                             View()->MaximumPageScaleFactor());

  widget_base_->LayerTreeHost()->SetRootLayer(layer);
}

void WebFrameWidgetImpl::DidCreateLocalRootView() {
  // If this WebWidget still hasn't received its size from the embedder, block
  // the parser. This is necessary, because the parser can cause layout to
  // happen, which needs to be done with the correct size.
  if (!size_) {
    did_suspend_parsing_ = true;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->BlockParser();
  }
}

void WebFrameWidgetImpl::GetScrollParamsForFocusedEditableElement(
    const Element& element,
    PhysicalRect& rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr& params) {
  LocalFrameView& frame_view = *element.GetDocument().View();
  IntRect absolute_element_bounds =
      element.GetLayoutObject()->AbsoluteBoundingBoxRect();
  IntRect absolute_caret_bounds =
      element.GetDocument().GetFrame()->Selection().AbsoluteCaretBounds();
  // Ideally, the chosen rectangle includes the element box and caret bounds
  // plus some margin on the left. If this does not work (i.e., does not fit
  // inside the frame view), then choose a subrect which includes the caret
  // bounds. It is preferrable to also include element bounds' location and left
  // align the scroll. If this cant be satisfied, the scroll will be right
  // aligned.
  IntRect maximal_rect =
      UnionRect(absolute_element_bounds, absolute_caret_bounds);

  // Set the ideal margin.
  maximal_rect.ShiftXEdgeTo(
      maximal_rect.X() -
      static_cast<int>(kIdealPaddingRatio * absolute_element_bounds.Width()));

  bool maximal_rect_fits_in_frame =
      !(frame_view.Size() - maximal_rect.Size()).IsEmpty();

  if (!maximal_rect_fits_in_frame) {
    IntRect frame_rect(maximal_rect.Location(), frame_view.Size());
    maximal_rect.Intersect(frame_rect);
    IntPoint point_forced_to_be_visible =
        absolute_caret_bounds.MaxXMaxYCorner() +
        IntSize(kCaretPadding, kCaretPadding);
    if (!maximal_rect.Contains(point_forced_to_be_visible)) {
      // Move the rect towards the point until the point is barely contained.
      maximal_rect.Move(point_forced_to_be_visible -
                        maximal_rect.MaxXMaxYCorner());
    }
  }

  params->zoom_into_rect = View()->ShouldZoomToLegibleScale(element);
  params->relative_element_bounds = NormalizeRect(
      Intersection(absolute_element_bounds, maximal_rect), maximal_rect);
  params->relative_caret_bounds = NormalizeRect(
      Intersection(absolute_caret_bounds, maximal_rect), maximal_rect);
  params->behavior = mojom::blink::ScrollBehavior::kInstant;
  rect_to_scroll = PhysicalRect(maximal_rect);
}

void WebFrameWidgetImpl::ApplyVisualPropertiesSizing(
    const VisualProperties& visual_properties) {
  SetWindowSegments(visual_properties.root_widget_window_segments);
  widget_base_->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value_or(viz::LocalSurfaceId()),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_info);

  // Store this even when auto-resizing, it is the size of the full viewport
  // used for clipping, and this value is propagated down the Widget
  // hierarchy via the VisualProperties waterfall.
  widget_base_->SetVisibleViewportSizeInDIPs(
      visual_properties.visible_viewport_size);

  // Widgets in a WebView's frame tree without a local main frame
  // set the size of the WebView to be the |visible_viewport_size|, in order
  // to limit compositing in (out of process) child frames to what is visible.
  //
  // Note that child frames in the same process/WebView frame tree as the
  // main frame do not do this in order to not clobber the source of truth in
  // the main frame.
  if (!View()->MainFrameImpl()) {
    View()->Resize(widget_base_->DIPsToCeiledBlinkSpace(
        widget_base_->VisibleViewportSizeInDIPs()));
  }

  Resize(widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size));
}

}  // namespace blink

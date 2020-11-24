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
        base::PassKey<WebFrameWidget>(), *client, web_view_impl,
        std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
        std::move(mojo_widget_host), std::move(mojo_widget),
        main_frame->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, is_for_nested_main_frame, hidden, never_composited);
  } else {
    // Note: this isn't a leak, as the object has a self-reference that the
    // caller needs to release by calling Close().
    // TODO(dcheng): Remove the special bridge class for main frame widgets.
    widget = MakeGarbageCollected<WebViewFrameWidget>(
        base::PassKey<WebFrameWidget>(), *client, web_view_impl,
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
      base::PassKey<WebFrameWidget>(), *client,
      std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
      std::move(mojo_widget_host), std::move(mojo_widget),
      local_root->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
      frame_sink_id, hidden, never_composited);
  widget->BindLocalRoot(*local_root);
  return widget;
}

WebFrameWidgetImpl::WebFrameWidgetImpl(
    base::PassKey<WebFrameWidget>,
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
                         /*is_for_nested_main_frame=*/false) {}

WebFrameWidgetImpl::~WebFrameWidgetImpl() = default;

// WebWidget ------------------------------------------------------------------

WebInputEventResult WebFrameWidgetImpl::HandleGestureEventScaled(
    const WebGestureEvent& scaled_event) {
  base::Optional<ContextMenuAllowedScope> maybe_context_menu_scope;
  switch (scaled_event.GetType()) {
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
  return frame->GetEventHandler().HandleGestureEvent(scaled_event);
}

}  // namespace blink

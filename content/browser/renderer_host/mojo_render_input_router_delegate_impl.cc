// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mojo_render_input_router_delegate_impl.h"

#include "components/input/input_event_source.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

namespace content {

MojoRenderInputRouterDelegateImpl::MojoRenderInputRouterDelegateImpl(
    RenderWidgetHostImpl* host)
    : host_(*host) {}

MojoRenderInputRouterDelegateImpl::~MojoRenderInputRouterDelegateImpl() =
    default;

void MojoRenderInputRouterDelegateImpl::
    SetupRenderInputRouterDelegateConnection() {
  rir_delegate_client_receiver_.reset();
  rir_delegate_remote_.reset();
  GetHostFrameSinkManager()->SetupRenderInputRouterDelegateConnection(
      host_->GetFrameSinkId(),
      rir_delegate_client_receiver_.BindNewEndpointAndPassRemote(),
      rir_delegate_remote_.BindNewEndpointAndPassReceiver());
}

input::mojom::RenderInputRouterDelegate*
MojoRenderInputRouterDelegateImpl::GetRenderInputRouterDelegateRemote() {
  if (!rir_delegate_remote_) {
    return nullptr;
  }
  return rir_delegate_remote_.get();
}

void MojoRenderInputRouterDelegateImpl::NotifyObserversOfInputEvent(
    std::unique_ptr<blink::WebCoalescedInputEvent> event,
    bool dispatched_to_renderer) {
  host_->NotifyObserversOfInputEventWithSource(
      event->Event(), input::InputEventSource::kViz, dispatched_to_renderer);
}

void MojoRenderInputRouterDelegateImpl::NotifyObserversOfInputEventAcks(
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result,
    std::unique_ptr<blink::WebCoalescedInputEvent> event) {
  host_->NotifyObserversOfInputEventAcks(ack_source, ack_result,
                                         event->Event());
}

void MojoRenderInputRouterDelegateImpl::OnInvalidInputEventSource() {
  host_->OnInvalidInputEventSource();
}

void MojoRenderInputRouterDelegateImpl::StateOnOverscrollTransfer(
    blink::mojom::DidOverscrollParamsPtr params) {
  host_->DidOverscroll(std::move(params));
}

void MojoRenderInputRouterDelegateImpl::RendererInputResponsivenessChanged(
    bool is_responsive,
    std::optional<base::TimeTicks> ack_timeout_ts) {
  if (is_responsive) {
    host_->RendererIsResponsive();
  } else {
    CHECK(ack_timeout_ts.has_value());
    host_->OnInputEventAckTimeout(*ack_timeout_ts);
  }
}

}  // namespace content

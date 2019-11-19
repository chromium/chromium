// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/frame_sink_provider_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

namespace content {

FrameSinkProviderImpl::FrameSinkProviderImpl(int32_t process_id)
    : process_id_(process_id) {}

FrameSinkProviderImpl::~FrameSinkProviderImpl() = default;

void FrameSinkProviderImpl::Bind(
    mojo::PendingReceiver<mojom::FrameSinkProvider> receiver) {
  if (receiver_.is_bound()) {
    DLOG(ERROR) << "Received multiple requests for FrameSinkProvider. "
                << "There should be only one instance per renderer.";
    return;
  }
  receiver_.Bind(std::move(receiver));
}

void FrameSinkProviderImpl::Unbind() {
  receiver_.reset();
}

void FrameSinkProviderImpl::CreateForWidget(
    int32_t widget_id,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client) {
  RenderWidgetHostImpl* render_widget_host_impl =
      RenderWidgetHostImpl::FromID(process_id_, widget_id);
  if (!render_widget_host_impl) {
    DLOG(ERROR) << "No RenderWidgetHost exists with id " << widget_id
                << " in process " << process_id_;
    return;
  }
  render_widget_host_impl->RequestCompositorFrameSink(
      std::move(compositor_frame_sink_receiver),
      std::move(compositor_frame_sink_client));
}

void FrameSinkProviderImpl::RegisterRenderFrameMetadataObserver(
    int32_t widget_id,
    mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver,
    mojo::PendingRemote<mojom::RenderFrameMetadataObserver>
        render_frame_metadata_observer) {
  RenderWidgetHostImpl* render_widget_host_impl =
      RenderWidgetHostImpl::FromID(process_id_, widget_id);
  if (!render_widget_host_impl) {
    DLOG(ERROR) << "No RenderWidgetHost exists with id " << widget_id
                << " in process " << process_id_;
    return;
  }
  render_widget_host_impl->RegisterRenderFrameMetadataObserver(
      std::move(render_frame_metadata_observer_client_receiver),
      std::move(render_frame_metadata_observer));
}

}  // namespace content

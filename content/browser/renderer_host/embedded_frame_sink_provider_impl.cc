// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/renderer_host/embedded_frame_sink_impl.h"

namespace content {

EmbeddedFrameSinkProviderImpl::EmbeddedFrameSinkProviderImpl(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    uint32_t renderer_client_id)
    : host_frame_sink_manager_(host_frame_sink_manager),
      renderer_client_id_(renderer_client_id) {
  DCHECK(host_frame_sink_manager);
}

EmbeddedFrameSinkProviderImpl::~EmbeddedFrameSinkProviderImpl() = default;

void EmbeddedFrameSinkProviderImpl::Add(
    mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void EmbeddedFrameSinkProviderImpl::RegisterEmbeddedFrameSink(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient> client) {
  // TODO(kylechar): Kill the renderer too.
  if (parent_frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid parent client id " << parent_frame_sink_id;
    return;
  }
  if (frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid client id " << frame_sink_id;
    return;
  }

  if (frame_sink_id.sink_id() <=
      uint32_t{std::numeric_limits<int32_t>::max()}) {
    receivers_.ReportBadMessage("Sink ID out of range");
    return;
  }

  auto destroy_callback =
      base::BindOnce(&EmbeddedFrameSinkProviderImpl::DestroyEmbeddedFrameSink,
                     base::Unretained(this), frame_sink_id);

  frame_sink_map_[frame_sink_id] = std::make_unique<EmbeddedFrameSinkImpl>(
      host_frame_sink_manager_, parent_frame_sink_id, frame_sink_id,
      std::move(client), std::move(destroy_callback));
}

void EmbeddedFrameSinkProviderImpl::RegisterEmbeddedFrameSinkBundle(
    const viz::FrameSinkBundleId& bundle_id,
    mojo::PendingReceiver<viz::mojom::FrameSinkBundle> receiver,
    mojo::PendingRemote<viz::mojom::FrameSinkBundleClient> client) {
  host_frame_sink_manager_->CreateFrameSinkBundle(
      bundle_id, std::move(receiver), std::move(client));
}

void EmbeddedFrameSinkProviderImpl::CreateCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver) {
  if (frame_sink_id.client_id() != renderer_client_id_) {
    receivers_.ReportBadMessage("Invalid client ID");
    return;
  }

  auto iter = frame_sink_map_.find(frame_sink_id);
  if (iter == frame_sink_map_.end()) {
    DLOG(ERROR) << "No EmbeddedFrameSinkImpl for " << frame_sink_id;
    return;
  }

  iter->second->CreateCompositorFrameSink(std::move(client),
                                          std::move(receiver));
}

void EmbeddedFrameSinkProviderImpl::CreateBundledCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    const viz::FrameSinkBundleId& bundle_id,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver) {
  if (frame_sink_id.client_id() != renderer_client_id_) {
    receivers_.ReportBadMessage("Invalid client ID");
    return;
  }

  auto iter = frame_sink_map_.find(frame_sink_id);
  if (iter == frame_sink_map_.end()) {
    DLOG(ERROR) << "No EmbeddedFrameSinkImpl for " << frame_sink_id;
    return;
  }

  iter->second->CreateBundledCompositorFrameSink(bundle_id, std::move(client),
                                                 std::move(receiver));
}

void EmbeddedFrameSinkProviderImpl::CreateSimpleCompositorFrameSink(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient>
        embedded_frame_sink_client,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver) {
  RegisterEmbeddedFrameSink(parent_frame_sink_id, frame_sink_id,
                            std::move(embedded_frame_sink_client));
  CreateCompositorFrameSink(frame_sink_id,
                            std::move(compositor_frame_sink_client),
                            std::move(compositor_frame_sink_receiver));
}

void EmbeddedFrameSinkProviderImpl::ConnectToEmbedder(
    const viz::FrameSinkId& child_frame_sink_id,
    mojo::PendingReceiver<blink::mojom::SurfaceEmbedder>
        surface_embedder_receiver) {
  // TODO(kylechar): Kill the renderer too.
  if (child_frame_sink_id.client_id() != renderer_client_id_) {
    DLOG(ERROR) << "Invalid client id " << child_frame_sink_id;
    return;
  }

  auto iter = frame_sink_map_.find(child_frame_sink_id);
  if (iter == frame_sink_map_.end()) {
    DLOG(ERROR) << "No EmbeddedFrameSinkImpl for " << child_frame_sink_id;
    return;
  }

  iter->second->ConnectToEmbedder(std::move(surface_embedder_receiver));
}

void EmbeddedFrameSinkProviderImpl::DestroyEmbeddedFrameSink(
    viz::FrameSinkId frame_sink_id) {
  frame_sink_map_.erase(frame_sink_id);
}

void EmbeddedFrameSinkProviderImpl::RegisterFrameSinkHierarchy(
    const viz::FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_map_.find(frame_sink_id);
  if (iter == frame_sink_map_.end()) {
    DLOG(ERROR) << "No EmbeddedFrameSinkImpl for " << frame_sink_id;
    return;
  }
  iter->second->RegisterFrameSinkHierarchy();
}

void EmbeddedFrameSinkProviderImpl::UnregisterFrameSinkHierarchy(
    const viz::FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_map_.find(frame_sink_id);
  if (iter == frame_sink_map_.end()) {
    DLOG(ERROR) << "No EmbeddedFrameSinkImpl for " << frame_sink_id;
    return;
  }
  iter->second->UnregisterFrameSinkHierarchy();
}

}  // namespace content

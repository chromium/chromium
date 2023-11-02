// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_PROVIDER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom.h"

namespace viz {
class HostFrameSinkManager;
}

namespace content {

class EmbeddedFrameSinkImpl;

// Provides embedded frame sinks for a renderer.
class CONTENT_EXPORT EmbeddedFrameSinkProviderImpl
    : public blink::mojom::EmbeddedFrameSinkProvider {
 public:
  EmbeddedFrameSinkProviderImpl(
      viz::HostFrameSinkManager* host_frame_sink_manager,
      uint32_t renderer_client_id);

  EmbeddedFrameSinkProviderImpl(const EmbeddedFrameSinkProviderImpl&) = delete;
  EmbeddedFrameSinkProviderImpl& operator=(
      const EmbeddedFrameSinkProviderImpl&) = delete;

  ~EmbeddedFrameSinkProviderImpl() override;

  void Add(
      mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver);

  // blink::mojom::EmbeddedFrameSinkProvider implementation.
  void RegisterEmbeddedFrameSink(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient> client)
      override;
  void RegisterEmbeddedFrameSinkBundle(
      const viz::FrameSinkBundleId& bundle_id,
      mojo::PendingReceiver<viz::mojom::FrameSinkBundle> receiver,
      mojo::PendingRemote<viz::mojom::FrameSinkBundleClient> client) override;
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> sink_client,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver)
      override;
  void CreateBundledCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      const viz::FrameSinkBundleId& bundle_id,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> sink_client,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver)
      override;
  void CreateSimpleCompositorFrameSink(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient>
          embedded_frame_sink_client,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
          compositor_frame_sink_client,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_receiver) override;
  void ConnectToEmbedder(const viz::FrameSinkId& child_frame_sink_id,
                         mojo::PendingReceiver<blink::mojom::SurfaceEmbedder>
                             surface_embedder_receiver) override;
  void RegisterFrameSinkHierarchy(
      const viz::FrameSinkId& frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const viz::FrameSinkId& frame_sink_id) override;

 private:
  friend class EmbeddedFrameSinkProviderImplTest;

  // Destroys the |frame_sink_map_| entry for |frame_sink_id|. Provided as
  // a callback to each EmbeddedFrameSinkImpl so they can destroy themselves.
  void DestroyEmbeddedFrameSink(viz::FrameSinkId frame_sink_id);

  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;

  // FrameSinkIds for embedded frame sinks must use the renderer client id.
  const uint32_t renderer_client_id_;

  mojo::ReceiverSet<blink::mojom::EmbeddedFrameSinkProvider> receivers_;

  base::flat_map<viz::FrameSinkId, std::unique_ptr<EmbeddedFrameSinkImpl>>
      frame_sink_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_PROVIDER_IMPL_H_

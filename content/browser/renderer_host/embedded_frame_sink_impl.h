// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom.h"

namespace viz {
class HostFrameSinkManager;
}

namespace content {

// The browser owned object for an embedded frame sink in a renderer process.
// Both the embedder and embedded frame sink are in the same renderer. Holds a
// client connection to the renderer that is notified when a new SurfaceId
// activates for the embedded frame sink.
class CONTENT_EXPORT EmbeddedFrameSinkImpl : public viz::HostFrameSinkClient {
 public:
  using DestroyCallback = base::OnceCallback<void()>;

  EmbeddedFrameSinkImpl(
      viz::HostFrameSinkManager* host_frame_sink_manager,
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& frame_sink_id,
      mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient> client,
      DestroyCallback destroy_callback);
  ~EmbeddedFrameSinkImpl() override;

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  const viz::LocalSurfaceId& local_surface_id() const {
    return local_surface_id_;
  }

  // Creates a CompositorFrameSink connection to FrameSinkManagerImpl.
  void CreateCompositorFrameSink(
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver);

  // Establishes a connection to the embedder of this FrameSink. Allows the
  // child to notify its embedder of its LocalSurfaceId changes.
  void ConnectToEmbedder(mojo::PendingReceiver<blink::mojom::SurfaceEmbedder>
                             surface_embedder_receiver);

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

 private:
  viz::HostFrameSinkManager* const host_frame_sink_manager_;

  mojo::Remote<blink::mojom::EmbeddedFrameSinkClient> client_;

  // Surface-related state
  const viz::FrameSinkId parent_frame_sink_id_;
  const viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId local_surface_id_;

  bool has_created_compositor_frame_sink_ = false;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedFrameSinkImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_EMBEDDED_FRAME_SINK_IMPL_H_

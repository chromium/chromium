// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_SINK_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_SINK_PROVIDER_IMPL_H_

#include "content/common/frame_sink_provider.mojom.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

// This class lives in the browser and provides CompositorFrameSink for the
// renderer.
class FrameSinkProviderImpl : public mojom::FrameSinkProvider {
 public:
  explicit FrameSinkProviderImpl(int32_t process_id);
  ~FrameSinkProviderImpl() override;

  void Bind(mojo::PendingReceiver<mojom::FrameSinkProvider> receiver);
  void Unbind();

  // mojom::FrameSinkProvider implementation.
  void CreateForWidget(
      int32_t widget_id,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_receiver,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
          compositor_frame_sink_client) override;
  void RegisterRenderFrameMetadataObserver(
      int32_t widget_id,
      mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
          render_frame_metadata_observer_client_receiver,
      mojo::PendingRemote<mojom::RenderFrameMetadataObserver> observer)
      override;

 private:
  const int32_t process_id_;
  mojo::Receiver<mojom::FrameSinkProvider> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameSinkProviderImpl);
};

}  // namespace content

#endif  //  CONTENT_BROWSER_RENDERER_HOST_FRAME_SINK_PROVIDER_IMPL_H_

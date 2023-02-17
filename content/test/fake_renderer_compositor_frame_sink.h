// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_
#define CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_

#include "components/viz/common/frame_timing_details_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace content {

// This class is given to RenderWidgetHost/RenderWidgetHostView unit tests
// instead of RendererCompositorFrameSink.
class FakeRendererCompositorFrameSink
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  FakeRendererCompositorFrameSink(
      mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> receiver);

  FakeRendererCompositorFrameSink(const FakeRendererCompositorFrameSink&) =
      delete;
  FakeRendererCompositorFrameSink& operator=(
      const FakeRendererCompositorFrameSink&) = delete;

  ~FakeRendererCompositorFrameSink() override;

  bool did_receive_ack() { return did_receive_ack_; }
  std::vector<viz::ReturnedResource>& last_reclaimed_resources() {
    return last_reclaimed_resources_;
  }

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}

  // Resets test data.
  void Reset();

  // Runs all queued messages.
  void Flush();

 private:
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient> receiver_;
  mojo::Remote<viz::mojom::CompositorFrameSink> sink_;
  bool did_receive_ack_ = false;
  std::vector<viz::ReturnedResource> last_reclaimed_resources_;
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_

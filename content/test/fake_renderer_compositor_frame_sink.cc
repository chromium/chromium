// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_renderer_compositor_frame_sink.h"

namespace content {

FakeRendererCompositorFrameSink::FakeRendererCompositorFrameSink(
    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> receiver)
    : receiver_(this, std::move(receiver)), sink_(std::move(sink)) {}

FakeRendererCompositorFrameSink::~FakeRendererCompositorFrameSink() = default;

void FakeRendererCompositorFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  ReclaimResources(std::move(resources));
  did_receive_ack_ = true;
}

void FakeRendererCompositorFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  last_reclaimed_resources_ = std::move(resources);
}

void FakeRendererCompositorFrameSink::Reset() {
  did_receive_ack_ = false;
  last_reclaimed_resources_.clear();
}

void FakeRendererCompositorFrameSink::Flush() {
  receiver_.FlushForTesting();
}

}  // namespace content

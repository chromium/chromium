// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_synchronous_compositor_android.h"

#include <utility>

#include "components/viz/common/quads/compositor_frame.h"

namespace content {

TestSynchronousCompositor::TestSynchronousCompositor(
    const viz::FrameSinkId& frame_sink_id)
    : client_(nullptr), frame_sink_id_(frame_sink_id) {}

TestSynchronousCompositor::~TestSynchronousCompositor() {
  SetClient(nullptr);
}

void TestSynchronousCompositor::SetClient(SynchronousCompositorClient* client) {
  if (client_)
    client_->DidDestroyCompositor(this, frame_sink_id_);
  client_ = client;
  if (client_)
    client_->DidInitializeCompositor(this, frame_sink_id_);
}

scoped_refptr<SynchronousCompositor::FrameFuture>
TestSynchronousCompositor::DemandDrawHwAsync(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  auto future = base::MakeRefCounted<FrameFuture>();
  future->SetFrame(std::move(hardware_frame_));
  return future;
}

void TestSynchronousCompositor::ReturnResources(
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  ReturnedResources returned_resources;
  returned_resources.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  returned_resources.resources = std::move(resources);
  frame_ack_array_.push_back(std::move(returned_resources));
}

void TestSynchronousCompositor::SwapReturnedResources(FrameAckArray* array) {
  DCHECK(array);
  frame_ack_array_.swap(*array);
}

bool TestSynchronousCompositor::DemandDrawSw(SkCanvas* canvas,
                                             bool software_canvas) {
  DCHECK(canvas);
  return true;
}

float TestSynchronousCompositor::GetVelocityInPixelsPerSecond() {
  return 0.f;
}

void TestSynchronousCompositor::SetHardwareFrame(
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<viz::CompositorFrame> frame) {
  hardware_frame_ = std::make_unique<Frame>();
  hardware_frame_->layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  hardware_frame_->local_surface_id =
      viz::LocalSurfaceId(1, base::UnguessableToken::Create());
  hardware_frame_->frame = std::move(frame);
}

TestSynchronousCompositor::ReturnedResources::ReturnedResources()
    : layer_tree_frame_sink_id(0u) {}

TestSynchronousCompositor::ReturnedResources::ReturnedResources(
    ReturnedResources&&) = default;

TestSynchronousCompositor::ReturnedResources::~ReturnedResources() {}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
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
  auto future = base::MakeRefCounted<FrameFuture>(
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  future->SetFrame(std::move(hardware_frame_));
  return future;
}

void TestSynchronousCompositor::ReturnResources(
    uint32_t layer_tree_frame_sink_id,
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnedResources returned_resources;
  returned_resources.layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  returned_resources.resources = resources;
  frame_ack_array_.push_back(returned_resources);
}

void TestSynchronousCompositor::SwapReturnedResources(FrameAckArray* array) {
  DCHECK(array);
  frame_ack_array_.swap(*array);
}

bool TestSynchronousCompositor::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(canvas);
  return true;
}

void TestSynchronousCompositor::SetHardwareFrame(
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<viz::CompositorFrame> frame) {
  hardware_frame_ = std::make_unique<Frame>();
  hardware_frame_->layer_tree_frame_sink_id = layer_tree_frame_sink_id;
  hardware_frame_->frame = std::move(frame);
}

TestSynchronousCompositor::ReturnedResources::ReturnedResources()
    : layer_tree_frame_sink_id(0u) {}

TestSynchronousCompositor::ReturnedResources::ReturnedResources(
    const ReturnedResources& other) = default;

TestSynchronousCompositor::ReturnedResources::~ReturnedResources() {}

}  // namespace content

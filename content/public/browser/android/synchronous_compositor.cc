// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/synchronous_compositor.h"

#include <utility>

#include "base/threading/thread_restrictions.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace content {

SynchronousCompositor::Frame::Frame() : layer_tree_frame_sink_id(0u) {}

SynchronousCompositor::Frame::~Frame() {}

SynchronousCompositor::Frame::Frame(Frame&& rhs)
    : layer_tree_frame_sink_id(rhs.layer_tree_frame_sink_id),
      frame(std::move(rhs.frame)) {}

SynchronousCompositor::FrameFuture::FrameFuture(
    viz::LocalSurfaceId local_surface_id)
    : waitable_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      local_surface_id_(local_surface_id) {}

SynchronousCompositor::FrameFuture::~FrameFuture() {}

void SynchronousCompositor::FrameFuture::SetFrame(
    std::unique_ptr<Frame> frame) {
  frame_ = std::move(frame);
  waitable_event_.Signal();
}

std::unique_ptr<SynchronousCompositor::Frame>
SynchronousCompositor::FrameFuture::GetFrame() {
#if DCHECK_IS_ON()
  DCHECK(!waited_);
  waited_ = true;
#endif
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      allow_base_sync_primitives;
  waitable_event_.Wait();
  return std::move(frame_);
}

SynchronousCompositor::Frame& SynchronousCompositor::Frame::operator=(
    Frame&& rhs) {
  layer_tree_frame_sink_id = rhs.layer_tree_frame_sink_id;
  frame = std::move(rhs.frame);
  return *this;
}

}  // namespace content

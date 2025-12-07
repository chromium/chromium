// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_observer.h"

#include "content/renderer/render_frame_impl.h"

using blink::WebFrame;

namespace content {

RenderFrameObserver::RenderFrameObserver(RenderFrame* render_frame)
    : render_frame_(render_frame) {
  // |render_frame| can be NULL on unit testing.
  if (render_frame) {
    RenderFrameImpl* impl = static_cast<RenderFrameImpl*>(render_frame);
    impl->AddObserver(this);
  }
}

RenderFrameObserver::~RenderFrameObserver() {
  Dispose();
}

void RenderFrameObserver::Dispose() {
  if (render_frame_) {
    RenderFrameImpl* impl = static_cast<RenderFrameImpl*>(render_frame_);
    impl->RemoveObserver(this);
  }
  render_frame_ = nullptr;
}

bool RenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return false;
}

RenderFrame* RenderFrameObserver::render_frame() const {
  return render_frame_;
}

void RenderFrameObserver::RenderFrameGone() {
  render_frame_ = nullptr;
}

bool RenderFrameObserver::SetUpDroppedFramesReporting(
    base::ReadOnlySharedMemoryRegion& shared_memory_dropped_frames) {
  return false;
}

}  // namespace content

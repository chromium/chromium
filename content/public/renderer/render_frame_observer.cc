// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame_observer.h"

#include "content/renderer/render_frame_impl.h"

using blink::WebFrame;

namespace content {

RenderFrameObserver::RenderFrameObserver(RenderFrame* render_frame)
    : render_frame_(render_frame),
      routing_id_(MSG_ROUTING_NONE) {
  // |render_frame| can be NULL on unit testing.
  if (render_frame) {
    RenderFrameImpl* impl = static_cast<RenderFrameImpl*>(render_frame);
    routing_id_ = impl->GetRoutingID();
    DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
    impl->AddObserver(this);
  }
}

RenderFrameObserver::~RenderFrameObserver() {
  if (render_frame_) {
    RenderFrameImpl* impl = static_cast<RenderFrameImpl*>(render_frame_);
    impl->RemoveObserver(this);
  }
}

bool RenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return false;
}

bool RenderFrameObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool RenderFrameObserver::Send(IPC::Message* message) {
  if (render_frame_)
    return render_frame_->Send(message);

  delete message;
  return false;
}

RenderFrame* RenderFrameObserver::render_frame() const {
  return render_frame_;
}

void RenderFrameObserver::RenderFrameGone() {
  render_frame_ = nullptr;
}

}  // namespace content

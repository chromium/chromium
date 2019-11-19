// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_factory.h"

#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

namespace content {

// static
RenderWidgetHostFactory* RenderWidgetHostFactory::factory_ = nullptr;

// static
std::unique_ptr<RenderWidgetHostImpl> RenderWidgetHostFactory::Create(
    RenderWidgetHostDelegate* delegate,
    RenderProcessHost* process,
    int32_t routing_id,
    mojo::PendingRemote<mojom::Widget> widget_interface,
    bool hidden) {
  if (factory_) {
    return factory_->CreateRenderWidgetHost(
        delegate, process, routing_id, std::move(widget_interface), hidden);
  }
  return std::make_unique<RenderWidgetHostImpl>(
      delegate, process, routing_id, std::move(widget_interface), hidden,
      std::make_unique<FrameTokenMessageQueue>());
}

// static
void RenderWidgetHostFactory::RegisterFactory(
    RenderWidgetHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderWidgetHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = nullptr;
}

}  // namespace content

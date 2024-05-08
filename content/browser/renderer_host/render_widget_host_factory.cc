// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_factory.h"

#include <memory>

#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

namespace content {

// static
RenderWidgetHostFactory* RenderWidgetHostFactory::factory_ = nullptr;

// static
std::unique_ptr<RenderWidgetHostImpl> RenderWidgetHostFactory::Create(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    viz::FrameSinkId frame_sink_id,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation) {
  if (factory_) {
    return factory_->CreateRenderWidgetHost(
        frame_tree, delegate, frame_sink_id, std::move(site_instance_group),
        routing_id, hidden, renderer_initiated_creation);
  }
  return RenderWidgetHostImpl::Create(
      {}, frame_tree, delegate, frame_sink_id, std::move(site_instance_group),
      routing_id, hidden, renderer_initiated_creation,
      std::make_unique<FrameTokenMessageQueue>());
}

// static
RenderWidgetHostImpl* RenderWidgetHostFactory::CreateSelfOwned(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden) {
  if (factory_) {
    return factory_->CreateSelfOwnedRenderWidgetHost(
        frame_tree, delegate, std::move(site_instance_group), routing_id,
        hidden);
  }

  return RenderWidgetHostImpl::CreateSelfOwned(
      {}, frame_tree, delegate, std::move(site_instance_group), routing_id,
      hidden, std::make_unique<FrameTokenMessageQueue>());
}

std::unique_ptr<RenderWidgetHostImpl>
RenderWidgetHostFactory::CreateRenderWidgetHost(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    viz::FrameSinkId frame_sink_id,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation) {
  return RenderWidgetHostImpl::Create(
      {}, frame_tree, delegate, frame_sink_id, std::move(site_instance_group),
      routing_id, hidden, renderer_initiated_creation,
      std::make_unique<FrameTokenMessageQueue>());
}

RenderWidgetHostImpl* RenderWidgetHostFactory::CreateSelfOwnedRenderWidgetHost(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden) {
  return RenderWidgetHostImpl::CreateSelfOwned(
      {}, frame_tree, delegate, std::move(site_instance_group), routing_id,
      hidden, std::make_unique<FrameTokenMessageQueue>());
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

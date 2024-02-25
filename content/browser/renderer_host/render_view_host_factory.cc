// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_factory.h"

#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/site_instance_group.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// static
RenderViewHostFactory* RenderViewHostFactory::factory_ = nullptr;

// static
bool RenderViewHostFactory::is_real_render_view_host_ = false;

// static
RenderViewHost* RenderViewHostFactory::Create(
    FrameTree* frame_tree,
    SiteInstanceGroup* group,
    const StoragePartitionConfig& storage_partition_config,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int32_t main_frame_routing_id,
    bool renderer_initiated_creation,
    scoped_refptr<BrowsingContextState> main_browsing_context_state,
    CreateRenderViewHostCase create_case,
    std::optional<viz::FrameSinkId> frame_sink_id) {
  int32_t routing_id = group->process()->GetNextRoutingID();
  int32_t widget_routing_id = group->process()->GetNextRoutingID();

  RenderViewHostImpl* view_host = nullptr;
  if (factory_) {
    view_host = factory_->CreateRenderViewHost(
        frame_tree, group, storage_partition_config, delegate, widget_delegate,
        routing_id, main_frame_routing_id, widget_routing_id,
        std::move(main_browsing_context_state), create_case, frame_sink_id);
  } else {
    view_host = new RenderViewHostImpl(
        frame_tree, group, storage_partition_config,
        RenderWidgetHostFactory::Create(
            frame_tree, widget_delegate,
            frame_sink_id.value_or(RenderWidgetHostImpl::DefaultFrameSinkId(
                *group, widget_routing_id)),
            group->GetSafeRef(), widget_routing_id,
            /*hidden=*/true, renderer_initiated_creation),
        delegate, routing_id, main_frame_routing_id,
        true /* has_initialized_audio_host */,
        std::move(main_browsing_context_state), create_case);
  }

  bool reusing_previous_frame_sink = frame_sink_id.has_value();
  view_host->GetWidget()->SetViewIsFrameSinkIdOwner(
      !reusing_previous_frame_sink);
  return view_host;
}

// static
void RenderViewHostFactory::RegisterFactory(RenderViewHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderViewHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = nullptr;
}

}  // namespace content

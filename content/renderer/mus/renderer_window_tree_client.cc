// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/renderer_window_tree_client.h"

#include <map>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "content/renderer/mus/mus_embedded_frame.h"
#include "content/renderer/mus/mus_embedded_frame_delegate.h"
#include "content/renderer/render_frame_proxy.h"
#include "ui/base/ui_base_features.h"

namespace content {

namespace {

using ConnectionMap = std::map<int, RendererWindowTreeClient*>;
base::LazyInstance<ConnectionMap>::Leaky g_connections =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void RendererWindowTreeClient::CreateIfNecessary(int routing_id) {
  if (!features::IsMultiProcessMash() || Get(routing_id))
    return;
  RendererWindowTreeClient* connection =
      new RendererWindowTreeClient(routing_id);
  g_connections.Get().insert(std::make_pair(routing_id, connection));
}

// static
void RendererWindowTreeClient::Destroy(int routing_id) {
  delete Get(routing_id);
}

// static
RendererWindowTreeClient* RendererWindowTreeClient::Get(int routing_id) {
  auto it = g_connections.Get().find(routing_id);
  if (it != g_connections.Get().end())
    return it->second;
  return nullptr;
}

void RendererWindowTreeClient::Bind(
    ws::mojom::WindowTreeClientRequest request,
    mojom::RenderWidgetWindowTreeClientRequest
        render_widget_window_tree_client_request) {
  // Bind() may be called multiple times.
  binding_.Close();
  render_widget_window_tree_client_binding_.Close();

  binding_.Bind(std::move(request));
  render_widget_window_tree_client_binding_.Bind(
      std::move(render_widget_window_tree_client_request));
}

std::unique_ptr<MusEmbeddedFrame>
RendererWindowTreeClient::OnRenderFrameProxyCreated(
    RenderFrameProxy* render_frame_proxy) {
  auto iter = pending_frames_.find(render_frame_proxy->routing_id());
  if (iter == pending_frames_.end())
    return nullptr;

  const base::UnguessableToken token = iter->second;
  pending_frames_.erase(iter);
  return CreateMusEmbeddedFrame(render_frame_proxy, token);
}

void RendererWindowTreeClient::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (tree_) {
    tree_->SetWindowVisibility(GetAndAdvanceNextChangeId(), root_window_id_,
                               visible);
  }
}

void RendererWindowTreeClient::RequestLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    LayerTreeFrameSinkCallback callback) {
  DCHECK(pending_layer_tree_frame_sink_callback_.is_null());
  if (tree_) {
    RequestLayerTreeFrameSinkInternal(std::move(context_provider),
                                      gpu_memory_buffer_manager,
                                      std::move(callback));
    return;
  }

  pending_context_provider_ = std::move(context_provider);
  pending_gpu_memory_buffer_manager_ = gpu_memory_buffer_manager;
  pending_layer_tree_frame_sink_callback_ = std::move(callback);
}

std::unique_ptr<MusEmbeddedFrame>
RendererWindowTreeClient::CreateMusEmbeddedFrame(
    MusEmbeddedFrameDelegate* delegate,
    const base::UnguessableToken& token) {
  std::unique_ptr<MusEmbeddedFrame> frame = base::WrapUnique<MusEmbeddedFrame>(
      new MusEmbeddedFrame(this, delegate, ++next_window_id_, token));
  embedded_frames_.insert(frame.get());
  return frame;
}

RendererWindowTreeClient::RendererWindowTreeClient(int routing_id)
    : routing_id_(routing_id),
      binding_(this),
      render_widget_window_tree_client_binding_(this) {}

RendererWindowTreeClient::~RendererWindowTreeClient() {
  g_connections.Get().erase(routing_id_);
  DCHECK(embedded_frames_.empty());
}

void RendererWindowTreeClient::RequestLayerTreeFrameSinkInternal(
    scoped_refptr<viz::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    LayerTreeFrameSinkCallback callback) {
  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.pipes.compositor_frame_sink_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization = true;
  if (features::IsVizHitTestingDrawQuadEnabled()) {
    params.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            true /* should_ask_for_child_region */,
            true /* root_accepts_events */);
  }
  auto frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), nullptr /* worker_context_provider */,
          &params);
  tree_->AttachCompositorFrameSink(root_window_id_, std::move(sink_request),
                                   std::move(client));
  std::move(callback).Run(std::move(frame_sink));
}

void RendererWindowTreeClient::OnEmbeddedFrameDestroyed(
    MusEmbeddedFrame* frame) {
  embedded_frames_.erase(embedded_frames_.find(frame));
}

void RendererWindowTreeClient::Embed(uint32_t frame_routing_id,
                                     const base::UnguessableToken& token) {
  RenderFrameProxy* render_frame_proxy =
      RenderFrameProxy::FromRoutingID(frame_routing_id);
  if (!render_frame_proxy) {
    pending_frames_[frame_routing_id] = token;
    return;
  }
  render_frame_proxy->SetMusEmbeddedFrame(
      CreateMusEmbeddedFrame(render_frame_proxy, token));
}

void RendererWindowTreeClient::OnEmbedFromToken(
    const base::UnguessableToken& token,
    ws::mojom::WindowDataPtr root,
    int64_t display_id,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  // Renderers don't use ScheduleEmbedForExistingClient(), so this path should
  // never be hit.
  NOTREACHED();
}

void RendererWindowTreeClient::DestroyFrame(uint32_t frame_routing_id) {
  pending_frames_.erase(frame_routing_id);
}

void RendererWindowTreeClient::OnEmbed(
    ws::mojom::WindowDataPtr root,
    ws::mojom::WindowTreePtr tree,
    int64_t display_id,
    ws::Id focused_window_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  const bool is_reembed = tree_.is_bound();
  if (is_reembed) {
    for (MusEmbeddedFrame* frame : embedded_frames_)
      frame->OnTreeWillChange();
  }
  root_window_id_ = root->window_id;

  tree_ = std::move(tree);
  tree_->SetWindowVisibility(GetAndAdvanceNextChangeId(), root_window_id_,
                             visible_);
  if (!is_reembed) {
    for (MusEmbeddedFrame* frame : embedded_frames_)
      frame->OnTreeAvailable();
  }

  if (!pending_layer_tree_frame_sink_callback_.is_null()) {
    RequestLayerTreeFrameSinkInternal(
        std::move(pending_context_provider_),
        pending_gpu_memory_buffer_manager_,
        std::move(pending_layer_tree_frame_sink_callback_));
    pending_context_provider_ = nullptr;
    pending_gpu_memory_buffer_manager_ = nullptr;
    pending_layer_tree_frame_sink_callback_.Reset();
  }
}

void RendererWindowTreeClient::OnEmbeddedAppDisconnected(ws::Id window_id) {
  // TODO(sad): Embedded mus-client (oopif) is gone. Figure out what to do.
}

void RendererWindowTreeClient::OnUnembed(ws::Id window_id) {
  // At this point all operations will fail. We don't delete this as it would
  // mean all consumers have to null check (as would MusEmbeddedFrames).
}

void RendererWindowTreeClient::OnCaptureChanged(ws::Id new_capture_window_id,
                                                ws::Id old_capture_window_id) {}

void RendererWindowTreeClient::OnFrameSinkIdAllocated(
    ws::Id window_id,
    const viz::FrameSinkId& frame_sink_id) {
  // When mus is not hosting viz FrameSinkIds come from the browser, so we
  // ignore them here.
  if (!features::IsMultiProcessMash())
    return;

  for (MusEmbeddedFrame* embedded_frame : embedded_frames_) {
    if (embedded_frame->window_id_ == window_id) {
      embedded_frame->delegate_->OnMusEmbeddedFrameSinkIdAllocated(
          frame_sink_id);
      return;
    }
  }
}

void RendererWindowTreeClient::OnTopLevelCreated(
    uint32_t change_id,
    ws::mojom::WindowDataPtr data,
    int64_t display_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnWindowBoundsChanged(
    ws::Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {}

void RendererWindowTreeClient::OnWindowTransformChanged(
    ws::Id window_id,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {}

void RendererWindowTreeClient::OnTransientWindowAdded(
    ws::Id window_id,
    ws::Id transient_window_id) {}

void RendererWindowTreeClient::OnTransientWindowRemoved(
    ws::Id window_id,
    ws::Id transient_window_id) {}

void RendererWindowTreeClient::OnWindowHierarchyChanged(
    ws::Id window_id,
    ws::Id old_parent_id,
    ws::Id new_parent_id,
    std::vector<ws::mojom::WindowDataPtr> windows) {}

void RendererWindowTreeClient::OnWindowReordered(
    ws::Id window_id,
    ws::Id relative_window_id,
    ws::mojom::OrderDirection direction) {}

void RendererWindowTreeClient::OnWindowDeleted(ws::Id window_id) {
  // See comments on OnUnembed() for why we do nothing here.
}

void RendererWindowTreeClient::OnWindowVisibilityChanged(ws::Id window_id,
                                                         bool visible) {}

void RendererWindowTreeClient::OnWindowOpacityChanged(ws::Id window_id,
                                                      float old_opacity,
                                                      float new_opacity) {}

void RendererWindowTreeClient::OnWindowDisplayChanged(ws::Id window_id,
                                                      int64_t display_id) {}

void RendererWindowTreeClient::OnWindowParentDrawnStateChanged(ws::Id window_id,
                                                               bool drawn) {}

void RendererWindowTreeClient::OnWindowSharedPropertyChanged(
    ws::Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& new_data) {}

void RendererWindowTreeClient::OnWindowInputEvent(
    uint32_t event_id,
    ws::Id window_id,
    int64_t display_id,
    std::unique_ptr<ui::Event> event,
    bool matches_event_observer) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnObservedInputEvent(
    std::unique_ptr<ui::Event> event) {
  NOTREACHED();
}

void RendererWindowTreeClient::OnWindowFocused(ws::Id focused_window_id) {}

void RendererWindowTreeClient::OnWindowCursorChanged(ws::Id window_id,
                                                     ui::CursorData cursor) {}

void RendererWindowTreeClient::OnDragDropStart(
    const base::flat_map<std::string, std::vector<uint8_t>>& mime_data) {}

void RendererWindowTreeClient::OnDragEnter(ws::Id window_id,
                                           uint32_t event_flags,
                                           const gfx::Point& position,
                                           uint32_t effect_bitmask,
                                           OnDragEnterCallback callback) {}

void RendererWindowTreeClient::OnDragOver(ws::Id window_id,
                                          uint32_t event_flags,
                                          const gfx::Point& position,
                                          uint32_t effect_bitmask,
                                          OnDragOverCallback callback) {}

void RendererWindowTreeClient::OnDragLeave(ws::Id window_id) {}

void RendererWindowTreeClient::OnCompleteDrop(ws::Id window_id,
                                              uint32_t event_flags,
                                              const gfx::Point& position,
                                              uint32_t effect_bitmask,
                                              OnCompleteDropCallback callback) {
}

void RendererWindowTreeClient::OnPerformDragDropCompleted(
    uint32_t change_id,
    bool success,
    uint32_t action_taken) {}

void RendererWindowTreeClient::OnDragDropDone() {}

void RendererWindowTreeClient::OnTopmostWindowChanged(
    const std::vector<ws::Id>& topmost_ids) {}

void RendererWindowTreeClient::OnChangeCompleted(uint32_t change_id,
                                                 bool success) {
  // Don't DCHECK success, as it's possible we'll try to do some operations
  // after unembedded, which means all operations will fail. Additionally
  // setting the window visibility may fail for the root frame (the browser
  // controls the visibility of the root frame).
}

void RendererWindowTreeClient::RequestClose(ws::Id window_id) {}

void RendererWindowTreeClient::GetScreenProviderObserver(
    ws::mojom::ScreenProviderObserverAssociatedRequest observer) {}

void RendererWindowTreeClient::OnOcclusionStateChanged(
    ws::Id window_id,
    ws::mojom::OcclusionState occlusion_state) {}

}  // namespace content

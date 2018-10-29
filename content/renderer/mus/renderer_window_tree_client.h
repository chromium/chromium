// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_
#define CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_

#include <map>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ws/common/types.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class UnguessableToken;
}

namespace cc {
class LayerTreeFrameSink;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
}

namespace content {

class MusEmbeddedFrame;
class MusEmbeddedFrameDelegate;
class RenderFrameProxy;

// ws::mojom::WindowTreeClient implementation for RenderWidget. This lives and
// operates on the renderer's main thread.
class RendererWindowTreeClient : public ws::mojom::WindowTreeClient,
                                 public mojom::RenderWidgetWindowTreeClient {
 public:
  // Creates a RendererWindowTreeClient instance for the RenderWidget instance
  // associated with |routing_id| (if one doesn't already exist). The instance
  // self-destructs when the connection to mus is lost, or when the window is
  // closed.
  static void CreateIfNecessary(int routing_id);

  // Destroys the client instance, if one exists. Otherwise, does nothing.
  static void Destroy(int routing_id);

  // Returns the RendererWindowTreeClient associated with |routing_id|. Returns
  // nullptr if none exists.
  // TODO(sky): make RenderWidget own RendererWindowTreeClient.
  static RendererWindowTreeClient* Get(int routing_id);

  void Bind(ws::mojom::WindowTreeClientRequest request,
            mojom::RenderWidgetWindowTreeClientRequest
                render_widget_window_tree_client_request);

  // Called when a new RenderFrameProxy has been created. If there is a pending
  // embedding ready, it's returned.
  std::unique_ptr<MusEmbeddedFrame> OnRenderFrameProxyCreated(
      RenderFrameProxy* render_frame_proxy);

  // Sets the visibility of the client.
  void SetVisible(bool visible);

  using LayerTreeFrameSinkCallback =
      base::OnceCallback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;
  void RequestLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      LayerTreeFrameSinkCallback callback);

  // Creates a new MusEmbeddedFrame. |token| is an UnguessableToken that was
  // registered for an embedding with mus (specifically ws::mojom::WindowTree).
  std::unique_ptr<MusEmbeddedFrame> CreateMusEmbeddedFrame(
      MusEmbeddedFrameDelegate* mus_embedded_frame_delegate,
      const base::UnguessableToken& token);

 private:
  friend class MusEmbeddedFrame;

  explicit RendererWindowTreeClient(int routing_id);
  ~RendererWindowTreeClient() override;

  void RequestLayerTreeFrameSinkInternal(
      scoped_refptr<viz::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      LayerTreeFrameSinkCallback callback);

  // Called from ~MusEmbeddedFrame to cleanup up internall mapping.
  void OnEmbeddedFrameDestroyed(MusEmbeddedFrame* frame);

  uint32_t GetAndAdvanceNextChangeId() { return ++next_change_id_; }

  // mojom::RenderWidgetWindowTreeClient:
  void Embed(uint32_t frame_routing_id,
             const base::UnguessableToken& token) override;
  void DestroyFrame(uint32_t frame_routing_id) override;

  // ws::mojom::WindowTreeClient:
  // Note: A number of the following are currently not-implemented. Some of
  // these will remain unimplemented in the long-term. Some of the
  // implementations would require some amount of refactoring out of
  // RenderWidget and related classes (e.g. resize, input, ime etc.).
  void OnEmbed(
      ws::mojom::WindowDataPtr root,
      ws::mojom::WindowTreePtr tree,
      int64_t display_id,
      ws::Id focused_window_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbedFromToken(
      const base::UnguessableToken& token,
      ws::mojom::WindowDataPtr root,
      int64_t display_id,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbeddedAppDisconnected(ws::Id window_id) override;
  void OnUnembed(ws::Id window_id) override;
  void OnCaptureChanged(ws::Id new_capture_window_id,
                        ws::Id old_capture_window_id) override;
  void OnFrameSinkIdAllocated(ws::Id window_id,
                              const viz::FrameSinkId& frame_sink_id) override;
  void OnTopLevelCreated(
      uint32_t change_id,
      ws::mojom::WindowDataPtr data,
      int64_t display_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnWindowBoundsChanged(
      ws::Id window_id,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      const base::Optional<viz::LocalSurfaceId>& local_frame_id) override;
  void OnWindowTransformChanged(ws::Id window_id,
                                const gfx::Transform& old_transform,
                                const gfx::Transform& new_transform) override;
  void OnTransientWindowAdded(ws::Id window_id,
                              ws::Id transient_window_id) override;
  void OnTransientWindowRemoved(ws::Id window_id,
                                ws::Id transient_window_id) override;
  void OnWindowHierarchyChanged(
      ws::Id window_id,
      ws::Id old_parent_id,
      ws::Id new_parent_id,
      std::vector<ws::mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(ws::Id window_id,
                         ws::Id relative_window_id,
                         ws::mojom::OrderDirection direction) override;
  void OnWindowDeleted(ws::Id window_id) override;
  void OnWindowVisibilityChanged(ws::Id window_id, bool visible) override;
  void OnWindowOpacityChanged(ws::Id window_id,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowDisplayChanged(ws::Id window_id, int64_t display_id) override;
  void OnWindowParentDrawnStateChanged(ws::Id window_id, bool drawn) override;
  void OnWindowSharedPropertyChanged(
      ws::Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          ws::Id window_id,
                          int64_t display_id,
                          std::unique_ptr<ui::Event> event,
                          bool matches_event_observer) override;
  void OnObservedInputEvent(std::unique_ptr<ui::Event> event) override;
  void OnWindowFocused(ws::Id focused_window_id) override;
  void OnWindowCursorChanged(ws::Id window_id, ui::CursorData cursor) override;
  void OnDragDropStart(const base::flat_map<std::string, std::vector<uint8_t>>&
                           mime_data) override;
  void OnDragEnter(ws::Id window_id,
                   uint32_t event_flags,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   OnDragEnterCallback callback) override;
  void OnDragOver(ws::Id window_id,
                  uint32_t event_flags,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  OnDragOverCallback callback) override;
  void OnDragLeave(ws::Id window_id) override;
  void OnCompleteDrop(ws::Id window_id,
                      uint32_t event_flags,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      OnCompleteDropCallback callback) override;
  void OnPerformDragDropCompleted(uint32_t change_id,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnTopmostWindowChanged(const std::vector<ws::Id>& topmost_ids) override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(ws::Id window_id) override;
  void GetScreenProviderObserver(
      ws::mojom::ScreenProviderObserverAssociatedRequest observer) override;
  void OnOcclusionStateChanged(
      ws::Id window_id,
      ws::mojom::OcclusionState occlusion_state) override;

  const int routing_id_;
  ws::Id root_window_id_ = 0u;
  bool visible_ = false;
  scoped_refptr<viz::ContextProvider> pending_context_provider_;
  gpu::GpuMemoryBufferManager* pending_gpu_memory_buffer_manager_ = nullptr;
  LayerTreeFrameSinkCallback pending_layer_tree_frame_sink_callback_;
  ws::mojom::WindowTreePtr tree_;
  mojo::Binding<ws::mojom::WindowTreeClient> binding_;
  mojo::Binding<mojom::RenderWidgetWindowTreeClient>
      render_widget_window_tree_client_binding_;
  ws::ClientSpecificId next_window_id_ = 0;
  uint32_t next_change_id_ = 0;

  // Set of MusEmbeddedFrames. They are owned by the corresponding
  // RenderFrameProxy.
  base::flat_set<MusEmbeddedFrame*> embedded_frames_;

  // Because RenderFrameProxy is created from an IPC message on a different
  // pipe it's entirely possible Embed() may be called before the
  // RenderFrameProxy is created. If Embed() is called before the
  // RenderFrameProxy is created the WindowTreeClient is added here.
  std::map<uint32_t, base::UnguessableToken> pending_frames_;

  DISALLOW_COPY_AND_ASSIGN(RendererWindowTreeClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_RENDERER_WINDOW_TREE_CLIENT_H_

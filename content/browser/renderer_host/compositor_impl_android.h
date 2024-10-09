// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include <stddef.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/paint_holding_commit_trigger.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/compositor.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/privileged/mojom/compositing/begin_frame_observer.mojom.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/compositor/host_begin_frame_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gl/android/scoped_a_native_window.h"

namespace cc::slim {
class Layer;
}  // namespace cc::slim

namespace viz {
class FrameSinkId;
class HostDisplayClient;
}  // namespace viz

namespace content {
class CompositorClient;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl : public Compositor,
                                      public cc::slim::LayerTreeClient,
                                      public ui::UIResourceProvider,
                                      public ui::WindowAndroidCompositor,
                                      public viz::HostFrameSinkClient,
                                      public display::DisplayObserver {
 public:
  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);

  CompositorImpl(const CompositorImpl&) = delete;
  CompositorImpl& operator=(const CompositorImpl&) = delete;

  ~CompositorImpl() override;

  static bool IsInitialized();

  void MaybeCompositeNow();

  // ui::ResourceProvider implementation.
  cc::UIResourceId CreateUIResource(cc::UIResourceClient* client) override;
  void DeleteUIResource(cc::UIResourceId resource_id) override;
  bool SupportsETC1NonPowerOfTwo() const override;

  // Test functions:
  void SetVisibleForTesting(bool visible) { SetVisible(visible); }
  void SetSwapCompletedWithSizeCallbackForTesting(
      base::RepeatingCallback<void(const gfx::Size&)> cb) {
    swap_completed_with_size_for_testing_ = std::move(cb);
  }
  cc::slim::LayerTree* GetLayerTreeForTesting() const { return host_.get(); }

  void AddSimpleBeginFrameObserver(
      ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs);
  void RemoveSimpleBeginFrameObserver(
      ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs);

  void AddFrameSubmissionObserver(FrameSubmissionObserver* observer) override;
  void RemoveFrameSubmissionObserver(
      FrameSubmissionObserver* observer) override;

 private:
  class AndroidHostDisplayClient;
  class ScopedCachedBackBuffer;
  class ReadbackRefImpl;

  // Compositor implementation.
  void SetRootWindow(gfx::NativeWindow root_window) override;
  void SetRootLayer(scoped_refptr<cc::slim::Layer> root) override;
  void SetSurface(
      const base::android::JavaRef<jobject>& surface,
      bool can_be_used_with_surface_control,
      const base::android::JavaRef<jobject>& host_input_token) override;
  void SetBackgroundColor(int color) override;
  void SetWindowBounds(const gfx::Size& size) override;
  const gfx::Size& GetWindowBounds() override;
  void SetRequiresAlphaChannel(bool flag) override;
  void SetNeedsComposite() override;
  base::WeakPtr<ui::UIResourceProvider> GetUIResourceProvider() override;
  ui::ResourceManager& GetResourceManager() override;
  void CacheBackBufferForCurrentSurface() override;
  void EvictCachedBackBuffer() override;
  void PreserveChildSurfaceControls() override;
  void RequestPresentationTimeForNextFrame(
      PresentationTimeCallback callback) override;
  void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulPresentationTimeCallback callback) override;
  void SetDidSwapBuffersCallbackEnabled(bool enable) override;

  // cc::slim::LayerTreeClient implementation.
  void BeginFrame(const viz::BeginFrameArgs& args) override;
  void DidReceiveCompositorFrameAck() override;
  void RequestNewFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override;

  // WindowAndroidCompositor implementation.
  ui::WindowAndroidCompositor::ScopedKeepSurfaceAliveCallback
  TakeScopedKeepSurfaceAliveCallback(const viz::SurfaceId& surface_id) override;
  void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<viz::CopyOutputRequest> request) override;
  void SetNeedsAnimate() override;
  viz::FrameSinkId GetFrameSinkId() override;
  void AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) override;
  void RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) override;
  bool IsDrawingFirstVisibleFrame() const override;
  void SetVSyncPaused(bool paused) override;
  void OnUpdateRefreshRate(float refresh_rate) override;
  void OnUpdateSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) override;
  void OnUpdateOverlayTransform() override;
  std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      base::TimeDelta timeout) override;
  void PostRequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulPresentationTimeCallback callback) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

  // display::DisplayObserver implementation.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  void SetVisible(bool visible);
  void CreateLayerTreeHost();

  void HandlePendingLayerTreeFrameSinkRequest();

  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);
  void DidSwapBuffers(const gfx::Size& swap_size);

  void DetachRootWindow();

  // Helper functions to perform delayed cleanup after the compositor is no
  // longer visible on low-end devices.
  void EnqueueLowEndBackgroundCleanup();
  void DoLowEndBackgroundCleanup();

  // Returns a new surface ID when in surface-synchronization mode. Otherwise
  // returns an empty surface.
  viz::LocalSurfaceId GenerateLocalSurfaceId();

  // Tears down the display for both Viz and non-Viz, unregistering the root
  // frame sink ID in the process.
  void TearDownDisplayAndUnregisterRootFrameSink();

  // Registers the root frame sink ID.
  void RegisterRootFrameSink();

  // Called with the result of context creation for the root frame sink.
  void OnContextCreationResult(gpu::ContextResult context_result);
  void OnFatalOrSurfaceContextCreationFailure(
      gpu::ContextResult context_result);

  // Viz specific functions:
  void InitializeVizLayerTreeFrameSink(
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider);

  void MaybeUpdateObserveBeginFrame();

  using PendingSurfaceCopyId =
      base::StrongAlias<struct PendingSurfaceCopyIdTag, uint32_t>;
  void RemoveScopedKeepSurfaceAlive(
      const PendingSurfaceCopyId& scoped_keep_surface_alive_id);

  viz::FrameSinkId frame_sink_id_;

  // root_layer_ is the persistent internal root layer, while subroot_layer_
  // is the one attached by the compositor client.
  scoped_refptr<cc::slim::Layer> subroot_layer_;

  // Destruction order matters here:
  std::unique_ptr<cc::slim::LayerTree> host_;
  ui::ResourceManagerImpl resource_manager_;

  gfx::DisplayColorSpaces display_color_spaces_;
  gfx::Size size_;
  bool requires_alpha_channel_ = false;

  gl::ScopedANativeWindow window_;
  gpu::SurfaceHandle surface_handle_;
  std::unique_ptr<ScopedCachedBackBuffer> cached_back_buffer_;

  raw_ptr<CompositorClient> client_;

  gfx::NativeWindow root_window_ = gfx::NativeWindow();

  // Whether we need to update animations on the next composite.
  bool needs_animate_;

  // The number of SubmitFrame calls that have not returned and ACK'd from
  // the GPU thread.
  unsigned int pending_frames_;

  // Whether there is a LayerTreeFrameSink request pending from the current
  // |host_|. Becomes |true| if RequestNewFrameSink is called, and
  // |false| if |host_| is deleted or we succeed in creating *and* initializing
  // a LayerTreeFrameSink (which is essentially the contract with cc).
  bool layer_tree_frame_sink_request_pending_;

  gpu::Capabilities gpu_capabilities_;
  std::unordered_set<viz::FrameSinkId, viz::FrameSinkIdHash>
      pending_child_frame_sink_ids_;
  bool has_submitted_frame_since_became_visible_ = false;

  // Viz-specific members for communicating with the display.
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private_;
  std::unique_ptr<viz::HostDisplayClient> display_client_;
  bool vsync_paused_ = false;

  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;

  // Test-only. Called when we are notified of a swap.
  base::RepeatingCallback<void(const gfx::Size&)>
      swap_completed_with_size_for_testing_;

  size_t num_of_consecutive_surface_failures_ = 0u;

  bool enable_swap_completion_callbacks_ = false;

  // Listen to display density change events and update painted device scale
  // factor accordingly.
  display::ScopedDisplayObserver display_observer_{this};

  ui::CompositorLockManager lock_manager_;

  ui::HostBeginFrameObserver::SimpleBeginFrameObserverList
      simple_begin_frame_observers_;
  std::unique_ptr<ui::HostBeginFrameObserver> host_begin_frame_observer_;

  base::ObserverList<FrameSubmissionObserver> frame_submission_observers_;

  // Tracks a list of pending `viz::CopyOutputRequest`s.
  PendingSurfaceCopyId pending_surface_copy_id_ = PendingSurfaceCopyId(0u);
  base::flat_map<PendingSurfaceCopyId,
                 std::unique_ptr<cc::slim::LayerTree::ScopedKeepSurfaceAlive>>
      pending_surface_copies_;

  base::WeakPtrFactory<CompositorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

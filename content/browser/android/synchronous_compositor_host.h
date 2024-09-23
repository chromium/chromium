// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/synchronous_compositor.mojom.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace ui {
struct DidOverscrollParams;
}

namespace viz {
class HostFrameSinkManager;
}

namespace content {

class RenderProcessHost;
class RenderWidgetHostViewAndroid;
class SynchronousCompositorClient;
class SynchronousCompositorSyncCallBridge;

class CONTENT_EXPORT SynchronousCompositorHost
    : public SynchronousCompositor,
      public blink::mojom::SynchronousCompositorHost,
      public viz::BeginFrameObserver {
 public:
  static std::unique_ptr<SynchronousCompositorHost> Create(
      RenderWidgetHostViewAndroid* rwhva,
      const viz::FrameSinkId& frame_sink_id,
      viz::HostFrameSinkManager* host_frame_sink_manager);

  SynchronousCompositorHost(const SynchronousCompositorHost&) = delete;
  SynchronousCompositorHost& operator=(const SynchronousCompositorHost&) =
      delete;

  ~SynchronousCompositorHost() override;

  // SynchronousCompositor overrides.
  void OnCompositorVisible() override;
  void OnCompositorHidden() override;
  scoped_refptr<FrameFuture> DemandDrawHwAsync(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas, bool software_canvas) override;
  void ReturnResources(uint32_t layer_tree_frame_sink_id,
                       std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) override;
  void DidPresentCompositorFrames(viz::FrameTimingDetailsMap timing_details,
                                  uint32_t frame_token) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  float GetVelocityInPixelsPerSecond() override;
  void DidBecomeActive() override;
  void DidChangeRootLayerScrollOffset(const gfx::PointF& root_offset) override;
  void SynchronouslyZoomBy(float zoom_delta, const gfx::Point& anchor) override;
  void OnComputeScroll(base::TimeTicks animation_time) override;
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source) override;
  void DidInvalidate() override;
  void WasEvicted() override;

  ui::ViewAndroid::CopyViewCallback GetCopyViewCallback();
  void DidOverscroll(const ui::DidOverscrollParams& over_scroll_params);

  // Called by SynchronousCompositorSyncCallBridge.
  void UpdateFrameMetaData(
      uint32_t version,
      viz::CompositorFrameMetadata frame_metadata,
      std::optional<viz::LocalSurfaceId> new_local_surface_id);
  void BeginFrameComplete(
      blink::mojom::SyncCompositorCommonRendererParamsPtr params);

  // Called when the mojo channel should be created.
  void InitMojo();

  SynchronousCompositorClient* client() { return client_; }

  RenderProcessHost* GetRenderProcessHost();

  void RequestOneBeginFrame();

  void AddBeginFrameCompletionCallback(base::OnceClosure callback);

  const viz::FrameSinkId& GetFrameSinkId() const { return frame_sink_id_; }
  viz::SurfaceId GetSurfaceId() const;

  // blink::mojom::SynchronousCompositorHost overrides.
  void LayerTreeFrameSinkCreated() override;
  void UpdateState(
      blink::mojom::SyncCompositorCommonRendererParamsPtr params) override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override;

  // viz::BeginFrameObserver implementation.
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

 private:
  enum BeginFrameRequestType {
    BEGIN_FRAME = 1 << 0,
    PERSISTENT_BEGIN_FRAME = 1 << 1
  };

  class ScopedSendZeroMemory;
  struct SharedMemoryWithSize;
  friend class ScopedSetZeroMemory;
  friend class SynchronousCompositorBase;
  FRIEND_TEST_ALL_PREFIXES(SynchronousCompositorBrowserTest,
                           RenderWidgetHostViewAndroidReuse);

  SynchronousCompositorHost(RenderWidgetHostViewAndroid* rwhva,
                            const viz::FrameSinkId& frame_sink_id,
                            viz::HostFrameSinkManager* host_frame_sink_manager,
                            bool use_in_proc_software_draw);
  SynchronousCompositor::Frame DemandDrawHw(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority);
  bool DemandDrawSwInProc(SkCanvas* canvas);
  void SetSoftwareDrawSharedMemoryIfNeeded(size_t stride, size_t buffer_size);
  void SendZeroMemory();
  blink::mojom::SynchronousCompositor* GetSynchronousCompositor();
  // Whether the synchronous compositor host is ready to
  // handle blocking calls.
  bool IsReadyForSynchronousCall();
  void UpdateRootLayerStateOnClient();

  void SendBeginFramePaused();
  void SendBeginFrame(viz::BeginFrameArgs args);
  void AddBeginFrameRequest(BeginFrameRequestType request);
  void ClearBeginFrameRequest(BeginFrameRequestType request);

  const raw_ptr<RenderWidgetHostViewAndroid> rwhva_;
  const raw_ptr<SynchronousCompositorClient> client_;
  const viz::FrameSinkId frame_sink_id_;
  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  const bool use_in_process_zero_copy_software_draw_;
  mojo::AssociatedRemote<blink::mojom::SynchronousCompositor> sync_compositor_;
  mojo::AssociatedReceiver<blink::mojom::SynchronousCompositorHost>
      host_receiver_{this};

  viz::LocalSurfaceId local_surface_id_;

  bool registered_with_filter_ = false;

  size_t bytes_limit_;
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // Make sure to send a synchronous IPC that succeeds first before sending
  // asynchronous ones. This shouldn't be needed. However we may have come
  // to rely on sending a synchronous message first on initialization. So
  // with an abundance of caution, keep that behavior until we are sure this
  // isn't required.
  bool allow_async_draw_ = false;

  // Indicates begin frames are paused from the browser.
  bool begin_frame_paused_ = false;

  // Updated by both renderer and browser. This is in physical pixels.
  gfx::PointF root_scroll_offset_;

  float velocity_in_pixels_per_second_ = 0.f;
  base::TimeDelta last_begin_frame_time_delta_;

  // Indicates that whether OnComputeScroll is called or overridden. The
  // fling_controller should advance the fling only when OnComputeScroll is not
  // overridden.
  bool on_compute_scroll_called_ = false;

  // Whether `DemandDrawHwAsync` has ever been called.
  bool draw_hw_called_ = false;

  // From renderer.
  uint32_t renderer_param_version_;
  uint32_t need_invalidate_count_;
  bool invalidate_needs_draw_;
  uint32_t did_activate_pending_tree_count_;
  uint32_t frame_metadata_version_ = 0u;
  // Physical pixel when use-zoom-for-dsf is enabled, otherwise in dip.
  gfx::PointF max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_ = 0.f;
  float min_page_scale_factor_ = 0.f;
  float max_page_scale_factor_ = 0.f;

  // If the last surface was evicted.
  bool was_evicted_ = false;

  scoped_refptr<SynchronousCompositorSyncCallBridge> bridge_;

  // Indicates whether and for what reason a request for begin frames has been
  // issued. Used to control action dispatch at the next |OnBeginFrame()| call.
  uint32_t outstanding_begin_frame_requests_ = 0;

  uint32_t num_invalidates_since_last_draw_ = 0u;
  uint32_t num_begin_frames_to_skip_ = 0u;

  // The begin frame source being observed.  Null if none.
  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;
  viz::BeginFrameArgs last_begin_frame_args_;
  viz::FrameTimingDetailsMap timing_details_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

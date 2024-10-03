// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_AR_COMPOSITOR_FRAME_SINK_H_
#define DEVICE_VR_ANDROID_ARCORE_AR_COMPOSITOR_FRAME_SINK_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_display_client.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace ui {
class WindowAndroid;
}

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace device {
struct WebXrFrame;

// This class creates a RootCompositorFrameSink for use with the Viz Compositor
// and manages building an appropriate ArFrame based off of the data from the
// passed in WebXrFrames.
class ArCompositorFrameSink : public viz::mojom::CompositorFrameSinkClient {
 public:
  // Enum used to indicate to SubmitFrame if content from the Renderer is valid
  // or not. If it's invalid, only the Camera Image (and DOM Overlay if
  // applicable), will be composited into a frame.
  enum FrameType {
    kMissingWebXrContent,
    kHasWebXrContent,
  };

  // Used when the compositor acknowledges that it is ready to begin processing
  // or working on the frame that previously requested to Begin.
  using BeginFrameCallback =
      base::RepeatingCallback<void(const viz::BeginFrameArgs& args,
                                   const viz::FrameTimingDetailsMap&)>;

  using CompositorReceivedFrameCallback = base::RepeatingClosure;

  // This callback signals when all of the resources associated with the given
  // frame have been "returned" by the Compositor. Note that just because the
  // resources have been returned, does not mean that they can immediately be
  // re-used. Any |reclaimed_sync_tokens| on the |WebXrFrame| must be waited
  // on (and a gpu-context server wait issued), before the frame can be reused.
  using RenderingFinishedCallback = base::RepeatingCallback<void(WebXrFrame*)>;

  // Once a BeginFrame call has been issued, we cannot issue another one until
  // the compositor is ready. Note that this will be sometime *after* the
  // previously begun frame has been submitted back to the compositor.
  using CanIssueNewFrameCallback = base::RepeatingClosure;

  ArCompositorFrameSink(
      scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner,
      BeginFrameCallback on_begin_frame,
      CompositorReceivedFrameCallback on_compositor_received_frame,
      RenderingFinishedCallback on_rendering_finished,
      CanIssueNewFrameCallback on_can_issue_new_frame);

  ~ArCompositorFrameSink() override;

  ArCompositorFrameSink(const ArCompositorFrameSink&) = delete;
  ArCompositorFrameSink& operator=(const ArCompositorFrameSink&) = delete;

  bool IsInitialized() { return is_initialized_; }
  bool CanIssueBeginFrame() { return can_issue_new_begin_frame_; }
  viz::FrameSinkId FrameSinkId();

  void Initialize(const scoped_refptr<base::SingleThreadTaskRunner>&
                      main_thread_task_runner,
                  gpu::SurfaceHandle surface_handle,
                  ui::WindowAndroid* root_window,
                  const gfx::Size& frame_size,
                  XrFrameSinkClient* xr_frame_sink_client,
                  DomOverlaySetup dom_setup,
                  base::OnceCallback<void(bool)> on_initialized,
                  base::OnceClosure on_bindings_disconnect);

  // This will tick the ExternalBeginFrameController to start a frame in the viz
  // process. The BeginFrameCallback (an acknowledgement of this frame from the
  // viz side), should be fired before attempting to call SubmitFrame or
  // DidNotProduceFrame.
  void RequestBeginFrame(base::TimeDelta interval, base::TimeTicks deadline);

  // We don't take ownership of the WebXrFrame, it is the responsibility of the
  // caller to keep this frame alive until the RenderingFinishedCallback is
  // triggered.
  void SubmitFrame(WebXrFrame* xr_frame, FrameType frame_type);
  void DidNotProduceFrame(WebXrFrame* xr_frame);

  // For the purposes of our callers we *can* composite DOM content as long as
  // we think we *should* try to do so. This prevents issues with this check if
  // we temporarily have an invalid surface id for some reason.
  bool CanCompositeDomContent() { return should_composite_dom_overlay_; }

 private:
  bool IsOnGlThread() const;

  void OnRootCompositorFrameSinkReady(DomOverlaySetup dom_setup);

  viz::CompositorFrame CreateFrame(WebXrFrame* xr_frame, FrameType frame_type);

  // viz::mojom::CompositorFrameSinkClient:
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override;

  // Callback that we bind when submitting a frame. It lets us know that viz
  // will allow us to call "BeginFrame" again.
  void OnFrameSubmitAck(const viz::BeginFrameAck& ack);

  void OnBindingsDisconnect();
  void CloseBindingsIfOpen();

  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;
  raw_ptr<XrFrameSinkClient> xr_frame_sink_client_;
  gfx::Size frame_size_;
  bool can_issue_new_begin_frame_ = true;
  bool is_initialized_ = false;

  // Each frame should only have two resources, and there should really only
  // be two frames with outstanding resources at a time (though we also have a
  // max size to our swapchain), so we generally expect this to be ~4 items, but
  // it could be 2*Swapchain size in a worst case.
  base::flat_map<viz::ResourceId, raw_ptr<WebXrFrame, CtnExperimental>>
      id_to_frame_map_;

  base::OnceCallback<void(bool)> on_initialized_;
  BeginFrameCallback on_begin_frame_;
  CompositorReceivedFrameCallback on_compositor_received_frame_;
  RenderingFinishedCallback on_rendering_finished_;
  CanIssueNewFrameCallback on_can_issue_new_frame_;
  base::OnceClosure on_bindings_disconnect_;

  // State for various Viz IDs
  viz::ParentLocalSurfaceIdAllocator allocator_;
  viz::FrameTokenGenerator next_frame_token_;
  viz::ResourceIdGenerator resource_id_generator_;
  uint64_t next_begin_frame_id_ = 1;
  bool should_composite_dom_overlay_ = false;

  // Mojom remotes and helpers
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private_;
  mojo::AssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote_;
  mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
      frame_controller_remote_;
  mojo::Receiver<viz::mojom::CompositorFrameSinkClient> sink_receiver_{this};
  std::unique_ptr<viz::HostDisplayClient> display_client_;

  // Must be the last member so that it will be destructed first.
  base::WeakPtrFactory<ArCompositorFrameSink> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_AR_COMPOSITOR_FRAME_SINK_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/common/content_export.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
class CompositorFrameSinkSupport;
}

namespace content {

class DelegatedFrameHost;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient {
 public:
  virtual ~DelegatedFrameHostClient() {}

  virtual ui::Layer* DelegatedFrameHostGetLayer() const = 0;
  virtual bool DelegatedFrameHostIsVisible() const = 0;
  // Returns the color that the resize gutters should be drawn with.
  virtual SkColor DelegatedFrameHostGetGutterColor() const = 0;
  virtual void OnBeginFrame(base::TimeTicks frame_time) = 0;
  virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
  virtual float GetDeviceScaleFactor() const = 0;
  virtual void WasEvicted() = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public ui::ContextFactoryObserver,
      public viz::FrameEvictorClient,
      public viz::mojom::CompositorFrameSinkClient,
      public viz::HostFrameSinkClient {
 public:
  // |should_register_frame_sink_id| flag indicates whether DelegatedFrameHost
  // is responsible for registering the associated FrameSinkId with the
  // compositor or not. This is set only on non-aura platforms, since aura is
  // responsible for doing the appropriate [un]registration.
  DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                     DelegatedFrameHostClient* client,
                     bool should_register_frame_sink_id);
  ~DelegatedFrameHost() override;

  // ui::CompositorObserver implementation.
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingChildResizing(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // ui::ContextFactoryObserver implementation.
  void OnLostSharedContext() override;
  void OnLostVizProcess() override;

  // FrameEvictorClient implementation.
  void EvictDelegatedFrame() override;

  void ResetFallbackToFirstNavigationSurface();

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(
      uint32_t presentation_token,
      const gfx::PresentationFeedback& feedback) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  // Public interface exposed to RenderWidgetHostView.

  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink);
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list);
  void ClearDelegatedFrame();
  void WasHidden();
  // TODO(ccameron): Include device scale factor here.
  void WasShown(const viz::LocalSurfaceId& local_surface_id,
                const gfx::Size& dip_size,
                bool record_presentation_time);
  void EmbedSurface(const viz::LocalSurfaceId& local_surface_id,
                    const gfx::Size& dip_size,
                    cc::DeadlinePolicy deadline_policy);
  bool HasSavedFrame() const;
  void AttachToCompositor(ui::Compositor* compositor);
  void DetachFromCompositor();
  // Note: |src_subrect| is specified in DIP dimensions while |output_size|
  // expects pixels. If |src_subrect| is empty, the entire surface area is
  // copied.
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);
  bool CanCopyFromCompositingSurface() const;
  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  // Given the SurfaceID of a Surface that is contained within this class'
  // Surface, find the relative transform between the Surfaces and apply it
  // to a point. Returns false if a Surface has not yet been created or if
  // |original_surface| is not embedded within our current Surface.
  bool TransformPointToLocalCoordSpaceLegacy(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point);

  void SetNeedsBeginFrames(bool needs_begin_frames);
  void SetWantsAnimateOnlyBeginFrames();
  void DidNotProduceFrame(const viz::BeginFrameAck& ack);

  // Returns the surface id for the most recently embedded surface.
  viz::SurfaceId GetCurrentSurfaceId() const {
    return viz::SurfaceId(frame_sink_id_, local_surface_id_);
  }
  viz::CompositorFrameSinkSupport* GetCompositorFrameSinkSupportForTesting() {
    return support_.get();
  }

  bool HasPrimarySurface() const;
  bool HasFallbackSurface() const;

  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }

  gfx::Size CurrentFrameSizeInDipForTesting() const {
    return current_frame_size_in_dip_;
  }

  void DidNavigate();

  void WindowTitleChanged(const std::string& title);

  // If our SurfaceLayer doesn't have a fallback, use the fallback info of
  // |other|.
  void TakeFallbackContentFrom(DelegatedFrameHost* other);

  base::WeakPtr<DelegatedFrameHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class DelegatedFrameHostClient;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);

  SkColor GetGutterColor() const;

  void CreateCompositorFrameSinkSupport();
  void ResetCompositorFrameSinkSupport();

  const viz::FrameSinkId frame_sink_id_;
  DelegatedFrameHostClient* const client_;
  const bool enable_viz_;
  const bool should_register_frame_sink_id_;
  ui::Compositor* compositor_ = nullptr;

  // The LocalSurfaceId of the currently embedded surface.
  viz::LocalSurfaceId local_surface_id_;
  // The size of the above surface (updated at the same time).
  gfx::Size surface_dip_size_;

  // In non-surface sync, this is the size of the most recently activated
  // surface (which is suitable for calculating gutter size). In surface sync,
  // this is most recent size set in EmbedSurface.
  // TODO(ccameron): The meaning of "current" should be made more clear here.
  gfx::Size current_frame_size_in_dip_;

  viz::HostFrameSinkManager* const host_frame_sink_manager_;

  // State for rendering into a Surface.
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  bool needs_begin_frame_ = false;

  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  viz::LocalSurfaceId first_local_surface_id_after_navigation_;

  base::WeakPtrFactory<DelegatedFrameHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

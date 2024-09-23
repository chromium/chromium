// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/common/content_export.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-forward.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"

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
  virtual void OnFrameTokenChanged(uint32_t frame_token,
                                   base::TimeTicks activation_time) = 0;
  virtual float GetDeviceScaleFactor() const = 0;
  virtual void InvalidateLocalSurfaceIdOnEviction() = 0;
  virtual viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction() = 0;
  virtual bool ShouldShowStaleContentOnEviction() = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public viz::FrameEvictorClient,
      public viz::HostFrameSinkClient {
 public:
  enum class FrameEvictionState {
    kNotStarted = 0,          // Frame eviction is ready.
    kPendingEvictionRequests  // Frame eviction is paused with pending requests.
  };

  class Observer {
   public:
    virtual void OnFrameEvictionStateChanged(FrameEvictionState new_state) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // |should_register_frame_sink_id| flag indicates whether DelegatedFrameHost
  // is responsible for registering the associated FrameSinkId with the
  // compositor or not. This is set only on non-aura platforms, since aura is
  // responsible for doing the appropriate [un]registration.
  DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                     DelegatedFrameHostClient* client,
                     bool should_register_frame_sink_id);

  DelegatedFrameHost(const DelegatedFrameHost&) = delete;
  DelegatedFrameHost& operator=(const DelegatedFrameHost&) = delete;

  ~DelegatedFrameHost() override;

  void AddObserverForTesting(Observer* observer);
  void RemoveObserverForTesting(Observer* observer);

  // ui::CompositorObserver implementation.
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void ClearFallbackSurfaceForCommitPending();
  void ResetFallbackToFirstNavigationSurface();

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  // Public interface exposed to RenderWidgetHostView.

  // kOccluded means the native window for the host was
  // occluded/hidden, kOther is for other causes, e.g., a tab became a
  // background tab.
  enum class HiddenCause { kOccluded, kOther };

  void WasHidden(HiddenCause cause);

  // TODO(ccameron): Include device scale factor here.
  void WasShown(const viz::LocalSurfaceId& local_surface_id,
                const gfx::Size& dip_size,
                blink::mojom::RecordContentToVisibleTimeRequestPtr
                    record_tab_switch_time_request);

  // Called to request the presentation time for the next frame or cancel any
  // requests when the RenderWidget's visibility state is not changing. If the
  // visibility state is changing call WasHidden or WasShown instead.
  void RequestSuccessfulPresentationTimeForNextFrame(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request);
  void CancelSuccessfulPresentationTimeRequest();

  void EmbedSurface(const viz::LocalSurfaceId& local_surface_id,
                    const gfx::Size& dip_size,
                    cc::DeadlinePolicy deadline_policy);
  bool HasSavedFrame() const;
  void AttachToCompositor(ui::Compositor* compositor);
  void DetachFromCompositor();

  // Copies |src_subrect| from the compositing surface into a bitmap (first
  // overload) or texture (second overload). |output_size| specifies the size of
  // the output bitmap or texture.
  // Note: |src_subrect| is specified in DIP dimensions while |output_size|
  // expects pixels. If |src_subrect| is empty, the entire surface area is
  // copied.
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);
  void CopyFromCompositingSurfaceAsTexture(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      viz::CopyOutputRequest::CopyOutputRequestCallback callback);

  bool CanCopyFromCompositingSurface() const;
  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  // FrameEvictorClient:
  // Returns the surface id for the most recently embedded surface.
  viz::SurfaceId GetCurrentSurfaceId() const override;

  bool HasPrimarySurface() const;
  bool HasFallbackSurface() const;

  viz::SurfaceId GetFallbackSurfaceIdForTesting() const;

  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }

  gfx::Size CurrentFrameSizeInDipForTesting() const {
    return current_frame_size_in_dip_;
  }

  void DidNavigate();

  // Navigation to a different page than the current one has begun. Caches the
  // current LocalSurfaceId information so that old content can be evicted if
  // navigation fails to complete.
  void DidNavigateMainFramePreCommit();

  // Called when the page has just entered BFCache.
  void DidEnterBackForwardCache();

  void WindowTitleChanged(const std::string& title);

  // If our SurfaceLayer doesn't have a fallback, use the fallback info of
  // |other|.
  void TakeFallbackContentFrom(DelegatedFrameHost* other);

  base::WeakPtr<DelegatedFrameHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  const ui::Layer* stale_content_layer() const {
    return stale_content_layer_.get();
  }

  FrameEvictionState frame_eviction_state() const {
    return frame_eviction_state_;
  }

  const viz::FrameEvictor* GetFrameEvictorForTesting() const {
    return frame_evictor_.get();
  }

  viz::SurfaceId GetPreNavigationSurfaceIdForTesting() const {
    return GetPreNavigationSurfaceId();
  }

  viz::SurfaceId GetFirstSurfaceIdAfterNavigationForTesting() const;

  void SetIsFrameSinkIdOwner(bool is_owner);

  // This is used to evict also the UI compositor if native occlusion is
  // enabled. This only makes sense on desktop platforms where the UI compositor
  // corresponds to a browser window, and native occlusion is supported.
  static bool ShouldIncludeUiCompositorForEviction();

 private:
  friend class DelegatedFrameHostClient;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraBrowserTest,
                           StaleFrameContentOnEvictionNormal);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraBrowserTest,
                           StaleFrameContentOnEvictionRejected);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraBrowserTest,
                           StaleFrameContentOnEvictionNone);
  FRIEND_TEST_ALL_PREFIXES(NoCompositingRenderWidgetHostViewBrowserTest,
                           BFCachedSurfaceShouldNotBeEvicted);

  // FrameEvictorClient implementation.
  void EvictDelegatedFrame(
      const std::vector<viz::SurfaceId>& surface_ids) override;
  viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction()
      const override;
  viz::SurfaceId GetPreNavigationSurfaceId() const override;

  void DidCopyStaleContent(std::unique_ptr<viz::CopyOutputResult> result);

  void ContinueDelegatedFrameEviction(
      const std::vector<viz::SurfaceId>& surface_ids);

  SkColor GetGutterColor() const;

  void CopyFromCompositingSurfaceInternal(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      viz::CopyOutputRequest::ResultFormat format,
      viz::CopyOutputRequest::ResultDestination destination,
      viz::CopyOutputRequest::CopyOutputRequestCallback callback);

  void SetFrameEvictionStateAndNotifyObservers(
      FrameEvictionState frame_eviction_state);

  const viz::FrameSinkId frame_sink_id_;
  const raw_ptr<DelegatedFrameHostClient> client_;
  const bool should_register_frame_sink_id_;
  raw_ptr<ui::Compositor> compositor_ = nullptr;

  // The LocalSurfaceId of the currently embedded surface.
  //
  // TODO(crbug.com/40274223): this value is a copy of what the browser
  // wants to embed. The source of truth is stored else where. We should
  // consider de-dup this ID.
  viz::LocalSurfaceId local_surface_id_;

  // The size of the above surface (updated at the same time).
  gfx::Size surface_dip_size_;

  // In non-surface sync, this is the size of the most recently activated
  // surface (which is suitable for calculating gutter size). In surface sync,
  // this is most recent size set in EmbedSurface.
  // TODO(ccameron): The meaning of "current" should be made more clear here.
  gfx::Size current_frame_size_in_dip_;

  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  viz::LocalSurfaceId first_local_surface_id_after_navigation_;

  // While navigating we have no active |local_surface_id_|. Track the one from
  // before a navigation, because if the navigation fails to complete, we will
  // need to evict its surface. If the old page enters BFCache, this id is used
  // to restore `local_surface_id_`.
  viz::LocalSurfaceId pre_navigation_local_surface_id_;

  // The fallback ID for BFCache restore. It is set when `this` enters the
  // BFCache and is cleared when resize-while-hidden (which supplies with a
  // latest fallback ID) or after it is used in `EmbedSurface`.
  viz::LocalSurfaceId bfcache_fallback_;

  FrameEvictionState frame_eviction_state_ = FrameEvictionState::kNotStarted;

  // Layer responsible for displaying the stale content for the DFHC when the
  // actual web content frame has been evicted. This will be reset when a new
  // compositor frame is submitted.
  std::unique_ptr<ui::Layer> stale_content_layer_;

  blink::ContentToVisibleTimeReporter tab_switch_time_recorder_;

  // Speculative RenderWidgetHostViews can start with a FrameSinkId owned by the
  // currently committed RenderWidgetHostView. Ownership is transferred when the
  // navigation is committed. This bit tracks whether this DelegatedFrameHost
  // owns its FrameSinkId.
  bool owns_frame_sink_id_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<DelegatedFrameHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

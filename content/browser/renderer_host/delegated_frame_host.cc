// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {
namespace {

// Normalized value [0..1] where 1 is full quality and 0 is empty. This sets
// the quality of the captured texture by reducing its dimensions by this
// factor.
constexpr float kFrameContentCaptureQuality = 0.4f;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                                       DelegatedFrameHostClient* client,
                                       bool should_register_frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      client_(client),
      should_register_frame_sink_id_(should_register_frame_sink_id),
      host_frame_sink_manager_(GetHostFrameSinkManager()),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  CHECK(host_frame_sink_manager_);
  frame_evictor_->SetVisible(client_->DelegatedFrameHostIsVisible());

  stale_content_layer_ =
      std::make_unique<ui::Layer>(ui::LayerType::LAYER_SOLID_COLOR);
  stale_content_layer_->SetVisible(false);
  stale_content_layer_->SetColor(SK_ColorTRANSPARENT);
}

DelegatedFrameHost::~DelegatedFrameHost() {
  CHECK(!compositor_);

  CHECK(host_frame_sink_manager_);
  if (owns_frame_sink_id_) {
    host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_, this);
  }
}

void DelegatedFrameHost::AddObserverForTesting(Observer* observer) {
  observers_.AddObserver(observer);
}

void DelegatedFrameHost::RemoveObserverForTesting(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DelegatedFrameHost::WasShown(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_dip_size,
    blink::mojom::RecordContentToVisibleTimeRequestPtr
        record_tab_switch_time_request) {
  // Cancel any pending frame eviction and unpause it if paused.
  SetFrameEvictionStateAndNotifyObservers(FrameEvictionState::kNotStarted);

  frame_evictor_->SetVisible(true);
  if (record_tab_switch_time_request && compositor_) {
    compositor_->RequestSuccessfulPresentationTimeForNextFrame(
        tab_switch_time_recorder_.TabWasShown(
            true /* has_saved_frames */,
            std::move(record_tab_switch_time_request)));
  }

  // Use the default deadline to synchronize web content with browser UI.
  // TODO(fsamuel): Investigate if there is a better deadline to use here.
  EmbedSurface(new_local_surface_id, new_dip_size,
               cc::DeadlinePolicy::UseDefaultDeadline());

  // Remove stale content that might be displayed.
  if (stale_content_layer_->has_external_content()) {
    stale_content_layer_->SetShowSolidColorContent();
    stale_content_layer_->SetVisible(false);
  }
}

void DelegatedFrameHost::RequestSuccessfulPresentationTimeForNextFrame(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  CHECK(visible_time_request);
  if (!compositor_)
    return;
  // Tab was shown while widget was already painting, eg. due to being
  // captured.
  compositor_->RequestSuccessfulPresentationTimeForNextFrame(
      tab_switch_time_recorder_.TabWasShown(true /* has_saved_frames */,
                                            std::move(visible_time_request)));
}

void DelegatedFrameHost::CancelSuccessfulPresentationTimeRequest() {
  // Tab was hidden while widget keeps painting, eg. due to being captured.
  tab_switch_time_recorder_.TabWasHidden();
}

bool DelegatedFrameHost::HasSavedFrame() const {
  return frame_evictor_->has_surface();
}

void DelegatedFrameHost::WasHidden(HiddenCause cause) {
  tab_switch_time_recorder_.TabWasHidden();
#if BUILDFLAG(IS_WIN)
  // Ignore if the native window was occluded.
  // Windows needs the frame host to display tab previews.
  if (cause == HiddenCause::kOccluded)
    return;
#endif
  frame_evictor_->SetVisible(false);
}

void DelegatedFrameHost::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  CopyFromCompositingSurfaceInternal(
      src_subrect, output_size, viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             std::unique_ptr<viz::CopyOutputResult> result) {
            auto scoped_bitmap = result->ScopedAccessSkBitmap();
            std::move(callback).Run(scoped_bitmap.GetOutScopedBitmap());
          },
          std::move(callback)));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceAsTexture(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  CopyFromCompositingSurfaceInternal(
      src_subrect, output_size, viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kNativeTextures,
      std::move(callback));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceInternal(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    viz::CopyOutputRequest::ResultFormat format,
    viz::CopyOutputRequest::ResultDestination destination,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  auto request = std::make_unique<viz::CopyOutputRequest>(format, destination,
                                                          std::move(callback));

  // It is possible for us to not have a valid surface to copy from. Such as
  // if a navigation fails to complete. In such a case do not attempt to request
  // a copy.
  if (!CanCopyFromCompositingSurface())
    return;

  if (!src_subrect.IsEmpty()) {
    request->set_area(
        gfx::ScaleToRoundedRect(src_subrect, client_->GetDeviceScaleFactor()));
  }
  if (!output_size.IsEmpty()) {
    // The CopyOutputRequest API does not allow fixing the output size. Instead
    // we have the set area and scale in such a way that it would result in the
    // desired output size.
    if (!request->has_area()) {
      request->set_area(gfx::Rect(gfx::ScaleToRoundedSize(
          surface_dip_size_, client_->GetDeviceScaleFactor())));
    }
    request->set_result_selection(gfx::Rect(output_size));
    const gfx::Rect& area = request->area();
    if (area.IsEmpty()) {
      // Viz would normally return an empty result for an empty area.
      // However, this guard here is still necessary to protect against setting
      // an illegal scaling ratio.
      return;
    }
    request->SetScaleRatio(
        gfx::Vector2d(area.width(), area.height()),
        gfx::Vector2d(output_size.width(), output_size.height()));
  }

  // Run result callback on the current thread in case `callback` needs to run
  // on the current thread. See http://crbug.com/1431363.
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  CHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->RequestCopyOfOutput(
      viz::SurfaceId(frame_sink_id_, local_surface_id_), std::move(request));
}

void DelegatedFrameHost::SetFrameEvictionStateAndNotifyObservers(
    FrameEvictionState frame_eviction_state) {
  if (frame_eviction_state_ == frame_eviction_state)
    return;

  frame_eviction_state_ = frame_eviction_state;
  for (auto& obs : observers_)
    obs.OnFrameEvictionStateChanged(frame_eviction_state_);
}

bool DelegatedFrameHost::CanCopyFromCompositingSurface() const {
  return local_surface_id_.is_valid();
}

bool DelegatedFrameHost::HasPrimarySurface() const {
  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetSurfaceId();
  return primary_surface_id && primary_surface_id->is_valid();
}

bool DelegatedFrameHost::HasFallbackSurface() const {
  const viz::SurfaceId* fallback_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetOldestAcceptableFallback();
  return fallback_surface_id && fallback_surface_id->is_valid();
}

viz::SurfaceId DelegatedFrameHost::GetFallbackSurfaceIdForTesting() const {
  const viz::SurfaceId* fallback_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetOldestAcceptableFallback();
  return fallback_surface_id ? *fallback_surface_id : viz::SurfaceId();
}

void DelegatedFrameHost::EmbedSurface(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_dip_size,
    cc::DeadlinePolicy deadline_policy) {
  TRACE_EVENT2("viz", "DelegatedFrameHost::EmbedSurface", "surface_id",
               new_local_surface_id.ToString(), "deadline_policy",
               deadline_policy.ToString());

  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetSurfaceId();

  local_surface_id_ = new_local_surface_id;
  surface_dip_size_ = new_dip_size;

  // The embedding of a new surface completes the navigation process.
  pre_navigation_local_surface_id_ = viz::LocalSurfaceId();

  // Navigations performed while hidden delay embedding until transitioning to
  // becoming visible. So we may not have a valid surface when DidNavigate is
  // called. Cache the first surface here so we have the correct oldest surface
  // to fallback to.
  if (!first_local_surface_id_after_navigation_.is_valid())
    first_local_surface_id_after_navigation_ = local_surface_id_;

  viz::SurfaceId new_primary_surface_id(frame_sink_id_, local_surface_id_);

  if (!client_->DelegatedFrameHostIsVisible()) {
    // If the tab is resized while hidden, advance the fallback so that the next
    // time user switches back to it the page is blank. This is preferred to
    // showing contents of old size. Don't call EvictDelegatedFrame to avoid
    // races when dragging tabs across displays. See https://crbug.com/813157.
    //
    // An empty |current_frame_size_in_dip_| indicates this renderer has never
    // been made visible. This is the case for pre-rendered contents. Don't use
    // the primary id as fallback since it's guaranteed to have no content. See
    // crbug.com/1218238.
    if (!current_frame_size_in_dip_.IsEmpty() &&
        surface_dip_size_ != current_frame_size_in_dip_) {
      client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
          new_primary_surface_id);

      // Invalidates `bfcache_fallback_` as resize-while-hidden has given us the
      // latest `local_surface_id_`.
      bfcache_fallback_ =
          viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
    }
    // Don't update the SurfaceLayer when invisible to avoid blocking on
    // renderers that do not submit CompositorFrames. Next time the renderer
    // is visible, EmbedSurface will be called again. See WasShown.
    return;
  }

  // Ignore empty frames. Extensions often create empty background page frames
  // which shouldn't count against the saved frames.
  if (!new_dip_size.IsEmpty())
    frame_evictor_->OnNewSurfaceEmbedded();

  if (bfcache_fallback_.is_valid()) {
    // Inform Viz to show the primary surface with new ID asap; if the new
    // surface isn't ready, use the fallback.
    deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
    client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
        viz::SurfaceId(frame_sink_id_, bfcache_fallback_));
    bfcache_fallback_ =
        viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
  }

  if (!primary_surface_id ||
      primary_surface_id->local_surface_id() != local_surface_id_) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // On Windows and Linux, we would like to produce new content as soon as
    // possible or the OS will create an additional black gutter. Until we can
    // block resize on surface synchronization on these platforms, we will not
    // block UI on the top-level renderer. The exception to this is if we're
    // using an infinite deadline, in which case we should respect the
    // specified deadline and block UI since that's what was requested.
    if (deadline_policy.policy_type() !=
            cc::DeadlinePolicy::kUseInfiniteDeadline &&
        !current_frame_size_in_dip_.IsEmpty() &&
        current_frame_size_in_dip_ != surface_dip_size_) {
      deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
    }
#endif
    current_frame_size_in_dip_ = surface_dip_size_;
    client_->DelegatedFrameHostGetLayer()->SetShowSurface(
        new_primary_surface_id, current_frame_size_in_dip_, GetGutterColor(),
        deadline_policy, false /* stretch_content_to_fill_bounds */);
    if (compositor_)
      compositor_->OnChildResizing();
  }
}

SkColor DelegatedFrameHost::GetGutterColor() const {
  // In fullscreen mode resizing is uncommon, so it makes more sense to
  // make the initial switch to fullscreen mode look better by using black as
  // the gutter color.
  return client_->DelegatedFrameHostGetGutterColor();
}

void DelegatedFrameHost::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED_IN_MIGRATION();
}

void DelegatedFrameHost::OnFrameTokenChanged(uint32_t frame_token,
                                             base::TimeTicks activation_time) {
  client_->OnFrameTokenChanged(frame_token, activation_time);
}

// CommitPending without a target for TakeFallbackContentFrom. Since we cannot
// guarantee that Navigation will complete, evict our surfaces which are from
// a previous Navigation.
void DelegatedFrameHost::ClearFallbackSurfaceForCommitPending() {
  const viz::SurfaceId* fallback_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetOldestAcceptableFallback();

  // CommitPending failed, and Navigation never completed. Evict our surfaces.
  if (fallback_surface_id && fallback_surface_id->is_valid()) {
    EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
    client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
        viz::SurfaceId());
  }
}

void DelegatedFrameHost::ResetFallbackToFirstNavigationSurface() {
  const viz::SurfaceId* fallback_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetOldestAcceptableFallback();

  // Don't update the fallback if it's already newer than the first id after
  // navigation.
  if (fallback_surface_id &&
      fallback_surface_id->frame_sink_id() == frame_sink_id_ &&
      fallback_surface_id->local_surface_id().IsSameOrNewerThan(
          first_local_surface_id_after_navigation_)) {
    return;
  }

  // If we have a surface from before a navigation, evict it as well.
  if (pre_navigation_local_surface_id_.is_valid() &&
      !first_local_surface_id_after_navigation_.is_valid()) {
    // If we have a valid `pre_navigation_local_surface_id_`, we must not be in
    // BFCache.
    CHECK(!bfcache_fallback_.is_valid());
    EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
  }

  client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
      first_local_surface_id_after_navigation_.is_valid()
          ? viz::SurfaceId(frame_sink_id_,
                           first_local_surface_id_after_navigation_)
          : viz::SurfaceId());
}

void DelegatedFrameHost::EvictDelegatedFrame(
    const std::vector<viz::SurfaceId>& surface_ids) {
  // There is already an eviction request pending.
  if (frame_eviction_state_ == FrameEvictionState::kPendingEvictionRequests) {
    frame_evictor_->OnSurfaceDiscarded();
    return;
  }

  if (!HasSavedFrame()) {
    ContinueDelegatedFrameEviction(surface_ids);
    return;
  }

  // Requests a copy of the compositing surface of the frame if one doesn't
  // already exist. The copy (stale content) will be set on the surface until
  // a new compositor frame is submitted. Setting a stale content prevents blank
  // white screens from being displayed during various animations such as the
  // CrOS overview mode.
  if (client_->ShouldShowStaleContentOnEviction() &&
      !stale_content_layer_->has_external_content()) {
    SetFrameEvictionStateAndNotifyObservers(
        FrameEvictionState::kPendingEvictionRequests);
    auto callback =
        base::BindOnce(&DelegatedFrameHost::DidCopyStaleContent, GetWeakPtr());

    // NOTE: This will not return any texture on non CrOS platforms as hiding
    // the window on non CrOS platform disables drawing all together.
    CopyFromCompositingSurfaceAsTexture(
        gfx::Rect(),
        gfx::ScaleToRoundedSize(surface_dip_size_, kFrameContentCaptureQuality),
        std::move(callback));
  } else {
    ContinueDelegatedFrameEviction(surface_ids);
  }
  frame_evictor_->OnSurfaceDiscarded();
}

viz::FrameEvictorClient::EvictIds
DelegatedFrameHost::CollectSurfaceIdsForEviction() const {
  return client_->CollectSurfaceIdsForEviction();
}

viz::SurfaceId DelegatedFrameHost::GetCurrentSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

viz::SurfaceId DelegatedFrameHost::GetPreNavigationSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, pre_navigation_local_surface_id_);
}

void DelegatedFrameHost::DidCopyStaleContent(
    std::unique_ptr<viz::CopyOutputResult> result) {
  // host may have become visible by the time the request to capture surface is
  // completed.
  if (frame_evictor_->visible() || result->IsEmpty())
    return;

  CHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  CHECK_EQ(result->destination(),
           viz::CopyOutputResult::Destination::kNativeTextures);

// TODO(crbug.com/1227661): Revert https://crrev.com/c/3222541 to re-enable this
// CHECK on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK_NE(frame_eviction_state_, FrameEvictionState::kNotStarted);
#endif
  SetFrameEvictionStateAndNotifyObservers(FrameEvictionState::kNotStarted);
  ContinueDelegatedFrameEviction(
      frame_evictor_->CollectSurfaceIdsForEviction());

  auto transfer_resource = viz::TransferableResource::MakeGpu(
      result->GetTextureResult()->mailbox, GL_TEXTURE_2D, gpu::SyncToken(),
      result->size(), viz::SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */,
      viz::TransferableResource::ResourceSource::kStaleContent);
  viz::CopyOutputResult::ReleaseCallbacks release_callbacks =
      result->TakeTextureOwnership();
  CHECK_EQ(1u, release_callbacks.size());

  if (stale_content_layer_->parent() != client_->DelegatedFrameHostGetLayer())
    client_->DelegatedFrameHostGetLayer()->Add(stale_content_layer_.get());

// TODO(crbug.com/40812011): This DCHECK occasionally gets hit on Chrome OS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(!stale_content_layer_->has_external_content());
#endif
  stale_content_layer_->SetVisible(true);
  stale_content_layer_->SetBounds(gfx::Rect(surface_dip_size_));
  stale_content_layer_->SetTransferableResource(
      transfer_resource, std::move(release_callbacks[0]), surface_dip_size_);
}

void DelegatedFrameHost::ContinueDelegatedFrameEviction(
    const std::vector<viz::SurfaceId>& surface_ids) {
  // Reset primary surface.
  if (HasPrimarySurface()) {
    client_->DelegatedFrameHostGetLayer()->SetShowSurface(
        viz::SurfaceId(), current_frame_size_in_dip_, GetGutterColor(),
        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  }

  if (!HasSavedFrame())
    return;

  // Ensure the list is not empty, otherwise we are silently disconnecting our
  // FrameTree. This prevents the eviction of viz::Surfaces, leading to GPU
  // memory staying allocated. We do allow the surface ids to be empty if we
  // don't have a local surface id, since that means we don't have memory
  // allocated in viz.
  //
  // TODO(b/337467299): determine why we are evicting without finding valid
  // surfaces.
  DCHECK(!local_surface_id_.is_valid() || !surface_ids.empty());
  if (!surface_ids.empty()) {
    CHECK(host_frame_sink_manager_);
    host_frame_sink_manager_->EvictSurfaces(surface_ids);
  }
  client_->InvalidateLocalSurfaceIdOnEviction();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  CHECK_EQ(compositor, compositor_);
  DetachFromCompositor();
  CHECK(!compositor_);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

void DelegatedFrameHost::AttachToCompositor(ui::Compositor* compositor) {
  CHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->AddChildFrameSink(frame_sink_id_);
}

void DelegatedFrameHost::DetachFromCompositor() {
  if (!compositor_)
    return;
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->RemoveChildFrameSink(frame_sink_id_);
  compositor_ = nullptr;
}

void DelegatedFrameHost::DidNavigate() {
  first_local_surface_id_after_navigation_ = local_surface_id_;
}

void DelegatedFrameHost::DidNavigateMainFramePreCommit() {
  // We are navigating to a different page, so the current |local_surface_id_|
  // and the fallback option of |first_local_surface_id_after_navigation_| are
  // no longer valid, as they represent older content from a different source.
  //
  // Cache the current |local_surface_id_| so that if navigation fails we can
  // evict it when transitioning to becoming visible.
  //
  // If the current page enters BFCache, `pre_navigation_local_surface_id_` will
  // be restored as the primary `LocalSurfaceId` for this `DelegatedFrameHost`.
  pre_navigation_local_surface_id_ = local_surface_id_;
  first_local_surface_id_after_navigation_ = viz::LocalSurfaceId();
  local_surface_id_ = viz::LocalSurfaceId();
}

void DelegatedFrameHost::DidEnterBackForwardCache() {
  if (local_surface_id_.is_valid()) {
    // `EmbedSurface` can be called after `DidNavigateMainFramePreCommit` and
    // before `DidEnterBackForwardCache`. This can happen on Mac where the
    // `DelegatedFrameHost` receives an `EmbedSurface` call directly from
    // NSView; this can also happen if there is an on-going Hi-DPI capture on
    // the old frame (see `WebContentsFrameTracker::RenderFrameHostChanged()`).
    //
    // The `EmbedSurface` will invalidate `pre_navigation_local_surface_id_`. In
    // this case we shouldn't restore the `local_surface_id_` nor
    // `bfcache_fallback_`because the surface should embed the latest
    // `local_surface_id_`.
    CHECK(!pre_navigation_local_surface_id_.is_valid());
    CHECK(!bfcache_fallback_.is_valid());
  } else {
    local_surface_id_ = pre_navigation_local_surface_id_;
    bfcache_fallback_ = pre_navigation_local_surface_id_;
    pre_navigation_local_surface_id_ = viz::LocalSurfaceId();
  }
}

void DelegatedFrameHost::WindowTitleChanged(const std::string& title) {
  if (host_frame_sink_manager_)
    host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_, title);
}

void DelegatedFrameHost::TakeFallbackContentFrom(DelegatedFrameHost* other) {
  // If the other view is not showing anything, we can't obtain a fallback.
  if (!other->HasPrimarySurface())
    return;

  // This method should not overwrite the existing fallback. This method is only
  // supposed to be called when the view was just created and there is no
  // existing fallback.
  if (HasFallbackSurface())
    return;

  const viz::SurfaceId* other_primary =
      other->client_->DelegatedFrameHostGetLayer()->GetSurfaceId();

  const viz::SurfaceId* other_fallback =
      other->client_->DelegatedFrameHostGetLayer()
          ->GetOldestAcceptableFallback();

  // In two cases we need to obtain a new fallback from the primary id of the
  // other view instead of using its fallback:
  // - When the other view has no fallback,
  // - When a fallback exists but has a different FrameSinkId or embed token
  // than the primary. If we use the fallback, then the resulting SurfaceRange
  // in this view will not cover any surface with the FrameSinkId / embed token
  // of the old view's primary.
  viz::SurfaceId desired_fallback;
  if (!other_fallback || !other_primary->IsSameOrNewerThan(*other_fallback)) {
    desired_fallback = other_primary->ToSmallestId();
  } else {
    desired_fallback = *other_fallback;
  }

  // If we explicitly tell a BFCached View and its `DelegatedFrameHost` to use
  // a specific fallback, discard the preserved fallback for BFCache. During the
  // BFCache activation (`EmbedSurface`) we will be using the
  // `desired_fallback` instead of `bfcache_fallback_`.
  bfcache_fallback_ =
      viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();

  if (!HasPrimarySurface()) {
    client_->DelegatedFrameHostGetLayer()->SetShowSurface(
        desired_fallback, other->client_->DelegatedFrameHostGetLayer()->size(),
        other->client_->DelegatedFrameHostGetLayer()->background_color(),
        cc::DeadlinePolicy::UseDefaultDeadline(),
        false /* stretch_content_to_fill_bounds */);
  }

  client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
      desired_fallback);
}

viz::SurfaceId DelegatedFrameHost::GetFirstSurfaceIdAfterNavigationForTesting()
    const {
  return viz::SurfaceId(frame_sink_id_,
                        first_local_surface_id_after_navigation_);
}

void DelegatedFrameHost::SetIsFrameSinkIdOwner(bool is_owner) {
  if (is_owner == owns_frame_sink_id_) {
    return;
  }

  owns_frame_sink_id_ = is_owner;
  if (owns_frame_sink_id_) {
    host_frame_sink_manager_->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
    host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                     "DelegatedFrameHost");
  }
}

// static
bool DelegatedFrameHost::ShouldIncludeUiCompositorForEviction() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::FeatureList::IsEnabled(
          features::kApplyNativeOcclusionToCompositor)) {
    return false;
  }

  const std::string type =
      features::kApplyNativeOcclusionToCompositorType.Get();
  return type == features::kApplyNativeOcclusionToCompositorTypeRelease ||
         type ==
             features::kApplyNativeOcclusionToCompositorTypeThrottleAndRelease;
#else
  // ChromeOS does not have native occlusion, and the UI compositor corresponds
  // to the entire display, so we don't evict it. Linux does not have native
  // occlusion support, so we don't know when we can evict it, as it may e.g.
  // be shown in a preview while minimized.
  return false;
#endif
}

}  // namespace content

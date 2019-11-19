// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/common/content_switches.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
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
      enable_viz_(features::IsVizDisplayCompositorEnabled()),
      should_register_frame_sink_id_(should_register_frame_sink_id),
      host_frame_sink_manager_(GetHostFrameSinkManager()),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->AddObserver(this);
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager_->EnableSynchronizationReporting(
      frame_sink_id_, "Compositing.MainFrameSynchronization.Duration");
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHost");
  CreateCompositorFrameSinkSupport();
  frame_evictor_->SetVisible(client_->DelegatedFrameHostIsVisible());

  stale_content_layer_ =
      std::make_unique<ui::Layer>(ui::LayerType::LAYER_SOLID_COLOR);
  stale_content_layer_->SetVisible(false);
  stale_content_layer_->SetColor(SK_ColorTRANSPARENT);
}

DelegatedFrameHost::~DelegatedFrameHost() {
  DCHECK(!compositor_);
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->RemoveObserver(this);

  ResetCompositorFrameSinkSupport();
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
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
    const base::Optional<RecordTabSwitchTimeRequest>&
        record_tab_switch_time_request) {
  // Cancel any pending frame eviction and unpause it if paused.
  SetFrameEvictionStateAndNotifyObservers(FrameEvictionState::kNotStarted);

  frame_evictor_->SetVisible(true);
  if (record_tab_switch_time_request && compositor_) {
    compositor_->RequestPresentationTimeForNextFrame(
        tab_switch_time_recorder_.TabWasShown(
            true /* has_saved_frames */, record_tab_switch_time_request.value(),
            base::TimeTicks::Now()));
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

bool DelegatedFrameHost::HasSavedFrame() const {
  return frame_evictor_->has_surface();
}

void DelegatedFrameHost::WasHidden(HiddenCause cause) {
  tab_switch_time_recorder_.TabWasHidden();
#if defined(OS_WIN)
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
      src_subrect, output_size,
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             std::unique_ptr<viz::CopyOutputResult> result) {
            std::move(callback).Run(result->AsSkBitmap());
          },
          std::move(callback)));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceAsTexture(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  CopyFromCompositingSurfaceInternal(
      src_subrect, output_size,
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE, std::move(callback));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceInternal(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    viz::CopyOutputRequest::ResultFormat format,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  DCHECK(CanCopyFromCompositingSurface());

  auto request =
      std::make_unique<viz::CopyOutputRequest>(format, std::move(callback));

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
  DCHECK(host_frame_sink_manager_);
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

void DelegatedFrameHost::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  needs_begin_frame_ = needs_begin_frames;
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DelegatedFrameHost::SetWantsAnimateOnlyBeginFrames() {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  support_->SetWantsAnimateOnlyBeginFrames();
}

void DelegatedFrameHost::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(!ack.has_damage);
  support_->DidNotProduceFrame(ack);
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

void DelegatedFrameHost::EmbedSurface(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_dip_size,
    cc::DeadlinePolicy deadline_policy) {
  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetSurfaceId();

  local_surface_id_ = new_local_surface_id;
  surface_dip_size_ = new_dip_size;

  viz::SurfaceId new_primary_surface_id(frame_sink_id_, local_surface_id_);

  if (!client_->DelegatedFrameHostIsVisible()) {
    // If the tab is resized while hidden, advance the fallback so that the next
    // time user switches back to it the page is blank. This is preferred to
    // showing contents of old size. Don't call EvictDelegatedFrame to avoid
    // races when dragging tabs across displays. See https://crbug.com/813157.
    if (surface_dip_size_ != current_frame_size_in_dip_) {
      client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
          new_primary_surface_id);
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

  if (!primary_surface_id ||
      primary_surface_id->local_surface_id() != local_surface_id_) {
#if defined(OS_WIN) || defined(USE_X11)
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

void DelegatedFrameHost::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  ResetCompositorFrameSinkSupport();
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
  CreateCompositorFrameSinkSupport();
}

void DelegatedFrameHost::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  std::move(hit_test_region_list));
}

void DelegatedFrameHost::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void DelegatedFrameHost::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->ReclaimResources(resources);
}

void DelegatedFrameHost::OnBeginFramePausedChanged(bool paused) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFramePausedChanged(paused);
}

void DelegatedFrameHost::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED();
}

void DelegatedFrameHost::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHost::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFrame(args, timing_details);
  client_->OnBeginFrame(args.frame_time);
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

  client_->DelegatedFrameHostGetLayer()->SetOldestAcceptableFallback(
      viz::SurfaceId(frame_sink_id_, first_local_surface_id_after_navigation_));
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  // There is already an eviction request pending.
  if (frame_eviction_state_ == FrameEvictionState::kPendingEvictionRequests) {
    frame_evictor_->OnSurfaceDiscarded();
    return;
  }

  if (!HasSavedFrame()) {
    ContinueDelegatedFrameEviction();
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
    ContinueDelegatedFrameEviction();
  }
  frame_evictor_->OnSurfaceDiscarded();
}

void DelegatedFrameHost::DidCopyStaleContent(
    std::unique_ptr<viz::CopyOutputResult> result) {
  // host may have become visible by the time the request to capture surface is
  // completed.
  if (frame_evictor_->visible() || result->IsEmpty())
    return;

  DCHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

  DCHECK_NE(frame_eviction_state_, FrameEvictionState::kNotStarted);
  SetFrameEvictionStateAndNotifyObservers(FrameEvictionState::kNotStarted);
  ContinueDelegatedFrameEviction();

  auto transfer_resource = viz::TransferableResource::MakeGL(
      result->GetTextureResult()->mailbox, GL_LINEAR, GL_TEXTURE_2D,
      result->GetTextureResult()->sync_token, result->size(),
      false /* is_overlay_candidate */);
  std::unique_ptr<viz::SingleReleaseCallback> release_callback =
      result->TakeTextureOwnership();

  if (stale_content_layer_->parent() != client_->DelegatedFrameHostGetLayer())
    client_->DelegatedFrameHostGetLayer()->Add(stale_content_layer_.get());

  DCHECK(!stale_content_layer_->has_external_content());
  stale_content_layer_->SetVisible(true);
  stale_content_layer_->SetBounds(gfx::Rect(surface_dip_size_));
  stale_content_layer_->SetTransferableResource(
      transfer_resource, std::move(release_callback), surface_dip_size_);
}

void DelegatedFrameHost::ContinueDelegatedFrameEviction() {
  // Reset primary surface.
  if (HasPrimarySurface()) {
    client_->DelegatedFrameHostGetLayer()->SetShowSurface(
        viz::SurfaceId(), current_frame_size_in_dip_, GetGutterColor(),
        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  }

  if (!HasSavedFrame())
    return;

  DCHECK(!client_->DelegatedFrameHostIsVisible());
  std::vector<viz::SurfaceId> surface_ids = {
      client_->CollectSurfaceIdsForEviction()};
  // This list could be empty if this frame is not in the frame tree (can happen
  // during navigation, construction, destruction, or in unit tests).
  if (!surface_ids.empty()) {
    DCHECK(std::find(surface_ids.begin(), surface_ids.end(),
                     GetCurrentSurfaceId()) != surface_ids.end());
    DCHECK(host_frame_sink_manager_);
    host_frame_sink_manager_->EvictSurfaces(surface_ids);
  }
  client_->InvalidateLocalSurfaceIdOnEviction();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  DetachFromCompositor();
  DCHECK(!compositor_);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ContextFactoryObserver implementation:

void DelegatedFrameHost::OnLostSharedContext() {}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

void DelegatedFrameHost::AttachToCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
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

void DelegatedFrameHost::CreateCompositorFrameSinkSupport() {
  if (enable_viz_)
    return;

  DCHECK(!support_);
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  DCHECK(host_frame_sink_manager_);
  support_ = host_frame_sink_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->AddChildFrameSink(frame_sink_id_);
  if (needs_begin_frame_)
    support_->SetNeedsBeginFrame(true);
}

void DelegatedFrameHost::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->RemoveChildFrameSink(frame_sink_id_);
  support_.reset();
}

void DelegatedFrameHost::DidNavigate() {
  first_local_surface_id_after_navigation_ = local_surface_id_;
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

}  // namespace content

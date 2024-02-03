// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/surface_chooser_helper.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/promotion_hint_aggregator_impl.h"

namespace media {

namespace {

// Number of frames to defer overlays for when entering fullscreen.  This lets
// blink relayout settle down a bit.  If overlay positions were synchronous,
// then we wouldn't need this.
enum { kFrameDelayForFullscreenLayout = 15 };

// How often do we let the surface chooser try for an overlay?  While we'll
// retry if some relevant state changes on our side (e.g., fullscreen state),
// there's plenty of state that we don't know about (e.g., power efficiency,
// memory pressure => cancelling an old overlay, etc.).  We just let the chooser
// retry every once in a while for those things.
constexpr base::TimeDelta RetryChooserTimeout = base::Seconds(5);

}  // namespace

SurfaceChooserHelper::SurfaceChooserHelper(
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    bool is_overlay_required,
    bool promote_secure_only,
    bool always_use_texture_owner,
    std::unique_ptr<PromotionHintAggregator> promotion_hint_aggregator,
    const base::TickClock* tick_clock)
    : surface_chooser_(std::move(surface_chooser)),
      is_overlay_required_(is_overlay_required),
      promotion_hint_aggregator_(
          promotion_hint_aggregator
              ? std::move(promotion_hint_aggregator)
              : std::make_unique<PromotionHintAggregatorImpl>()),
      tick_clock_(tick_clock ? tick_clock
                             : base::DefaultTickClock::GetInstance()) {
  surface_chooser_state_.is_required = is_overlay_required_;
  surface_chooser_state_.promote_secure_only = promote_secure_only;
  surface_chooser_state_.always_use_texture_owner = always_use_texture_owner;
}

SurfaceChooserHelper::~SurfaceChooserHelper() {}

void SurfaceChooserHelper::SetSecureSurfaceMode(SecureSurfaceMode mode) {
  bool is_secure = false;
  requires_secure_video_surface_ = false;

  switch (mode) {
    case SecureSurfaceMode::kInsecure:
      break;
    case SecureSurfaceMode::kRequested:
      is_secure = true;
      break;
    case SecureSurfaceMode::kRequired:
      is_secure = true;
      requires_secure_video_surface_ = true;
      break;
  }

  surface_chooser_state_.is_secure = is_secure;
  surface_chooser_state_.is_required =
      requires_secure_video_surface_ || is_overlay_required_;
}

void SurfaceChooserHelper::SetIsFullscreen(bool is_fullscreen) {
  // TODO(liberato): AVDA previously only set is_expecting_relayout when
  // getting overlay info, not when checking fullscreen for the first time.
  // This might affect pre-M devices.  I think the pre-M path doesn't care.
  if (is_fullscreen && !surface_chooser_state_.is_fullscreen) {
    // It would be nice if we could just delay until we get a hint from an
    // overlay that's "in fullscreen" in the sense that the CompositorFrame it
    // came from had some flag set to indicate that the renderer was in
    // fullscreen mode when it was generated.  However, even that's hard, since
    // there's no real connection between "renderer finds out about fullscreen"
    // and "blink has completed layouts for it".  The latter is what we really
    // want to know.
    surface_chooser_state_.is_expecting_relayout = true;
    hints_until_clear_relayout_flag_ = kFrameDelayForFullscreenLayout;
  }

  surface_chooser_state_.is_fullscreen = is_fullscreen;
}

void SurfaceChooserHelper::SetVideoRotation(VideoRotation video_rotation) {
  surface_chooser_state_.video_rotation = video_rotation;
}

void SurfaceChooserHelper::SetIsPersistentVideo(bool is_persistent_video) {
  surface_chooser_state_.is_persistent_video = is_persistent_video;
}

void SurfaceChooserHelper::UpdateChooserState(
    std::optional<AndroidOverlayFactoryCB> new_factory) {
  surface_chooser_->UpdateState(std::move(new_factory), surface_chooser_state_);
}

void SurfaceChooserHelper::NotifyPromotionHintAndUpdateChooser(
    const PromotionHintAggregator::Hint& hint,
    bool is_using_overlay) {
  bool update_state = false;

  promotion_hint_aggregator_->NotifyPromotionHint(hint);

  // If we're expecting a full screen relayout, then also use this hint as a
  // notification that another frame has happened.
  if (hints_until_clear_relayout_flag_ > 0) {
    hints_until_clear_relayout_flag_--;
    if (hints_until_clear_relayout_flag_ == 0) {
      surface_chooser_state_.is_expecting_relayout = false;
      update_state = true;
    }
  }

  surface_chooser_state_.initial_position = hint.screen_rect;
  bool promotable = promotion_hint_aggregator_->IsSafeToPromote();
  if (promotable != surface_chooser_state_.is_compositor_promotable) {
    surface_chooser_state_.is_compositor_promotable = promotable;
    update_state = true;
  }

  // If we've been provided with enough new frames, then update the state even
  // if it hasn't changed.  This lets |surface_chooser_| retry for an overlay.
  // It's especially helpful for power-efficient overlays, since we don't know
  // when an overlay becomes power efficient.  It also helps retry any failure
  // that's not accompanied by a state change, such as if android destroys the
  // overlay asynchronously for a transient reason.
  //
  // If we're already using an overlay, then there's no need to do this.
  base::TimeTicks now = tick_clock_->NowTicks();
  if (!is_using_overlay &&
      now - most_recent_chooser_retry_ >= RetryChooserTimeout) {
    update_state = true;
  }

  if (update_state) {
    most_recent_chooser_retry_ = now;
    UpdateChooserState(std::optional<AndroidOverlayFactoryCB>());
  }
}

SurfaceChooserHelper::FrameInformation
SurfaceChooserHelper::ComputeFrameInformation(bool is_using_overlay) {
  if (!is_using_overlay) {
    // Not an overlay.
    return surface_chooser_state_.is_secure
               ? FrameInformation::NON_OVERLAY_L3
               : FrameInformation::NON_OVERLAY_INSECURE;
  }

  // Overlay.
  if (surface_chooser_state_.is_secure) {
    return surface_chooser_state_.is_required ? FrameInformation::OVERLAY_L1
                                              : FrameInformation::OVERLAY_L3;
  }

  return surface_chooser_state_.is_fullscreen
             ? FrameInformation::OVERLAY_INSECURE_PLAYER_ELEMENT_FULLSCREEN
             : FrameInformation::OVERLAY_INSECURE_NON_PLAYER_ELEMENT_FULLSCREEN;
}

}  // namespace media

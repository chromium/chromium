// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/android_video_surface_chooser_impl.h"

#include <memory>

#include "base/time/default_tick_clock.h"

namespace media {

// Minimum time that we require after a failed overlay attempt before we'll try
// again for an overlay.
constexpr base::TimeDelta MinimumDelayAfterFailedOverlay = base::Seconds(5);

AndroidVideoSurfaceChooserImpl::AndroidVideoSurfaceChooserImpl(
    bool allow_dynamic,
    const base::TickClock* tick_clock)
    : allow_dynamic_(allow_dynamic), tick_clock_(tick_clock) {
  // Use a DefaultTickClock if one wasn't provided.
  if (!tick_clock_)
    tick_clock_ = base::DefaultTickClock::GetInstance();
}

AndroidVideoSurfaceChooserImpl::~AndroidVideoSurfaceChooserImpl() {}

void AndroidVideoSurfaceChooserImpl::SetClientCallbacks(
    UseOverlayCB use_overlay_cb,
    UseTextureOwnerCB use_texture_owner_cb) {
  DCHECK(use_overlay_cb && use_texture_owner_cb);
  use_overlay_cb_ = std::move(use_overlay_cb);
  use_texture_owner_cb_ = std::move(use_texture_owner_cb);
}

void AndroidVideoSurfaceChooserImpl::UpdateState(
    std::optional<AndroidOverlayFactoryCB> new_factory,
    const State& new_state) {
  DCHECK(use_overlay_cb_);
  bool entered_fullscreen =
      !current_state_.is_fullscreen && new_state.is_fullscreen;
  current_state_ = new_state;

  bool factory_changed = new_factory.has_value();
  if (factory_changed)
    overlay_factory_ = std::move(*new_factory);

  if (!allow_dynamic_) {
    if (!initial_state_received_) {
      initial_state_received_ = true;
      // Choose here so that Choose() doesn't have to handle non-dynamic.
      // Note that we ignore |is_expecting_relayout| here, since it's transient.
      // We don't want to pick TextureOwner permanently for that.
      if (overlay_factory_ &&
          ((current_state_.is_fullscreen &&
            !current_state_.promote_secure_only) ||
           current_state_.is_secure || current_state_.is_required) &&
          current_state_.video_rotation == VIDEO_ROTATION_0) {
        SwitchToOverlay(false);
      } else {
        SwitchToTextureOwner();
      }
    }
    return;
  }

  // If we're entering fullscreen, clear any previous failure attempt.  It's
  // likely that any previous failure was due to a lack of power efficiency,
  // but entering fs likely changes that anyway.
  if (entered_fullscreen)
    most_recent_overlay_failure_ = base::TimeTicks();

  // If the factory changed, we should cancel pending overlay requests and
  // set the client state back to Unknown if they're using an old overlay.
  if (factory_changed) {
    overlay_ = nullptr;
    if (client_overlay_state_ == kUsingOverlay)
      client_overlay_state_ = kUnknown;
  }

  Choose();
}

void AndroidVideoSurfaceChooserImpl::Choose() {
  // Pre-M we shouldn't be called.
  DCHECK(allow_dynamic_);

  // TODO(liberato): should this depend on resolution?
  OverlayState new_overlay_state =
      current_state_.promote_secure_only ? kUsingTextureOwner : kUsingOverlay;

  // Do we require a power-efficient overlay?
  bool needs_power_efficient = true;

  // Try to use an overlay if possible for protected content.  If the compositor
  // won't promote, though, it's okay if we switch out.  Set |is_required| in
  // addition, if you don't want this behavior.
  if (current_state_.is_secure) {
    new_overlay_state = kUsingOverlay;
    // Don't un-promote if not power efficient.  If we did, then inline playback
    // would likely not promote.
    needs_power_efficient = false;
  }

  // If the compositor won't promote, then don't.
  if (!current_state_.is_compositor_promotable)
    new_overlay_state = kUsingTextureOwner;

  // If we're PIP'd, then don't use an overlay unless it is required.  It isn't
  // positioned exactly right in some cases (crbug.com/917984).
  if (current_state_.is_persistent_video)
    new_overlay_state = kUsingTextureOwner;

  // If we're expecting a relayout, then don't transition to overlay if we're
  // not already in one.  We don't want to transition out, though.  This lets us
  // delay entering on a fullscreen transition until blink relayout is complete.
  // TODO(liberato): Detect this more directly.
  if (current_state_.is_expecting_relayout &&
      client_overlay_state_ != kUsingOverlay)
    new_overlay_state = kUsingTextureOwner;

  // If we're requesting an overlay, check that we haven't asked too recently
  // since the last failure.  This includes L1.  We don't bother to check for
  // our current state, since using an overlay would imply that our most recent
  // failure was long ago enough to pass this check earlier.
  if (new_overlay_state == kUsingOverlay) {
    base::TimeDelta time_since_last_failure =
        tick_clock_->NowTicks() - most_recent_overlay_failure_;
    if (time_since_last_failure < MinimumDelayAfterFailedOverlay)
      new_overlay_state = kUsingTextureOwner;
  }

  // If an overlay is required, then choose one.  The only way we won't is if we
  // don't have a factory or our request fails, or if it's rotated.
  if (current_state_.is_required) {
    new_overlay_state = kUsingOverlay;
    // Required overlays don't need to be power efficient.
    needs_power_efficient = false;
  }

  // Specifying a rotated overlay can NOTREACHED() in the compositor, so it's
  // better to fail.
  if (current_state_.video_rotation != VIDEO_ROTATION_0)
    new_overlay_state = kUsingTextureOwner;

  // If we have no factory, then we definitely don't want to use overlays.
  if (!overlay_factory_)
    new_overlay_state = kUsingTextureOwner;

  if (current_state_.always_use_texture_owner)
    new_overlay_state = kUsingTextureOwner;

  // Make sure that we're in |new_overlay_state_|.
  if (new_overlay_state == kUsingTextureOwner)
    SwitchToTextureOwner();
  else
    SwitchToOverlay(needs_power_efficient);
}

void AndroidVideoSurfaceChooserImpl::SwitchToTextureOwner() {
  // Invalidate any outstanding deletion callbacks for any overlays that we've
  // provided to the client already.  We assume that it will eventually drop
  // them in response to the callback.  Ready / failed callbacks aren't affected
  // by this, since we own the overlay until those occur.  We're about to
  // drop |overlay_|, if we have one, which cancels them.
  weak_factory_.InvalidateWeakPtrs();

  // Cancel any outstanding overlay request, in case we're switching to overlay.
  if (overlay_)
    overlay_ = nullptr;

  // Notify the client to switch if it's in the wrong state.
  if (client_overlay_state_ != kUsingTextureOwner) {
    DCHECK(use_texture_owner_cb_);

    client_overlay_state_ = kUsingTextureOwner;
    use_texture_owner_cb_.Run();
  }
}

void AndroidVideoSurfaceChooserImpl::SwitchToOverlay(
    bool needs_power_efficient) {
  // If there's already an overlay request outstanding, then do nothing.  We'll
  // finish switching when it completes.
  // TODO(liberato): If the power efficient flag for |overlay_| doesn't match
  // |needs_power_efficient|, then we should cancel it anyway.  In practice,
  // this doesn't happen, so we ignore it.
  if (overlay_)
    return;

  // Do nothing if the client is already using an overlay.  Note that if one
  // changes overlay factories, then this might not be true; an overlay from the
  // old factory is not the same as an overlay from the new factory.  However,
  // we assume that ReplaceOverlayFactory handles that.
  if (client_overlay_state_ == kUsingOverlay)
    return;

  // We don't modify |client_overlay_state_| yet, since we don't call the client
  // back yet.

  // Invalidate any outstanding callbacks.  This is needed for the deletion
  // callback, since for ready/failed callbacks, we still have ownership of the
  // object.  If we delete the object, then callbacks are cancelled anyway.
  // We also don't want to receive the power efficient callback.
  weak_factory_.InvalidateWeakPtrs();

  AndroidOverlayConfig config;
  // We bind all of our callbacks with weak ptrs, since we don't know how long
  // the client will hold on to overlays.  They could, in principle, show up
  // long after the client is destroyed too, if codec destruction hangs.
  config.ready_cb =
      base::BindOnce(&AndroidVideoSurfaceChooserImpl::OnOverlayReady,
                     weak_factory_.GetWeakPtr());
  config.failed_cb =
      base::BindOnce(&AndroidVideoSurfaceChooserImpl::OnOverlayFailed,
                     weak_factory_.GetWeakPtr());
  config.rect = current_state_.initial_position;
  config.secure = current_state_.is_secure;

  // Request power efficient overlays and callbacks if we're supposed to.
  config.power_efficient = needs_power_efficient;
  config.power_cb = base::BindRepeating(
      &AndroidVideoSurfaceChooserImpl::OnPowerEfficientState,
      weak_factory_.GetWeakPtr());

  overlay_ = overlay_factory_.Run(std::move(config));
  if (!overlay_)
    SwitchToTextureOwner();
}

void AndroidVideoSurfaceChooserImpl::OnOverlayReady(AndroidOverlay* overlay) {
  // |overlay_| is the only overlay for which we haven't gotten a ready callback
  // back yet.
  DCHECK_EQ(overlay, overlay_.get());

  // Notify the overlay that we'd like to know if it's destroyed, so that we can
  // update our internal state if the client drops it without being told.
  overlay_->AddOverlayDeletedCallback(
      base::BindOnce(&AndroidVideoSurfaceChooserImpl::OnOverlayDeleted,
                     weak_factory_.GetWeakPtr()));

  client_overlay_state_ = kUsingOverlay;
  use_overlay_cb_.Run(std::move(overlay_));
}

void AndroidVideoSurfaceChooserImpl::OnOverlayFailed(AndroidOverlay* overlay) {
  // We shouldn't get a failure for any overlay except the incoming one.
  DCHECK_EQ(overlay, overlay_.get());

  overlay_ = nullptr;
  most_recent_overlay_failure_ = tick_clock_->NowTicks();

  // If the client isn't already using a TextureOwner, then switch to it.
  // Note that this covers the case of kUnknown, when we might not have told the
  // client anything yet.  That's important for Initialize, so that a failed
  // overlay request still results in some callback to the client to know what
  // surface to start with.
  SwitchToTextureOwner();
}

void AndroidVideoSurfaceChooserImpl::OnOverlayDeleted(AndroidOverlay* overlay) {
  client_overlay_state_ = kUsingTextureOwner;
  // We don't call SwitchToTextureOwner since the client dropped the overlay.
  // It's already using TextureOwner.
}

void AndroidVideoSurfaceChooserImpl::OnPowerEfficientState(
    AndroidOverlay* overlay,
    bool is_power_efficient) {
  // We cannot receive this before OnSurfaceReady, since that is the first
  // callback if it arrives.  Getting a new overlay clears any previous cbs.
  DCHECK(!overlay_);

  // We cannot receive it after switching to TextureOwner, since that also
  // clears all callbacks.
  DCHECK(client_overlay_state_ == kUsingOverlay);

  // If the overlay has become power efficient, then take no action.
  if (is_power_efficient)
    return;

  // If the overlay is now required, then keep it.  It might have become
  // required since we requested it.
  if (current_state_.is_required)
    return;

  // If we're not able to switch dynamically, then keep the overlay.
  if (!allow_dynamic_)
    return;

  // We could set the failure timer here, but we don't mostly for fullscreen.
  // We don't want to delay transitioning to an overlay if the user re-enters
  // fullscreen.  TODO(liberato): Perhaps we should just clear the failure timer
  // if we detect a transition into fs when we get new state from the client.
  SwitchToTextureOwner();
}

}  // namespace media

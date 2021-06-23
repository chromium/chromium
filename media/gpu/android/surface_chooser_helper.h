// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SURFACE_CHOOSER_HELPER_H_
#define MEDIA_GPU_ANDROID_SURFACE_CHOOSER_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/video_transformation.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class TickClock;
}

namespace media {

// Helper class to manage state transitions for SurfaceChooser::State.  It's
// complicated and standalone enough not to be part of SurfaceChooser itself.
class MEDIA_GPU_EXPORT SurfaceChooserHelper {
 public:
  // |promotion_hint_aggregator| and |tick_clock| are for tests.  Normally, we
  // create the correct default implementations ourself.
  // |is_overlay_required| tells us to require overlays(!).
  // |promote_aggressively| causes us to use overlays whenever they're power-
  // efficient, which lets us catch fullscreen-div cases.
  // |always_use_texture_owner| forces us to always use a texture owner,
  // completely ignoring all other conditions.
  SurfaceChooserHelper(
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      bool is_overlay_required,
      bool promote_aggressively,
      bool always_use_texture_owner,
      std::unique_ptr<PromotionHintAggregator> promotion_hint_aggregator =
          nullptr,
      const base::TickClock* tick_clock = nullptr);
  ~SurfaceChooserHelper();

  enum class SecureSurfaceMode {
    // The surface should not be secure.  This allows both overlays and
    // TextureOwner surfaces.
    kInsecure,

    // It is preferable to have a secure surface, but insecure
    // (TextureOwner) is better than failing.
    kRequested,

    // The surface must be a secure surface, and should fail otherwise.
    kRequired,
  };

  // Must match AVDAFrameInformation UMA enum.  Please do not remove or re-order
  // values, only append new ones.
  enum class FrameInformation {
    NON_OVERLAY_INSECURE = 0,
    NON_OVERLAY_L3 = 1,
    OVERLAY_L3 = 2,
    OVERLAY_L1 = 3,
    OVERLAY_INSECURE_PLAYER_ELEMENT_FULLSCREEN = 4,
    OVERLAY_INSECURE_NON_PLAYER_ELEMENT_FULLSCREEN = 5,

    // Max enum value.
    FRAME_INFORMATION_MAX = OVERLAY_INSECURE_NON_PLAYER_ELEMENT_FULLSCREEN
  };

  // The setters do not update the chooser state, since pre-M requires us to be
  // careful about the first update, since we can't change it later.

  // Notify us about the desired surface security.  Does not update the chooser
  // state.
  void SetSecureSurfaceMode(SecureSurfaceMode mode);

  // Notify us about the fullscreen state.  Does not update the chooser state.
  void SetIsFullscreen(bool is_fullscreen);

  // Notify us about the default rotation for the video.
  void SetVideoRotation(VideoRotation video_rotation);

  // Notify us about PIP state.
  void SetIsPersistentVideo(bool is_persistent_video);

  // Update the chooser state using the given factory.
  void UpdateChooserState(base::Optional<AndroidOverlayFactoryCB> new_factory);

  // Notify us about a promotion hint.  This will update the chooser state
  // if needed.
  void NotifyPromotionHintAndUpdateChooser(
      const PromotionHintAggregator::Hint& hint,
      bool is_using_overlay);

  AndroidVideoSurfaceChooser* chooser() const { return surface_chooser_.get(); }

  // Return the FrameInformation bucket number that the config reflects, given
  // that |is_using_overlay| reflects whether we're currently using an overlay
  // or not.
  FrameInformation ComputeFrameInformation(bool is_using_overlay);

 private:
  AndroidVideoSurfaceChooser::State surface_chooser_state_;
  std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser_;

  // Are overlays required by command-line options?
  bool is_overlay_required_ = false;

  // Do we require an overlay due to the surface mode?
  bool requires_secure_video_surface_ = false;

  std::unique_ptr<PromotionHintAggregator> promotion_hint_aggregator_;

  // Time since we last updated the chooser state.
  base::TimeTicks most_recent_chooser_retry_;

  const base::TickClock* tick_clock_;

  // Number of promotion hints that we need to receive before clearing the
  // "delay overlay promotion" flag in |surface_chooser_state_|.  We do this so
  // that the transition looks better, since it gives blink time to stabilize.
  // Since overlay positioning isn't synchronous, it's good to make sure that
  // blink isn't moving the quad around too.
  int hints_until_clear_relayout_flag_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SurfaceChooserHelper);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SURFACE_CHOOSER_HELPER_H_

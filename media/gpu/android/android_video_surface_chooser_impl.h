// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_
#define MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Implementation of AndroidVideoSurfaceChooser.
class MEDIA_GPU_EXPORT AndroidVideoSurfaceChooserImpl
    : public AndroidVideoSurfaceChooser {
 public:
  // |allow_dynamic| should be true if and only if we are allowed to change the
  // surface selection after the initial callback.  |tick_clock|, if provided,
  // will be used as our time source.  Otherwise, we'll use wall clock.  If
  // provided, then it must outlast |this|.
  AndroidVideoSurfaceChooserImpl(bool allow_dynamic,
                                 const base::TickClock* tick_clock = nullptr);

  AndroidVideoSurfaceChooserImpl(const AndroidVideoSurfaceChooserImpl&) =
      delete;
  AndroidVideoSurfaceChooserImpl& operator=(
      const AndroidVideoSurfaceChooserImpl&) = delete;

  ~AndroidVideoSurfaceChooserImpl() override;

  // AndroidVideoSurfaceChooser
  void SetClientCallbacks(UseOverlayCB use_overlay_cb,
                          UseTextureOwnerCB use_texture_owner_cb) override;
  void UpdateState(std::optional<AndroidOverlayFactoryCB> new_factory,
                   const State& new_state) override;

 private:
  // Choose whether we should be using a TextureOwner or overlay, and issue
  // the right callbacks if we're changing between them.  This should only be
  // called if |allow_dynamic_|.
  void Choose();

  // Start switching to TextureOwner or overlay, as needed.  These will call
  // the client callbacks if we're changing state, though those callbacks might
  // happen after this returns.
  void SwitchToTextureOwner();
  // If |overlay_| has an in-flight request, then this will do nothing.  If
  // |power_efficient|, then we will require a power-efficient overlay, and
  // cancel it if it becomes not power efficient.
  void SwitchToOverlay(bool power_efficient);

  // AndroidOverlay callbacks.
  void OnOverlayReady(AndroidOverlay*);
  void OnOverlayFailed(AndroidOverlay*);
  void OnOverlayDeleted(AndroidOverlay*);
  void OnPowerEfficientState(AndroidOverlay* overlay, bool is_power_efficient);

  // Client callbacks.
  UseOverlayCB use_overlay_cb_;
  UseTextureOwnerCB use_texture_owner_cb_;

  // Current overlay that we've constructed but haven't received ready / failed
  // callbacks yet.  Will be nullptr if we haven't constructed one, or if we
  // sent it to the client already once it became ready to use.
  std::unique_ptr<AndroidOverlay> overlay_;

  AndroidOverlayFactoryCB overlay_factory_;

  // Do we allow dynamic surface switches.  Usually this means "Are we running
  // on M or later?".
  bool allow_dynamic_;

  enum OverlayState {
    kUnknown,
    kUsingTextureOwner,
    kUsingOverlay,
  };

  // What was the last signal that the client received?
  OverlayState client_overlay_state_ = kUnknown;

  State current_state_;

  bool initial_state_received_ = false;

  // Not owned by us.
  raw_ptr<const base::TickClock> tick_clock_;

  // Time at which we most recently got a failed overlay request.
  base::TimeTicks most_recent_overlay_failure_;

  base::WeakPtrFactory<AndroidVideoSurfaceChooserImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_H_
#define MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/android_overlay.h"
#include "media/base/video_transformation.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Manage details of which surface to use for video playback.
class MEDIA_GPU_EXPORT AndroidVideoSurfaceChooser {
 public:
  // Input state used for choosing the surface type.
  struct State {
    State();
    ~State();

    // Is an overlay required?
    bool is_required = false;

    // Is the player currently in fullscreen?
    bool is_fullscreen = false;

    // Should the overlay be marked as secure?
    bool is_secure = false;

    // Is the player's frame hidden / closed?
    bool is_frame_hidden = false;

    // Is the compositor willing to promote this?
    bool is_compositor_promotable = false;

    // Are we expecting a relayout soon?
    bool is_expecting_relayout = false;

    // If true, then we will promote to overlay only if it's required for secure
    // video playback.
    bool promote_secure_only = false;

    // Default orientation for the video.
    VideoRotation video_rotation = VIDEO_ROTATION_0;

    // Hint to use for the initial position when transitioning to an overlay.
    gfx::Rect initial_position;

    // Indicates that we should always use a TextureOwner. This is used with
    // SurfaceControl where the TextureOwner can be promoted to an overlay
    // dynamically by the compositor.
    bool always_use_texture_owner = false;

    // Is the video persistent (PIP)?
    bool is_persistent_video = false;
  };

  // Notify the client that |overlay| is ready for use.  The client may get
  // the surface immediately.
  using UseOverlayCB =
      base::RepeatingCallback<void(std::unique_ptr<AndroidOverlay> overlay)>;

  // Notify the client that the most recently provided overlay should be
  // discarded.  The overlay is still valid, but we recommend against
  // using it soon, in favor of a TextureOwner.
  using UseTextureOwnerCB = base::RepeatingCallback<void(void)>;

  AndroidVideoSurfaceChooser() {}

  AndroidVideoSurfaceChooser(const AndroidVideoSurfaceChooser&) = delete;
  AndroidVideoSurfaceChooser& operator=(const AndroidVideoSurfaceChooser&) =
      delete;

  virtual ~AndroidVideoSurfaceChooser() {}

  // Sets the client callbacks to be called when a new surface choice is made.
  // Must be called before UpdateState();
  virtual void SetClientCallbacks(UseOverlayCB use_overlay_cb,
                                  UseTextureOwnerCB use_texture_owner_cb) = 0;

  // Updates the current state and makes a new surface choice with the new
  // state. If |new_factory| is empty, the factory is left as-is. Otherwise,
  // the factory is updated to |*new_factory|.
  virtual void UpdateState(std::optional<AndroidOverlayFactoryCB> new_factory,
                           const State& new_state) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_ANDROID_VIDEO_SURFACE_CHOOSER_H_

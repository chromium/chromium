// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OVERLAY_WINDOW_H_
#define CONTENT_PUBLIC_BROWSER_OVERLAY_WINDOW_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class Layer;
}

namespace viz {
class SurfaceId;
}

namespace content {

class PictureInPictureWindowController;

// This window will always float above other windows. The intention is to show
// content perpetually while the user is still interacting with the other
// browser windows.
class OverlayWindow {
 public:
  enum PlaybackState {
    kPlaying = 0,
    kPaused,
    kEndOfVideo,
  };

  OverlayWindow() = default;
  virtual ~OverlayWindow() = default;

  // Returns a created OverlayWindow. This is defined in the platform-specific
  // implementation for the class.
  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller);

  virtual bool IsActive() = 0;
  virtual void Close() = 0;
  virtual void ShowInactive() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() = 0;
  virtual bool IsAlwaysOnTop() = 0;
  // Retrieves the window's current bounds, including its window.
  virtual gfx::Rect GetBounds() = 0;
  virtual void UpdateVideoSize(const gfx::Size& natural_size) = 0;
  virtual void SetPlaybackState(PlaybackState playback_state) = 0;
  virtual void SetAlwaysHidePlayPauseButton(bool is_visible) = 0;
  virtual void SetSkipAdButtonVisibility(bool is_visible) = 0;
  virtual void SetNextTrackButtonVisibility(bool is_visible) = 0;
  virtual void SetPreviousTrackButtonVisibility(bool is_visible) = 0;
  virtual void SetSurfaceId(const viz::SurfaceId& surface_id) = 0;
  virtual cc::Layer* GetLayerForTesting() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayWindow);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERLAY_WINDOW_H_

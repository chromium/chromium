// Copyright 2017 The Chromium Authors
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

namespace viz {
class SurfaceId;
}

namespace content {

class VideoPictureInPictureWindowController;

// This window will always float above other windows. The intention is to show
// content perpetually while the user is still interacting with the other
// browser windows.
class VideoOverlayWindow {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE:(
  //   org.chromium.content_public.browser.overlay_window)
  enum PlaybackState {
    kPlaying = 0,
    kPaused,
    kEndOfVideo,
  };

  VideoOverlayWindow() = default;

  VideoOverlayWindow(const VideoOverlayWindow&) = delete;
  VideoOverlayWindow& operator=(const VideoOverlayWindow&) = delete;

  // Returns a created VideoOverlayWindow. This is defined in the
  // platform-specific implementation for the class.
  static std::unique_ptr<VideoOverlayWindow> Create(
      VideoPictureInPictureWindowController* controller);

  virtual ~VideoOverlayWindow() = default;

  virtual bool IsActive() const = 0;
  virtual void Close() = 0;
  virtual void ShowInactive() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() const = 0;
  // Retrieves the window's current bounds, including its window.
  virtual gfx::Rect GetBounds() = 0;
  virtual void UpdateNaturalSize(const gfx::Size& natural_size) = 0;
  virtual void SetPlaybackState(PlaybackState playback_state) = 0;
  virtual void SetPlayPauseButtonVisibility(bool is_visible) = 0;
  virtual void SetSkipAdButtonVisibility(bool is_visible) = 0;
  virtual void SetNextTrackButtonVisibility(bool is_visible) = 0;
  virtual void SetPreviousTrackButtonVisibility(bool is_visible) = 0;
  virtual void SetMicrophoneMuted(bool muted) = 0;
  virtual void SetCameraState(bool turned_on) = 0;
  virtual void SetToggleMicrophoneButtonVisibility(bool is_visible) = 0;
  virtual void SetToggleCameraButtonVisibility(bool is_visible) = 0;
  virtual void SetHangUpButtonVisibility(bool is_visible) = 0;
  virtual void SetNextSlideButtonVisibility(bool is_visible) = 0;
  virtual void SetPreviousSlideButtonVisibility(bool is_visible) = 0;

  virtual void SetSurfaceId(const viz::SurfaceId& surface_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERLAY_WINDOW_H_

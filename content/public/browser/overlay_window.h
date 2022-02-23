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

class DocumentPictureInPictureWindowController;
class VideoPictureInPictureWindowController;

// This window will always float above other windows. The intention is to show
// content perpetually while the user is still interacting with the other
// browser windows.
class OverlayWindow {
 public:
  OverlayWindow() = default;

  OverlayWindow(const OverlayWindow&) = delete;
  OverlayWindow& operator=(const OverlayWindow&) = delete;

  virtual ~OverlayWindow() = default;

  virtual bool IsActive() = 0;
  virtual void Close() = 0;
  virtual void ShowInactive() = 0;
  virtual void Hide() = 0;
  virtual bool IsVisible() = 0;
  virtual bool IsAlwaysOnTop() = 0;
  // Retrieves the window's current bounds, including its window.
  virtual gfx::Rect GetBounds() = 0;
  // Updates the content (video or document) size.
  virtual void UpdateNaturalSize(const gfx::Size& natural_size) = 0;
};

class VideoOverlayWindow : public OverlayWindow {
 public:
  enum PlaybackState {
    kPlaying = 0,
    kPaused,
    kEndOfVideo,
  };

  VideoOverlayWindow() = default;

  // Returns a created VideoOverlayWindow. This is defined in the
  // platform-specific implementation for the class.
  static std::unique_ptr<VideoOverlayWindow> Create(
      VideoPictureInPictureWindowController* controller);

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
  virtual void SetSurfaceId(const viz::SurfaceId& surface_id) = 0;
  virtual cc::Layer* GetLayerForTesting() = 0;
};

class DocumentOverlayWindow : public OverlayWindow {
 public:
  DocumentOverlayWindow() = default;

  // Returns a created DocumentOverlayWindow. This is defined in the
  // platform-specific implementation for the class.
  static std::unique_ptr<DocumentOverlayWindow> Create(
      DocumentPictureInPictureWindowController* controller);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERLAY_WINDOW_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CAPTURE_STREAM_H_
#define REMOTING_HOST_LINUX_CAPTURE_STREAM_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/portal/pipewire_utils.h"

namespace remoting {

// Wraps a capture stream representing a logical monitor.
class CaptureStream {
 public:
  class CursorObserver : public base::CheckedObserver {
   public:
    using Subscription = base::ScopedClosureRunner;

    virtual void OnCursorShapeChanged(CaptureStream* stream) {}
    virtual void OnCursorPositionChanged(CaptureStream* stream) {}
  };

  virtual ~CaptureStream() = default;

  // Specifies the |pipewire_node| from which to capture and the
  // |initial_resolution| to negotiate. The node should be configured to provide
  // the mouse cursor as metadata.
  //
  // |mapping_id| is an opaque mapping ID that may be provided by the
  // higher-level remote desktop API to facilitate matching the monitor to its
  // corresponding input region. It is stored and made accessible via the
  // mapping_id() method for convenience, but is otherwise unused and may be an
  // empty string.
  //
  // |pipewire_fd| is used to communicate with the target PipeWire instance.
  virtual void SetPipeWireStream(std::uint32_t pipewire_node,
                                 const webrtc::DesktopSize& initial_resolution,
                                 std::string_view mapping_id,
                                 int pipewire_fd) = 0;

  // Similar to SetPipeWireStream(), but connects to the default PipeWire
  // instance.
  void SetPipeWireStream(std::uint32_t pipewire_node,
                         const webrtc::DesktopSize& initial_resolution,
                         std::string_view mapping_id) {
    SetPipeWireStream(pipewire_node, initial_resolution, mapping_id,
                      webrtc::kInvalidPipeWireFd);
  }

  // Starts capturing the video stream, which creates the virtual monitor. The
  // virtual monitor is not created until this method is called. This method can
  // be called before SetCallback(). See documentation for SetCallback().
  virtual void StartVideoCapture() = 0;

  // Sets a callback to be invoked on the current sequence as each new frame is
  // received. If StartVideoCapture() has been called, the callback will be
  // immediately called on the current stack frame with the last available
  // frame.
  // Passing `nullptr` will stop the previously set callback from being called.
  virtual void SetCallback(
      base::WeakPtr<webrtc::DesktopCapturer::Callback> callback) = 0;

  // Sets whether damage region should be used. Mutter may return invalid damage
  // regions in some cases, where disabling damage region makes sense.
  // Damage region is enabled by default.
  // See: https://gitlab.gnome.org/GNOME/mutter/-/issues/4269
  virtual void SetUseDamageRegion(bool use_damage_region) = 0;

  // Negotiates a new video resolution with PipeWire. If capturing from a
  // virtual monitor, it will be resized to match.
  virtual void SetResolution(const webrtc::DesktopSize& new_resolution) = 0;

  // Sets the maximum rate at which new frames should be delivered.
  virtual void SetMaxFrameRate(std::uint32_t frame_rate) = 0;

  // Gets the most recent mouse cursor shape, if one has been received since the
  // last call. Otherwise, returns nullptr. (May only return a value once each
  // time the cursor actually changes.)
  virtual std::unique_ptr<webrtc::MouseCursor> CaptureCursor() = 0;

  // Returns a copy of the most recent mouse cursor location received from
  // PipeWire, if any.
  virtual std::optional<webrtc::DesktopVector> CaptureCursorPosition() = 0;

  // Adds a cursor observer. Discarding the returned subscription will result in
  // the removal of the observer.
  [[nodiscard]] virtual CursorObserver::Subscription AddCursorObserver(
      CursorObserver* observer) = 0;

  // Retrieves the mapping ID previously stored by set_mapping_id().
  virtual std::string_view mapping_id() const = 0;

  virtual const webrtc::DesktopSize& resolution() const = 0;

  virtual void set_screen_id(webrtc::ScreenId screen_id) = 0;

  virtual webrtc::ScreenId screen_id() const = 0;

  // Obtains a weak pointer to this.
  virtual base::WeakPtr<CaptureStream> GetWeakPtr() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CAPTURE_STREAM_H_

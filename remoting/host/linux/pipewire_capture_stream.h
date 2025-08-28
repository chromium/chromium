// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/portal/pipewire_utils.h"

namespace remoting {

// Wraps a PipeWire capture stream representing a logical monitor, such as may
// be provided by the GNOME, Portal, and similar remote desktop APIs.
class PipewireCaptureStream {
 public:
  PipewireCaptureStream();
  PipewireCaptureStream(const PipewireCaptureStream&) = delete;
  PipewireCaptureStream& operator=(const PipewireCaptureStream&) = delete;
  ~PipewireCaptureStream();

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
  // If specified, |pipewire_fd| is used to communicate with the target PipeWire
  // instance. Otherwise, connects to the default PipeWire instance.
  void SetPipeWireStream(std::uint32_t pipewire_node,
                         const webrtc::DesktopSize& initial_resolution,
                         std::string mapping_id,
                         int pipewire_fd = webrtc::kInvalidPipeWireFd);

  // Starts capturing the video stream, which creates the virtual monitor. The
  // virtual monitor is not created until this method is called. This method can
  // be called before SetCallback(). See documentation for SetCallback().
  void StartVideoCapture();

  // Sets a callback to be invoked on the current sequence as each new frame is
  // received. If StartVideoCapture() has been called, the callback will be
  // immediately called on the current stack frame with the last available
  // frame.
  // Passing `nullptr` will stop the previously set callback from being called.
  void SetCallback(base::WeakPtr<webrtc::DesktopCapturer::Callback> callback);

  // Sets whether damage region should be used. Mutter may return invalid damage
  // regions in some cases, where disabling damage region makes sense.
  // Damage region is enabled by default.
  // See: https://gitlab.gnome.org/GNOME/mutter/-/issues/4269
  void SetUseDamageRegion(bool use_damage_region);

  // Negotiates a new video resolution with PipeWire. If capturing from a
  // virtual monitor, it will be resized to match.
  void SetResolution(const webrtc::DesktopSize& new_resolution);

  // Sets the maximum rate at which new frames should be delivered.
  void SetMaxFrameRate(std::uint32_t frame_rate);

  // Gets the most recent mouse cursor shape, if one has been received since the
  // last call. Otherwise, returns nullptr. (May only return a value once each
  // time the cursor actually changes.)
  std::unique_ptr<webrtc::MouseCursor> CaptureCursor();

  // Returns a copy of the most recent mouse cursor location received from
  // PipeWire, if any.
  std::optional<webrtc::DesktopVector> CaptureCursorPosition();

  // Retrieves the mapping ID previously stored by set_mapping_id().
  std::string_view mapping_id();

  const webrtc::DesktopSize& resolution() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return resolution_;
  }

  void set_screen_id(webrtc::ScreenId screen_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    screen_id_ = screen_id;
  }

  webrtc::ScreenId screen_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return screen_id_;
  }

  // Obtains a weak pointer to this.
  base::WeakPtr<PipewireCaptureStream> GetWeakPtr();

 private:
  class CallbackProxy;
  // Recaptures (i.e. invokes `callback_` again with) the latest available frame
  // in the current call stack, with the entire frame marked as dirty, if no
  // frame is currently being captured. If capturing is in progress, a flag will
  // be set to mark the next captured frame as dirty.
  // This is useful to supply the first frame when `callback_` is set, or when
  // the `use_damage_region` flag has changed.
  // Note: Do not access class members after this method is called, since `this`
  // may potentially be deleted at that point.
  void RecaptureLatestFrameAsDirty();

  // Called by the callback proxy.
  void OnFrameCaptureStart();
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

  int pipewire_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::uint32_t pipewire_node_ GUARDED_BY_CONTEXT(sequence_checker_);

  webrtc::DesktopSize resolution_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string mapping_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::ScreenId screen_id_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;
  base::WeakPtr<webrtc::DesktopCapturer::Callback> callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::scoped_refptr<webrtc::SharedScreenCastStream> stream_
      GUARDED_BY_CONTEXT(sequence_checker_) =
          webrtc::SharedScreenCastStream::CreateDefault();
  std::unique_ptr<CallbackProxy> callback_proxy_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_capturing_frame_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool should_mark_current_frame_dirty_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipewireCaptureStream> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_

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
#include "base/thread_annotations.h"
#include "remoting/host/base/screen_resolution.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
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
                         ScreenResolution initial_resolution,
                         std::string mapping_id,
                         int pipewire_fd = webrtc::kInvalidPipeWireFd);

  // Starts capturing the video stream. The methods on |callback| will be
  // invoked from the PipeWire thread as each new frame is received. (They will
  // not be invoked on the caller's sequence.)
  void StartVideoCapture(webrtc::DesktopCapturer::Callback* callback);

  // Negotiates a new video resolution with PipeWire. If capturing from a
  // virtual monitor, it will be resized to match.
  void SetResolution(ScreenResolution new_resolution);

  // Sets the maximum rate at which new frames should be delivered.
  void SetMaxFrameRate(std::uint32_t frame_rate);

  // Gets the most recent mouse cursor shape, if one has been received since the
  // last call. Otherwise, returns nullptr. (May only return a value once each
  // time the cursor actually changes.)
  std::unique_ptr<webrtc::MouseCursor> CaptureCursor();

  // Returns a copy of the most recent mouse cursor location received from
  // PipeWire, if any.
  std::optional<webrtc::DesktopVector> CaptureCursorPosition();

  // Disconnects from the pipewire stream. No more frame callbacks will be
  // invoked after this method returns.
  void StopVideoCapture();

  // Retrieves the mapping ID previously stored by set_mapping_id().
  std::string_view mapping_id();

  // Obtains a weak pointer to this.
  base::WeakPtr<PipewireCaptureStream> GetWeakPtr();

 private:
  int pipewire_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::uint32_t pipewire_node_ GUARDED_BY_CONTEXT(sequence_checker_);

  ScreenResolution resolution_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string mapping_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::scoped_refptr<webrtc::SharedScreenCastStream> stream_
      GUARDED_BY_CONTEXT(sequence_checker_) =
          webrtc::SharedScreenCastStream::CreateDefault();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipewireCaptureStream> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_

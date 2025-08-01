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
#include "base/thread_annotations.h"
#include "remoting/host/base/screen_resolution.h"
#include "third_party/webrtc/api/scoped_refptr.h"
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
                         ScreenResolution initial_resolution,
                         std::string mapping_id,
                         int pipewire_fd = webrtc::kInvalidPipeWireFd);

  // Starts capturing the video stream, which creates the virtual monitor. This
  // can be called before SetCallback(). See documentation for SetCallback().
  void StartVideoCapture();

  // Sets a callback to be invoked on `callback_sequence` as each new frame is
  // received. If StartVideoCapture() has been called, a task will be
  // immediately posted to `callback_sequence` to run the callback with the
  // last available frame. `callback` will no longer be called once
  // StopVideoCapture() is called.
  void SetCallback(scoped_refptr<base::SequencedTaskRunner> callback_sequence,
                   base::WeakPtr<webrtc::DesktopCapturer::Callback> callback);

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

  const ScreenResolution& resolution() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return resolution_;
  }

  // Obtains a weak pointer to this.
  base::WeakPtr<PipewireCaptureStream> GetWeakPtr();

 private:
  // SharedScreenCastStream runs the pipewire loop, and invokes frame callbacks,
  // on a separate thread. This class is responsible for bouncing them back to
  // `callback_sequence`.
  class CallbackProxy : public webrtc::DesktopCapturer::Callback {
   public:
    CallbackProxy();
    ~CallbackProxy() override;

    void Initialize(scoped_refptr<base::SequencedTaskRunner> callback_sequence,
                    base::WeakPtr<webrtc::DesktopCapturer::Callback> callback,
                    std::unique_ptr<webrtc::DesktopFrame> initial_frame);

    // Callback interface
    void OnFrameCaptureStart() override;
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override;

   private:
    // Lock is needed since Initialize() and the callback methods are called
    // from different threads. It also ensures that the initial frame is
    // delivered before any frames received from the SharedScreenCastStream.
    base::Lock lock_;
    scoped_refptr<base::SequencedTaskRunner> callback_sequence_
        GUARDED_BY(lock_);
    base::WeakPtr<webrtc::DesktopCapturer::Callback> callback_
        GUARDED_BY(lock_);
  };

  int pipewire_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::uint32_t pipewire_node_ GUARDED_BY_CONTEXT(sequence_checker_);

  ScreenResolution resolution_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string mapping_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::scoped_refptr<webrtc::SharedScreenCastStream> stream_
      GUARDED_BY_CONTEXT(sequence_checker_) =
          webrtc::SharedScreenCastStream::CreateDefault();
  CallbackProxy callback_proxy_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipewireCaptureStream> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_

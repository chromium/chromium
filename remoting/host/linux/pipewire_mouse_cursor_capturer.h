// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

// A class that allows incarnations of PipewireMouseCursorMonitor to capture
// mouse cursor shapes and positions, and get the latest cursor shape before
// it is created. The interface of this class pretty much mirrors
// webrtc::MouseCursorMonitor.
class PipewireMouseCursorCapturer {
 public:
  using Callback = webrtc::MouseCursorMonitor::Callback;
  using Mode = webrtc::MouseCursorMonitor::Mode;

  explicit PipewireMouseCursorCapturer(
      base::WeakPtr<PipewireCaptureStreamManager> stream_manager);
  ~PipewireMouseCursorCapturer();

  // Sets a callback and the monitor mode. Pass nullptr to prevent the
  // previously set callback from being called.
  void SetCallback(Callback* callback, Mode mode);

  // Attempts to capture the current mouse cursor and position and calls the
  // corresponding methods on the callback. OnMouseCursor() will be called iff
  // the cursor has changed, or, this is the first call of Capture() since
  // SetCallback() and the latest cursor is available. No-op if callback is
  // nullptr.
  void Capture();

  base::WeakPtr<PipewireMouseCursorCapturer> GetWeakPtr();

 private:
  raw_ptr<Callback> callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool report_position_ GUARDED_BY_CONTEXT(sequence_checker_);
  // If this is set to true, the Capture() call will supply the latest cursor
  // when stream->CaptureCursor() returns nullptr (i.e. cursor is unchanged).
  // This is set to true in SetCallback() and is set to false at the end of the
  // Capture() call.
  bool want_latest_cursor_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<webrtc::SharedDesktopFrame> latest_cursor_frame_
      GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::DesktopVector latest_cursor_hotspot_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<PipewireCaptureStreamManager> stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PipewireMouseCursorCapturer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_MOUSE_CURSOR_CAPTURER_H_

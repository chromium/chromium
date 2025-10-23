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
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/host/linux/capture_stream.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

// Wraps a PipeWire capture stream representing a logical monitor, such as may
// be provided by the GNOME, Portal, and similar remote desktop APIs.
class PipewireCaptureStream : public CaptureStream {
 public:
  // Inherit overloads from the base class.
  using CaptureStream::SetPipeWireStream;

  PipewireCaptureStream();
  PipewireCaptureStream(const PipewireCaptureStream&) = delete;
  PipewireCaptureStream& operator=(const PipewireCaptureStream&) = delete;
  ~PipewireCaptureStream() override;

  // CaptureStream implementation:
  void SetPipeWireStream(std::uint32_t pipewire_node,
                         const webrtc::DesktopSize& initial_resolution,
                         std::string_view mapping_id,
                         int pipewire_fd) override;
  void StartVideoCapture() override;
  void SetCallback(
      base::WeakPtr<webrtc::DesktopCapturer::Callback> callback) override;
  void SetUseDamageRegion(bool use_damage_region) override;
  void SetResolution(const webrtc::DesktopSize& new_resolution) override;
  void SetMaxFrameRate(std::uint32_t frame_rate) override;
  std::unique_ptr<webrtc::MouseCursor> CaptureCursor() override;
  std::optional<webrtc::DesktopVector> CaptureCursorPosition() override;
  CursorObserver::Subscription AddCursorObserver(
      CursorObserver* observer) override;
  std::string_view mapping_id() const override;

  const webrtc::DesktopSize& resolution() const override;

  void set_screen_id(webrtc::ScreenId screen_id) override;

  webrtc::ScreenId screen_id() const override;

  // Obtains a weak pointer to this.
  base::WeakPtr<CaptureStream> GetWeakPtr() override;

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
  void RemoveCursorObserver(CursorObserver* observer);

  // Called by the callback proxy.
  void OnFrameCaptureStart();
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);
  void OnCursorPositionChanged();
  void OnCursorShapeChanged();

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
  base::ObserverList<CaptureStream::CursorObserver> cursor_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_capturing_frame_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool should_mark_current_frame_dirty_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipewireCaptureStream> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_CAPTURE_STREAM_H_

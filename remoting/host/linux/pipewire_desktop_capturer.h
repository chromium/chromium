// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// DesktopCapturer implementation that allows capturing a single screen via the
// provided PipewireCaptureStream.
class PipewireDesktopCapturer : public DesktopCapturer,
                                public webrtc::DesktopCapturer::Callback {
 public:
  explicit PipewireDesktopCapturer(base::WeakPtr<PipewireCaptureStream> stream);
  PipewireDesktopCapturer(const PipewireDesktopCapturer&) = delete;
  PipewireDesktopCapturer& operator=(const PipewireDesktopCapturer&) = delete;
  ~PipewireDesktopCapturer() override;

  // DesktopCapturer interface.
  bool SupportsFrameCallbacks() override;
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  void SetMaxFrameRate(std::uint32_t max_frame_rate) override;

  // Unimplemented DesktopCapturer methods that should not be called. Rather,
  // the appropriate PipewireCaptureStream is provided to the constructor by the
  // DesktopInteractionStrategy based on the screen ID passed to
  // DesktopInteractionStrategy::CreateVideoCapturer().
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  scoped_refptr<base::SequencedTaskRunner> creating_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SequencedTaskRunner> capture_sequence_;

  // Must only be tested for validity and dereferenced on the creating sequence.
  base::WeakPtr<PipewireCaptureStream> stream_;

  // Per the webrtc::DesktopCapturer interface, callback is required to remain
  // valid until this is destroyed.
  raw_ptr<Callback> callback_;

  // Will be bound to the capture sequence when Start() is called and used by
  // tasks posted by CallbackProxy.
  base::WeakPtrFactory<PipewireDesktopCapturer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_

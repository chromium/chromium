// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_

#include <cstdint>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
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
  explicit PipewireDesktopCapturer(base::WeakPtr<CaptureStream> stream);
  PipewireDesktopCapturer(const PipewireDesktopCapturer&) = delete;
  PipewireDesktopCapturer& operator=(const PipewireDesktopCapturer&) = delete;
  ~PipewireDesktopCapturer() override;

  // DesktopCapturer interface.
  // These methods can be called on any sequence.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  void SetMaxFrameRate(std::uint32_t max_frame_rate) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;

  // Unimplemented DesktopCapturer methods that should not be called. Rather,
  // the appropriate PipewireCaptureStream is provided to the constructor by the
  // DesktopInteractionStrategy based on the screen ID passed to
  // DesktopInteractionStrategy::CreateVideoCapturer().
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  void Pause(bool pause) override;
  void BoostCaptureRate(base::TimeDelta capture_interval,
                        base::TimeDelta duration) override;

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  base::WeakPtr<CaptureStream> stream_;

  // Per the webrtc::DesktopCapturer interface, callback is required to remain
  // valid until this is destroyed.
  raw_ptr<Callback> callback_ = nullptr;

  uint32_t last_frame_rate_ GUARDED_BY_CONTEXT(sequence_checker_) = 60;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipewireDesktopCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_

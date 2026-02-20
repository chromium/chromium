// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DESKTOP_CAPTURER_WRAPPER_H_
#define REMOTING_PROTOCOL_DESKTOP_CAPTURER_WRAPPER_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if defined(WEBRTC_USE_GIO)
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#endif

namespace remoting {

// Wrapper class that adapts a webrtc::DesktopCapturer into a
// remoting::DesktopCapturer. It calls CaptureFrame() on the underlying capturer
// at the target frame rate.
class DesktopCapturerWrapper : public DesktopCapturer,
                               public webrtc::DesktopCapturer::Callback {
 public:
  explicit DesktopCapturerWrapper(
      std::unique_ptr<webrtc::DesktopCapturer> capturer);
  DesktopCapturerWrapper(const DesktopCapturerWrapper&) = delete;
  DesktopCapturerWrapper& operator=(const DesktopCapturerWrapper&) = delete;
  ~DesktopCapturerWrapper() override;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  void SetMaxFrameRate(std::uint32_t max_frame_rate) override;
  void Pause(bool pause) override;
  void BoostCaptureRate(base::TimeDelta capture_interval,
                        base::TimeDelta duration) override;

#if defined(WEBRTC_USE_GIO)
  void GetMetadataAsync(base::OnceCallback<void(webrtc::DesktopCaptureMetadata)>
                            callback) override;
#endif

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  void CaptureFrameInternal();

  raw_ptr<webrtc::DesktopCapturer::Callback> callback_ = nullptr;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  std::unique_ptr<protocol::WebrtcFrameSchedulerConstantRate> scheduler_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DESKTOP_CAPTURER_WRAPPER_H_

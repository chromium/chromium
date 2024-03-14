// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_WRAPPER_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_WRAPPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/desktop_capturer.h"

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

// Simple wrapper class which holds a webrtc::DesktopCapturer and exposes a
// remoting::DesktopCapturer interface for interacting with it.
class DesktopCapturerWrapper : public DesktopCapturer,
                               public webrtc::DesktopCapturer::Callback {
 public:
  DesktopCapturerWrapper();
  DesktopCapturerWrapper(const DesktopCapturerWrapper&) = delete;
  DesktopCapturerWrapper& operator=(const DesktopCapturerWrapper&) = delete;
  ~DesktopCapturerWrapper() override;

  void CreateCapturer(const webrtc::DesktopCaptureOptions& options,
                      SourceId id);

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool SupportsFrameCallbacks() override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
#if defined(WEBRTC_USE_GIO)
  void GetMetadataAsync(base::OnceCallback<void(webrtc::DesktopCaptureMetadata)>
                            callback) override;
#endif

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  raw_ptr<webrtc::DesktopCapturer::Callback> callback_ = nullptr;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_WRAPPER_H_

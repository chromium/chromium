// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/desktop_capturer.h"

#if defined(WEBRTC_USE_GIO)
#include "base/functional/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

// DesktopCapturerProxy is responsible for calling webrtc::DesktopCapturer on
// the capturer thread and then returning results to the caller's thread.
// GetSourceList() is not implemented by this class, it always returns false.
class DesktopCapturerProxy : public DesktopCapturer {
 public:
  explicit DesktopCapturerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner);

  DesktopCapturerProxy(const DesktopCapturerProxy&) = delete;
  DesktopCapturerProxy& operator=(const DesktopCapturerProxy&) = delete;

  ~DesktopCapturerProxy() override;

  // CreateCapturer() should be used if the capturer needs to be created on the
  // capturer thread. Otherwise, the capturer can be passed to set_capturer().
  void CreateCapturer(const webrtc::DesktopCaptureOptions& options,
                      SourceId id);
  void set_capturer(std::unique_ptr<webrtc::DesktopCapturer> capturer);

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
  class Core;

  void OnFrameCaptureStarting();
  void OnFrameCaptured(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

#if defined(WEBRTC_USE_GIO)
  void OnMetadata(webrtc::DesktopCaptureMetadata metadata);
#endif

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  raw_ptr<webrtc::DesktopCapturer::Callback> callback_ = nullptr;

#if defined(WEBRTC_USE_GIO)
  base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> metadata_callback_;
#endif

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<DesktopCapturerProxy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

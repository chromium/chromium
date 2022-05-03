// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

class ClientSessionControl;
class DesktopDisplayInfoMonitor;

// DesktopCapturerProxy is responsible for calling webrtc::DesktopCapturer on
// the capturer thread and then returning results to the caller's thread.
// GetSourceList() is not implemented by this class, it always returns false.
// This class optionally loads the list of desktop displays on the UI thread
// (after each captured frame), which will notify the ClientSessionControl
// if the displays have changed.
class DesktopCapturerProxy : public DesktopCapturer {
 public:
  explicit DesktopCapturerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  DesktopCapturerProxy(const DesktopCapturerProxy&) = delete;
  DesktopCapturerProxy& operator=(const DesktopCapturerProxy&) = delete;

  ~DesktopCapturerProxy() override;

  // If a monitor is provided, it will be asked to load the display-info after
  // each captured frame. This is intended only for the single-video-stream
  // case. When multiple video streams are used (each with its own capturer),
  // the display-info will not be loaded by this class.
  void set_desktop_display_info_monitor(
      std::unique_ptr<DesktopDisplayInfoMonitor> monitor);

  // CreateCapturer() should be used if the capturer needs to be created on the
  // capturer thread. Alternatively the capturer can be passed to
  // set_capturer().
  void CreateCapturer(const webrtc::DesktopCaptureOptions& options);
  void set_capturer(std::unique_ptr<webrtc::DesktopCapturer> capturer);

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
#if defined(WEBRTC_USE_GIO)
  void GetMetadataAsync(base::OnceCallback<void(webrtc::DesktopCaptureMetadata)>
                            callback) override;
#endif

  bool SelectSource(SourceId id) override;

 private:
  class Core;

  void OnFrameCaptured(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

#if defined(WEBRTC_USE_GIO)
  void OnMetadata(webrtc::DesktopCaptureMetadata metadata);
#endif

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  raw_ptr<webrtc::DesktopCapturer::Callback> callback_;

  // Monitors and stores info about the desktop displays. Only used in the
  // single-video-stream case.
  std::unique_ptr<DesktopDisplayInfoMonitor> desktop_display_info_monitor_;

#if defined(WEBRTC_USE_GIO)
  base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> metadata_callback_;
#endif
  base::WeakPtrFactory<DesktopCapturerProxy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

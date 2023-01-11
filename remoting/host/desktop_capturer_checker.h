// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_CHECKER_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_CHECKER_H_

#include <memory>

#include "base/functional/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

// DesktopCapturerChecker is a simple DesktopCapturer that captures a single
// screen image (which is ignored). The purpose of this class is to trigger
// the native API's permission check when a screen capture is attempted.
class DesktopCapturerChecker : public webrtc::DesktopCapturer::Callback {
 public:
  DesktopCapturerChecker();

  DesktopCapturerChecker(const DesktopCapturerChecker&) = delete;
  DesktopCapturerChecker& operator=(const DesktopCapturerChecker&) = delete;

  ~DesktopCapturerChecker() override;

  void TriggerSingleCapture();

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_CHECKER_H_

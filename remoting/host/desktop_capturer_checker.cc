// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_capturer_checker.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/client_session_control.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

DesktopCapturerChecker::DesktopCapturerChecker() {}

DesktopCapturerChecker::~DesktopCapturerChecker() {}

void DesktopCapturerChecker::TriggerSingleCapture() {
  DCHECK(!capturer_);

  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();

#if BUILDFLAG(IS_MAC)
  // Use the new capture APIs (on macOS 13.0+) instead of the deprecated ones
  // which may stop working in later versions of macOS. The same permissions
  // work for both APIs.
  // TODO: crbug.com/327458809 - Remove this when WebRTC defaults to using
  // SCK.
  options.set_allow_sck_capturer(true);
#endif

  capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
  DCHECK(capturer_)
      << "Failed to initialize screen capturer for DesktopCapturerChecker.";
  capturer_->Start(this);
  capturer_->CaptureFrame();
}

void DesktopCapturerChecker::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  // Ignore capture result.
}

}  // namespace remoting

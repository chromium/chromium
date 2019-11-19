// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_capturer_checker.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

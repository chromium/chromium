// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/desktop_capturer_wrapper.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

DesktopCapturerWrapper::DesktopCapturerWrapper(
    std::unique_ptr<webrtc::DesktopCapturer> capturer)
    : capturer_(std::move(capturer)),
      scheduler_(
          std::make_unique<protocol::WebrtcFrameSchedulerConstantRate>()) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(capturer_);
}

DesktopCapturerWrapper::~DesktopCapturerWrapper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DesktopCapturerWrapper::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!callback_);

  callback_ = callback;

  if (capturer_) {
    capturer_->Start(this);
  }
  scheduler_->Start(base::BindRepeating(
      &DesktopCapturerWrapper::CaptureFrameInternal, base::Unretained(this)));
}

void DesktopCapturerWrapper::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
  }
}

void DesktopCapturerWrapper::CaptureFrameInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // WebrtcVideoStream expects OnFrameCaptureStart() to be called to create the
  // frame stats. However, currently only the Wayland SharedScreencastStream
  // calls OnFrameCaptureStart(). So we explicitly call it here. If the
  // underlying capturer calls OnFrameCaptureStart(), WebrtcVideoStream will
  // just override its stats, which is harmless.
  callback_->OnFrameCaptureStart();
  if (capturer_) {
    capturer_->CaptureFrame();
  } else {
    OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT, nullptr);
  }
}

void DesktopCapturerWrapper::SelectSource(SourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    capturer_->SelectSource(id);
  }
}

void DesktopCapturerWrapper::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->SetMaxFramerateFps(max_frame_rate);
  if (capturer_) {
    capturer_->SetMaxFrameRate(max_frame_rate);
  }
}

void DesktopCapturerWrapper::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->Pause(pause);
}

void DesktopCapturerWrapper::BoostCaptureRate(base::TimeDelta capture_interval,
                                              base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->BoostCaptureRate(capture_interval, duration);
}

void DesktopCapturerWrapper::OnFrameCaptureStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  callback_->OnFrameCaptureStart();
}

void DesktopCapturerWrapper::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scheduler_->OnFrameCaptured(frame.get());
  callback_->OnCaptureResult(result, std::move(frame));
}

}  // namespace remoting

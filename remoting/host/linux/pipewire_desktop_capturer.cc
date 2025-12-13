// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_desktop_capturer.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

PipewireDesktopCapturer::PipewireDesktopCapturer(
    base::WeakPtr<CaptureStream> stream)
    : stream_(stream) {}

PipewireDesktopCapturer::~PipewireDesktopCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_) {
    stream_->SetCallback(nullptr);
  }
}

bool PipewireDesktopCapturer::SupportsFrameCallbacks() const {
  return kSupportsFrameCallbacks;
}

void PipewireDesktopCapturer::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  if (stream_) {
    stream_->SetCallback(weak_factory_.GetWeakPtr());
  }
}

void PipewireDesktopCapturer::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Capturer will push frames as they are ready.
}

void PipewireDesktopCapturer::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_) {
    stream_->SetMaxFrameRate(max_frame_rate);
  }
}

bool PipewireDesktopCapturer::GetSourceList(SourceList* sources) {
  NOTREACHED();
}

bool PipewireDesktopCapturer::SelectSource(SourceId id) {
  NOTREACHED();
}

void PipewireDesktopCapturer::OnFrameCaptureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_->OnFrameCaptureStart();
}

void PipewireDesktopCapturer::OnCaptureResult(
    Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_->OnCaptureResult(result, std::move(frame));
}

}  // namespace remoting

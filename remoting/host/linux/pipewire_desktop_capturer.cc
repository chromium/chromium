// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_desktop_capturer.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

PipewireDesktopCapturer::PipewireDesktopCapturer(
    base::WeakPtr<PipewireCaptureStream> stream)
    : stream_(std::move(stream)) {}

PipewireDesktopCapturer::~PipewireDesktopCapturer() {
  if (!creating_sequence_->RunsTasksInCurrentSequence()) {
    creating_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipewireCaptureStream::StopVideoCapture, stream_));
    // The callback proxy may continue to receive frames until StopVideoCapture
    // is called, so ensure it is deleted after that.
    creating_sequence_->DeleteSoon(FROM_HERE, callback_proxy_.release());
  } else if (stream_) {
    stream_->StopVideoCapture();
  }
}

bool PipewireDesktopCapturer::SupportsFrameCallbacks() {
  return true;
}

void PipewireDesktopCapturer::Start(Callback* callback) {
  // The capturer is created by GnomeInteractionStrategy on its sequence, but
  // is potentially passed to and owned by a different sequence, which will be
  // the sequence that calls Start().
  capture_sequence_ = base::SequencedTaskRunner::GetCurrentDefault();
  callback_ = callback;
  callback_proxy_ = std::make_unique<CallbackProxy>(
      capture_sequence_, weak_ptr_factory_.GetWeakPtr());
  if (!creating_sequence_->RunsTasksInCurrentSequence()) {
    // Unretained is safe because callback_proxy_ is always deleted on the
    // creating sequence after StopVideoCapture is called.
    creating_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipewireCaptureStream::StartVideoCapture, stream_,
                       base::Unretained(callback_proxy_.get())));
  } else if (stream_) {
    stream_->StartVideoCapture(callback_proxy_.get());
  }
}

void PipewireDesktopCapturer::CaptureFrame() {
  // Capturer will push frames as they are ready.
  DCHECK(capture_sequence_->RunsTasksInCurrentSequence());
}

bool PipewireDesktopCapturer::GetSourceList(SourceList* sources) {
  NOTREACHED();
}

bool PipewireDesktopCapturer::SelectSource(SourceId id) {
  NOTREACHED();
}

void PipewireDesktopCapturer::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  if (!creating_sequence_->RunsTasksInCurrentSequence()) {
    creating_sequence_->PostTask(
        FROM_HERE, base::BindOnce(&PipewireCaptureStream::SetMaxFrameRate,
                                  stream_, max_frame_rate));
  } else if (stream_) {
    stream_->SetMaxFrameRate(max_frame_rate);
  }
}

PipewireDesktopCapturer::CallbackProxy::CallbackProxy(
    scoped_refptr<base::SequencedTaskRunner> capture_sequence,
    base::WeakPtr<PipewireDesktopCapturer> capturer)
    : capture_sequence_(std::move(capture_sequence)),
      capturer_(std::move(capturer)) {}

PipewireDesktopCapturer::CallbackProxy::~CallbackProxy() = default;

void PipewireDesktopCapturer::CallbackProxy::OnFrameCaptureStart() {
  capture_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireDesktopCapturer::OnFrameCaptureStart, capturer_));
}

void PipewireDesktopCapturer::CallbackProxy::OnCaptureResult(
    Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  capture_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&PipewireDesktopCapturer::OnCaptureResult,
                                capturer_, result, std::move(frame)));
}

void PipewireDesktopCapturer::OnFrameCaptureStart() {
  callback_->OnFrameCaptureStart();
}

void PipewireDesktopCapturer::OnCaptureResult(
    Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  callback_->OnCaptureResult(result, std::move(frame));
}

}  // namespace remoting

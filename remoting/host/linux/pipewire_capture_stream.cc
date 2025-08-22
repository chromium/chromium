// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_capture_stream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

PipewireCaptureStream::CallbackProxy::CallbackProxy() = default;

PipewireCaptureStream::CallbackProxy::~CallbackProxy() = default;

void PipewireCaptureStream::CallbackProxy::Initialize(
    base::WeakPtr<PipewireCaptureStream> parent) {
  base::AutoLock lock(lock_);
  callback_sequence_ = base::SequencedTaskRunner::GetCurrentDefault();
  parent_ = parent;
}

void PipewireCaptureStream::CallbackProxy::OnFrameCaptureStart() {
  base::AutoLock lock(lock_);
  if (!callback_sequence_) {
    // Not initialized yet.
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireCaptureStream::OnFrameCaptureStart, parent_));
}

void PipewireCaptureStream::CallbackProxy::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  base::AutoLock lock(lock_);
  if (!callback_sequence_) {
    // Not initialized yet.
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&PipewireCaptureStream::OnCaptureResult,
                                parent_, result, std::move(frame)));
}

PipewireCaptureStream::PipewireCaptureStream() = default;

PipewireCaptureStream::~PipewireCaptureStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PipewireCaptureStream::SetPipeWireStream(
    std::uint32_t pipewire_node,
    const webrtc::DesktopSize& initial_resolution,
    std::string mapping_id,
    int pipewire_fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pipewire_node_ = pipewire_node;
  resolution_ = initial_resolution;
  mapping_id_ = std::move(mapping_id);
  pipewire_fd_ = pipewire_fd;
}

void PipewireCaptureStream::StartVideoCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->StartScreenCastStream(pipewire_node_, pipewire_fd_,
                                 resolution_.width(), resolution_.height(),
                                 false, &callback_proxy_);
}

void PipewireCaptureStream::SetCallback(
    base::WeakPtr<webrtc::DesktopCapturer::Callback> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  auto self = weak_ptr_factory_.GetWeakPtr();
  // RecaptureLatestFrameAsDirty() must be called before
  // callback_proxy_.Initialize(), since calling the latter will immediately
  // start pumping frames to `PipewireCaptureStream` and can potentially cause
  // race conditions.
  RecaptureLatestFrameAsDirty();
  // While unlikely, RecaptureLatestFrameAsDirty() runs `callback_` in the
  // current stack frame and could potentially delete `this`, so we should only
  // access class members if the weak pointer remains valid.
  if (self) {
    callback_proxy_.Initialize(self);
  }
}

void PipewireCaptureStream::SetUseDamageRegion(bool use_damage_region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetUseDamageRegion(use_damage_region);
  RecaptureLatestFrameAsDirty();
}

void PipewireCaptureStream::SetResolution(
    const webrtc::DesktopSize& new_resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resolution_ = new_resolution;
  stream_->UpdateScreenCastStreamResolution(resolution_.width(),
                                            resolution_.height());
}

void PipewireCaptureStream::SetMaxFrameRate(std::uint32_t frame_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->UpdateScreenCastStreamFrameRate(frame_rate);
}

std::unique_ptr<webrtc::MouseCursor> PipewireCaptureStream::CaptureCursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->CaptureCursor();
}

std::optional<webrtc::DesktopVector>
PipewireCaptureStream::CaptureCursorPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->CaptureCursorPosition();
}

void PipewireCaptureStream::StopVideoCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->StopScreenCastStream();
}

std::string_view PipewireCaptureStream::mapping_id() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapping_id_;
}

base::WeakPtr<PipewireCaptureStream> PipewireCaptureStream::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void PipewireCaptureStream::RecaptureLatestFrameAsDirty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_capturing_frame_) {
    should_mark_current_frame_dirty_ = true;
    return;
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  OnFrameCaptureStart();
  // While unlikely, OnFrameCaptureStart() runs `callback_` in the current stack
  // frame and could potentially delete `this`, so we should only access class
  // members if the weak pointer remains valid.
  if (!self) {
    return;
  }
  // Note: CaptureFrame() does not really capture a new frame. It just returns
  // the latest available frame, or null if it's unavailable.
  auto frame = stream_->CaptureFrame();
  if (frame) {
    // Mark the entire frame as dirty.
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeSize(frame->size()));
    OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS, std::move(frame));
  } else {
    OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
  }
}

void PipewireCaptureStream::OnFrameCaptureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_frame_ = true;
  if (callback_) {
    callback_->OnFrameCaptureStart();
  }
}

void PipewireCaptureStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_frame_ = false;

  if (frame) {
    if (!should_mark_current_frame_dirty_) {
      // Check to see if the updated region is invalid, which may happen if the
      // frame with an invalid updated region is received before
      // SetUseDamageRegion(false) is called. If this happens, we mark the
      // entire frame dirty. Note that the updated region could still be invalid
      // even if the check passes, e.g., the monitor offset changes slightly so
      // the updated rectangles still remain in the desktop rectangle.
      // SetUseDamageRegion() will call RecaptureLatestFrameAsDirty() to cover
      // that.
      auto updated_region_it =
          webrtc::DesktopRegion::Iterator(frame->updated_region());
      while (!updated_region_it.IsAtEnd()) {
        if (updated_region_it.rect().left() < 0 ||
            updated_region_it.rect().top() < 0 ||
            updated_region_it.rect().right() > frame->size().width() ||
            updated_region_it.rect().bottom() > frame->size().height()) {
          should_mark_current_frame_dirty_ = true;
          break;
        }
        updated_region_it.Advance();
      }
    }
    if (should_mark_current_frame_dirty_) {
      frame->mutable_updated_region()->SetRect(
          webrtc::DesktopRect::MakeSize(frame->size()));
    }
  }

  should_mark_current_frame_dirty_ = false;
  if (callback_) {
    callback_->OnCaptureResult(result, std::move(frame));
  }
}

}  // namespace remoting

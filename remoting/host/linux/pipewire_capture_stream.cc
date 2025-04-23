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

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/base/screen_resolution.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

PipewireCaptureStream::PipewireCaptureStream() = default;

PipewireCaptureStream::~PipewireCaptureStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PipewireCaptureStream::SetPipeWireStream(
    std::uint32_t pipewire_node,
    ScreenResolution initial_resolution,
    std::string mapping_id,
    int pipewire_fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pipewire_node_ = pipewire_node;
  resolution_ = initial_resolution;
  mapping_id_ = std::move(mapping_id);
  pipewire_fd_ = pipewire_fd;
}

void PipewireCaptureStream::StartVideoCapture(
    webrtc::DesktopCapturer::Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->StartScreenCastStream(
      pipewire_node_, pipewire_fd_, resolution_.dimensions().width(),
      resolution_.dimensions().height(), false, callback);
}

void PipewireCaptureStream::SetResolution(ScreenResolution new_resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resolution_ = new_resolution;
  stream_->UpdateScreenCastStreamResolution(resolution_.dimensions().width(),
                                            resolution_.dimensions().height());
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

}  // namespace remoting

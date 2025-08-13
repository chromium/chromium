// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/host/linux/gnome_interaction_strategy.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"

namespace remoting {

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<PipewireCaptureStreamManager> stream_manager)
    : stream_manager_(stream_manager) {}

GnomeDesktopResizer::~GnomeDesktopResizer() = default;

ScreenResolution GnomeDesktopResizer::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return {};
  }
  base::WeakPtr<PipewireCaptureStream> stream =
      stream_manager_->GetStream(screen_id);
  if (!stream) {
    LOG(ERROR) << "Cannot find pipewire stream for screen ID: " << screen_id;
    return {};
  }
  return stream->resolution();
}

std::list<ScreenResolution> GnomeDesktopResizer::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  return {preferred};
}

void GnomeDesktopResizer::SetResolution(const ScreenResolution& resolution,
                                        webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  base::WeakPtr<PipewireCaptureStream> stream =
      stream_manager_->GetStream(screen_id);
  if (!stream) {
    LOG(ERROR) << "Cannot find pipewire stream for screen ID: " << screen_id;
    return;
  }
  stream->SetResolution(resolution);
}

void GnomeDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {}

void GnomeDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  for (const auto& track : layout.video_track()) {
    if (!track.has_screen_id()) {
      stream_manager_->AddStream(
          {{track.width(), track.height()}, {track.x_dpi(), track.y_dpi()}},
          base::BindOnce(&GnomeDesktopResizer::OnAddStreamResult,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void GnomeDesktopResizer::OnAddStreamResult(
    PipewireCaptureStreamManager::AddStreamResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to add stream: " << result.error();
    return;
  }
  // TODO: crbug.com/432217140 - Configure offset and scale by calling
  // ApplyMonitorsConfig via D-Bus.
}

}  // namespace remoting

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include "remoting/host/linux/gnome_interaction_strategy.h"

namespace remoting {

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<GnomeInteractionStrategy> session)
    : session_(std::move(session)) {}

GnomeDesktopResizer::~GnomeDesktopResizer() = default;

ScreenResolution GnomeDesktopResizer::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  if (!session_) {
    return {};
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_->sequence_checker_);
  return session_->capture_stream_.resolution();
}

std::list<ScreenResolution> GnomeDesktopResizer::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  return {preferred};
}

void GnomeDesktopResizer::SetResolution(const ScreenResolution& resolution,
                                        webrtc::ScreenId screen_id) {
  if (!session_) {
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_->sequence_checker_);
  session_->capture_stream_.SetResolution(resolution);
}

void GnomeDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {}

void GnomeDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {}

}  // namespace remoting

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_proxy.h"

#include <utility>

#include "base/check.h"

namespace remoting {

DesktopResizerProxy::DesktopResizerProxy(
    base::WeakPtr<DesktopResizer> desktop_resizer)
    : desktop_resizer_(std::move(desktop_resizer)) {
  DCHECK(desktop_resizer_);
}

DesktopResizerProxy::~DesktopResizerProxy() = default;

ScreenResolution DesktopResizerProxy::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  if (desktop_resizer_) {
    return desktop_resizer_->GetCurrentResolution(screen_id);
  }
  return {};
}

std::list<ScreenResolution> DesktopResizerProxy::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  if (desktop_resizer_) {
    return desktop_resizer_->GetSupportedResolutions(preferred, screen_id);
  }
  return {};
}

void DesktopResizerProxy::SetResolution(const ScreenResolution& resolution,
                                        webrtc::ScreenId screen_id) {
  if (desktop_resizer_) {
    desktop_resizer_->SetResolution(resolution, screen_id);
  }
}

void DesktopResizerProxy::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {
  if (desktop_resizer_) {
    desktop_resizer_->RestoreResolution(original, screen_id);
  }
}

void DesktopResizerProxy::SetVideoLayout(const protocol::VideoLayout& layout) {
  if (desktop_resizer_) {
    desktop_resizer_->SetVideoLayout(layout);
  }
}

}  // namespace remoting

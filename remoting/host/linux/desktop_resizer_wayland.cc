// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_resizer_wayland.h"

#include "base/notreached.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/wayland_manager.h"

namespace remoting {

DesktopResizerWayland::~DesktopResizerWayland() = default;

ScreenResolution DesktopResizerWayland::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  // TODO(crbug.com/40266740): Need to find a way to get the resolution from
  // the capturer via wayland manager to avoid spurious resizing operations.
  // Note: This will become more trickier for the multi-mon case.
  return ScreenResolution();
}

std::list<ScreenResolution> DesktopResizerWayland::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  return {preferred};
}

void DesktopResizerWayland::RestoreResolution(const ScreenResolution& original,
                                              webrtc::ScreenId screen_id) {
  NOTIMPLEMENTED();
}

void DesktopResizerWayland::SetResolution(const ScreenResolution& resolution,
                                          webrtc::ScreenId screen_id) {
  WaylandManager::Get()->OnUpdateScreenResolution(resolution, screen_id);
}

void DesktopResizerWayland::SetVideoLayout(
    const protocol::VideoLayout& layout) {
  NOTIMPLEMENTED();
}

}  // namespace remoting

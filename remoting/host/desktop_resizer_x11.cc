// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_x11.h"

namespace remoting {

DesktopResizerX11::DesktopResizerX11() = default;
DesktopResizerX11::~DesktopResizerX11() = default;

// DesktopResizer interface
ScreenResolution DesktopResizerX11::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  return resizer_.GetCurrentResolution(screen_id);
}
std::list<ScreenResolution> DesktopResizerX11::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  return resizer_.GetSupportedResolutions(preferred, screen_id);
}
void DesktopResizerX11::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  resizer_.SetResolution(resolution, screen_id);
}
void DesktopResizerX11::RestoreResolution(const ScreenResolution& original,
                                          webrtc::ScreenId screen_id) {
  resizer_.SetResolution(original, screen_id);
}
void DesktopResizerX11::SetVideoLayout(const protocol::VideoLayout& layout) {
  resizer_.SetVideoLayout(layout);
}

}  // namespace remoting

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_x11.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_geometry.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace remoting {

namespace {

ScreenResolution DesktopResolutionToScreenResolution(
    const DesktopResolution& resolution) {
  return ScreenResolution(
      webrtc::DesktopSize(resolution.dimensions().width(),
                          resolution.dimensions().height()),
      webrtc::DesktopVector(resolution.dpi().x(), resolution.dpi().y()));
}

DesktopResolution ScreenResolutionToDesktopResolution(
    const ScreenResolution& resolution) {
  return DesktopResolution(
      gfx::Size(resolution.dimensions().width(),
                resolution.dimensions().height()),
      gfx::Vector2d(resolution.dpi().x(), resolution.dpi().y()));
}

}  // namespace

DesktopResizerX11::DesktopResizerX11() = default;
DesktopResizerX11::~DesktopResizerX11() = default;

// DesktopResizer interface
ScreenResolution DesktopResizerX11::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  return DesktopResolutionToScreenResolution(
      resizer_.GetCurrentResolution(static_cast<DesktopScreenId>(screen_id)));
}
std::list<ScreenResolution> DesktopResizerX11::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  std::list<DesktopResolution> resolutions = resizer_.GetSupportedResolutions(
      ScreenResolutionToDesktopResolution(preferred), screen_id);
  std::list<ScreenResolution> result;
  base::ranges::transform(resolutions, std::back_inserter(result),
                          DesktopResolutionToScreenResolution);
  return result;
}
void DesktopResizerX11::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  resizer_.SetResolution(ScreenResolutionToDesktopResolution(resolution),
                         screen_id);
}
void DesktopResizerX11::RestoreResolution(const ScreenResolution& original,
                                          webrtc::ScreenId screen_id) {
  resizer_.SetResolution(ScreenResolutionToDesktopResolution(original),
                         screen_id);
}
void DesktopResizerX11::SetVideoLayout(const protocol::VideoLayout& layout) {
  DesktopLayoutSet desktop_layouts;
  if (layout.has_primary_screen_id()) {
    desktop_layouts.primary_screen_id = layout.primary_screen_id();
  }
  for (const auto& track : layout.video_track()) {
    desktop_layouts.layouts.emplace_back(
        track.has_screen_id() ? std::make_optional(track.screen_id())
                              : std::nullopt,
        gfx::Rect(track.position_x(), track.position_y(), track.width(),
                  track.height()),
        gfx::Vector2d(track.x_dpi(), track.y_dpi()));
  }
  resizer_.SetVideoLayout(desktop_layouts);
}

}  // namespace remoting

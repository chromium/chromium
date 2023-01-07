// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <memory>
#include <string>

#include "base/notreached.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/wayland_manager.h"

namespace remoting {

class DesktopResizerWayland : public DesktopResizer {
 public:
  DesktopResizerWayland() {}
  DesktopResizerWayland(const DesktopResizerWayland&) = delete;
  DesktopResizerWayland& operator=(const DesktopResizerWayland&) = delete;
  ~DesktopResizerWayland() override;

  // DesktopResizer interface
  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
};

DesktopResizerWayland::~DesktopResizerWayland() = default;

ScreenResolution DesktopResizerWayland::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  // TODO(salmanmalik): Need to find a way to get the resolution from
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

// static
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return std::make_unique<DesktopResizerWayland>();
}

}  // namespace remoting

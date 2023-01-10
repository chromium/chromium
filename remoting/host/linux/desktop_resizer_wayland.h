// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_RESIZER_WAYLAND_H_
#define REMOTING_HOST_LINUX_DESKTOP_RESIZER_WAYLAND_H_

#include <memory>

#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_resizer.h"

namespace remoting {

class DesktopResizerWayland : public DesktopResizer {
 public:
  DesktopResizerWayland() = default;
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

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_RESIZER_WAYLAND_H_

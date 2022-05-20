// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <memory>

#include "base/notreached.h"

namespace remoting {

namespace {

// DesktopResizer implementation for Linux platforms where
// X11 is not enabled.
class DesktopResizerLinux : public DesktopResizer {
 public:
  DesktopResizerLinux() = default;
  DesktopResizerLinux(const DesktopResizerLinux&) = delete;
  DesktopResizerLinux& operator=(const DesktopResizerLinux&) = delete;
  ~DesktopResizerLinux() override = default;

  ScreenResolution GetCurrentResolution(
      absl::optional<webrtc::ScreenId> screen_id) override {
    NOTIMPLEMENTED();
    return ScreenResolution();
  }

  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      absl::optional<webrtc::ScreenId> screen_id) override {
    NOTIMPLEMENTED();
    return std::list<ScreenResolution>();
  }

  void SetResolution(const ScreenResolution& resolution,
                     absl::optional<webrtc::ScreenId> screen_id) override {
    NOTIMPLEMENTED();
  }

  void RestoreResolution(const ScreenResolution& original,
                         absl::optional<webrtc::ScreenId> screen_id) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

// static
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return std::make_unique<DesktopResizerLinux>();
}

}  // namespace remoting

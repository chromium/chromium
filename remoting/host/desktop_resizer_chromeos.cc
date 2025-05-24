// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <list>
#include <memory>

#include "base/notimplemented.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

namespace {

// No-op DesktopResizer implementation for Chrome OS
class DesktopResizerChromeOS : public DesktopResizer {
 public:
  DesktopResizerChromeOS() = default;
  DesktopResizerChromeOS(const DesktopResizerChromeOS&) = delete;
  DesktopResizerChromeOS& operator=(const DesktopResizerChromeOS&) = delete;
  ~DesktopResizerChromeOS() override = default;

  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override {
    NOTIMPLEMENTED();
    return ScreenResolution();
  }

  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override {
    NOTIMPLEMENTED();
    return std::list<ScreenResolution>();
  }

  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override {
    NOTIMPLEMENTED();
  }

  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override {
    NOTIMPLEMENTED();
  }

  void SetVideoLayout(const protocol::VideoLayout& layout) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return std::make_unique<DesktopResizerChromeOS>();
}

}  // namespace remoting

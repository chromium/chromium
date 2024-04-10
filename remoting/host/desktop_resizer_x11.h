// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_X11_H_
#define REMOTING_HOST_DESKTOP_RESIZER_X11_H_

#include "remoting/host/desktop_resizer.h"
#include "remoting/host/x11_desktop_resizer.h"

namespace remoting {

class DesktopResizerX11 : public DesktopResizer {
 public:
  DesktopResizerX11();
  DesktopResizerX11(const DesktopResizerX11&) = delete;
  DesktopResizerX11& operator=(const DesktopResizerX11&) = delete;
  ~DesktopResizerX11() override;

  // DesktopResizer interface
  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;

 private:
  X11DesktopResizer resizer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_X11_H_

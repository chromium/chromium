// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/desktop_resizer.h"

namespace remoting {

class GnomeInteractionStrategy;

class GnomeDesktopResizer : public DesktopResizer {
 public:
  explicit GnomeDesktopResizer(base::WeakPtr<GnomeInteractionStrategy> session);
  GnomeDesktopResizer(const GnomeDesktopResizer&) = delete;
  GnomeDesktopResizer& operator=(const GnomeDesktopResizer&) = delete;
  ~GnomeDesktopResizer() override;

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
  base::WeakPtr<GnomeInteractionStrategy> session_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

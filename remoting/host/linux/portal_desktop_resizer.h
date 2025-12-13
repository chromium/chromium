// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_PORTAL_DESKTOP_RESIZER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/capture_stream_manager.h"

namespace remoting {

class PortalDesktopResizer : public DesktopResizer {
 public:
  // `stream_manager` must outlive `this`.
  explicit PortalDesktopResizer(CaptureStreamManager& stream_manager);
  PortalDesktopResizer(const PortalDesktopResizer&) = delete;
  PortalDesktopResizer& operator=(const PortalDesktopResizer&) = delete;
  ~PortalDesktopResizer() override;

  // DesktopResizer interface.
  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;

  base::WeakPtr<PortalDesktopResizer> GetWeakPtr();

 private:
  raw_ptr<CaptureStreamManager> stream_manager_;
  base::WeakPtrFactory<PortalDesktopResizer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_DESKTOP_RESIZER_H_

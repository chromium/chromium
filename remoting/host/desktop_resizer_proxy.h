// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_PROXY_H_
#define REMOTING_HOST_DESKTOP_RESIZER_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/desktop_resizer.h"

namespace remoting {

// Simple class that delegates calls to the underlying `desktop_resizer`. Once
// `desktop_resizer` becomes invalidates, all calls will either be no-op, or
// return an empty result.
class DesktopResizerProxy : public DesktopResizer {
 public:
  explicit DesktopResizerProxy(base::WeakPtr<DesktopResizer> desktop_resizer);
  ~DesktopResizerProxy() override;

  DesktopResizerProxy(const DesktopResizerProxy&) = delete;
  DesktopResizerProxy& operator=(const DesktopResizerProxy&) = delete;

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

 private:
  base::WeakPtr<DesktopResizer> desktop_resizer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_PROXY_H_

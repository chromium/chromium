// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_H_
#define REMOTING_HOST_DESKTOP_RESIZER_H_

#include <list>
#include <memory>

#include "remoting/host/base/screen_resolution.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Interface for resizing the desktop displays. These methods take an optional
// |screen_id| parameter to resize an individual monitor. If |screen_id| refers
// to a monitor that no longer exists, the implementation should do nothing, or
// return empty data. If |screen_id| is not provided, the implementation should
// operate on the single monitor if there is only one. If there are several
// monitors, the implementation should fall back to the legacy (per-platform)
// behavior.
class DesktopResizer {
 public:
  virtual ~DesktopResizer() {}

  // Create a platform-specific DesktopResizer instance.
  static std::unique_ptr<DesktopResizer> Create();

  // Return the current resolution of the monitor.
  virtual ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) = 0;

  // Get the list of supported resolutions for the monitor, which should ideally
  // include |preferred|. Implementations will generally do one of the
  // following:
  //   1. Return the list of resolutions supported by the underlying video
  //      driver, regardless of |preferred|.
  //   2. Return a list containing just |preferred|, perhaps after imposing
  //      some minimum size constraint. This will typically be the case if
  //      there are no constraints imposed by the underlying video driver.
  //   3. Return an empty list if resize is not supported.
  virtual std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) = 0;

  // Set the resolution of the monitor. |resolution| must be one of the
  // resolutions previously returned by |GetSupportedResolutions|. Note that
  // implementations should fail gracefully if the specified resolution is no
  // longer supported, since monitor configurations may change on the fly.
  virtual void SetResolution(const ScreenResolution& resolution,
                             webrtc::ScreenId screen_id) = 0;

  // Restore the original monitor resolution. The caller must provide the
  // original resolution of the monitor, as returned by GetCurrentResolution(),
  // as a hint. However, implementations are free to ignore this. For example,
  // virtual hosts will typically ignore it to avoid unnecessary resizes.
  virtual void RestoreResolution(const ScreenResolution& original,
                                 webrtc::ScreenId screen_id) = 0;

  // Updates current display layout to match |layout|. If a video track doesn't
  // have screen_id, a new monitor will be added with the matching geometry.
  // This method has the same requirements and behavior as SetResolution()
  // regarding the screen resolution.
  virtual void SetVideoLayout(const protocol::VideoLayout& layout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_SCREEN_CONTROLS_H_
#define REMOTING_HOST_BASE_SCREEN_CONTROLS_H_

#include <optional>

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

namespace protocol {
class VideoLayout;
}  // namespace protocol

class ScreenResolution;

// Used to change the screen resolution (both dimensions and DPI).
class ScreenControls {
 public:
  virtual ~ScreenControls() = default;

  // If |resolution| is not empty, attempts to set new screen resolution for
  // the monitor |screen_id|. If |resolution| is empty, attempts to restore the
  // original resolution of |screen_id|. Resizing a monitor may cause other
  // monitors to be repositioned to accommodate the new size.
  //
  // If |screen_id| is not provided:
  // If there is only 1 monitor, the monitor should be resized to |resolution|
  // or restored if |resolution| is empty. If there is more than 1 monitor, the
  // implementation should fall back to the behavior of older hosts (for
  // example, the request may be ignored, or only the primary display may be
  // resized).
  virtual void SetScreenResolution(
      const ScreenResolution& resolution,
      std::optional<webrtc::ScreenId> screen_id) = 0;

  virtual void SetVideoLayout(const protocol::VideoLayout& video_layout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_SCREEN_CONTROLS_H_

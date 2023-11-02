// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_SCREEN_RESOLUTION_H_
#define REMOTING_HOST_BASE_SCREEN_RESOLUTION_H_

#include "base/compiler_specific.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

// Describes a screen's dimensions and DPI.
class ScreenResolution {
 public:
  ScreenResolution();
  ScreenResolution(const webrtc::DesktopSize& dimensions,
                   const webrtc::DesktopVector& dpi);

  // Returns the screen dimensions scaled according to the passed |new_dpi|.
  webrtc::DesktopSize ScaleDimensionsToDpi(
      const webrtc::DesktopVector& new_dpi) const;

  // Dimensions of the screen in pixels.
  const webrtc::DesktopSize& dimensions() const { return dimensions_; }

  // The vertical and horizontal DPI of the screen.
  const webrtc::DesktopVector& dpi() const { return dpi_; }

  // Returns true if |dimensions_| specifies an empty rectangle.
  bool IsEmpty() const;

  // Returns true if the dimensions and DPI of the two resolutions match.
  bool Equals(const ScreenResolution& other) const;

 private:
  webrtc::DesktopSize dimensions_;
  webrtc::DesktopVector dpi_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_SCREEN_RESOLUTION_H_

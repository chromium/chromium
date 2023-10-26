// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_
#define REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_

#include "ui/gfx/geometry/point_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace remoting {

// This class performs coordinate transformations between the different
// coordinate systems used in ChromeOS.
//
// To understand the different coordinate systems, you must understand the
// following concepts:
//
// Screen vs window coordinates:
//    * Window coordinates are relative to a given window. So 2 different
//      windows both have a 0x0 coordinate.
//      Because each physical display has its own root window, window
//      coordinates can also be used to express coordinates relative to a
//      display.
//    * Screen coordinates are absolute. Each root window is placed at a
//      different, non-overlapping place in the screen. This way screen
//      coordinates are 'globally unique', meaning that they can uniquely
//      identify any location in any root window.
//
// DIP vs pixels:
//    * Pixel coordinates use the physical pixel location of windows/displays.
//      This means a screen with physical resolution 3000x2000 will always have
//      a size of 3000x2000 in pixel coordinates, even if the screen is rotated
//      or uses a different scale factor (to make things look bigger).
//    * DIP coordinates use Device-Independent-Pixels, which is something that
//      can be changed in the software to make items on the screen look bigger
//      or smaller. So a screen with physical resolution 3000x2000 and a scale
//      factor of 2 will have a DIP resolution of 1500x1000.
//      DIP coordinates also take rotation into account, so if the above screen
//      is rotated 90 degrees in the software, it will have a DIP resolution of
//      1000x1500.
//
//
// These 2 concepts are orthogonal and can be combined, resulting in 4 different
// coordinate systems:
//    * Screen DIP: Globally unique coordinates that take rotation and scale
//      factor into account. This is the system the user intuitively expects,
//      and what the user sees in the display settings.
//    * Screen Pixel: Globally unique coordinates that represent the physical
//      pixels of each display. This is what is used by the SystemInputInjector
//      class.
//    * Window DIP: Coordinates relative to a given window (display) that take
//      rotation and scale factor into account.
//    * Window Pixel: Coordinates relative to a given window (display) that
//      match the physical pixels of the display. This is used by the event
//      processing pipeline (i.e. the ui::PlatformEvent).
//
// All methods use floating points to minimize the rounding errors when
// transforming between the different coordinate systems.
class PointTransformer {
 public:
  static gfx::PointF ConvertScreenInDipToScreenInPixel(
      gfx::PointF location_in_screen_in_dip);

  static gfx::PointF ConvertWindowInPixelToScreenInDip(
      const aura::Window& window,
      gfx::PointF location_in_window_in_pixels);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_POINT_TRANSFORMER_H_

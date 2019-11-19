// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info.h"

#include "build/build_config.h"
#include "remoting/base/constants.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace remoting {

DesktopDisplayInfo::DesktopDisplayInfo() = default;

DesktopDisplayInfo::~DesktopDisplayInfo() = default;

bool DesktopDisplayInfo::operator==(const DesktopDisplayInfo& other) {
  if (other.displays_.size() == displays_.size()) {
    for (size_t display = 0; display < displays_.size(); display++) {
      DisplayGeometry this_display = displays_[display];
      DisplayGeometry other_display = other.displays_[display];
      if (this_display.id != other_display.id ||
          this_display.x != other_display.x ||
          this_display.y != other_display.y ||
          this_display.width != other_display.width ||
          this_display.height != other_display.height ||
          this_display.dpi != other_display.dpi ||
          this_display.bpp != other_display.bpp ||
          this_display.is_default != other_display.is_default) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool DesktopDisplayInfo::operator!=(const DesktopDisplayInfo& other) {
  return !(*this == other);
}

/* static */
webrtc::DesktopSize DesktopDisplayInfo::CalcSizeDips(webrtc::DesktopSize size,
                                                     int dpi_x,
                                                     int dpi_y) {
  // Guard against invalid input.
  // TODO: Replace with a DCHECK, once crbug.com/938648 is fixed.
  if (dpi_x == 0)
    dpi_x = kDefaultDpi;
  if (dpi_y == 0)
    dpi_y = kDefaultDpi;

  webrtc::DesktopSize size_dips(size.width() * kDefaultDpi / dpi_x,
                                size.height() * kDefaultDpi / dpi_y);
  return size_dips;
}

void DesktopDisplayInfo::Reset() {
  displays_.clear();
}

int DesktopDisplayInfo::NumDisplays() {
  return displays_.size();
}

const DisplayGeometry* DesktopDisplayInfo::GetDisplayInfo(unsigned int id) {
  if (id >= displays_.size())
    return nullptr;
  return &displays_[id];
}

// Calculate the offset from the upper-left of the desktop to the origin of
// the specified display.
//
// x         b-----------+            ---
//           |           |             |  y-offset to c
// a---------+           |             |
// |         +-------c---+-------+    ---
// |         |       |           |
// +---------+       |           |
//                   +-----------+
//
// |-----------------|
//    x-offset to c
//
// x = upper left of desktop
// a,b,c = origin of display A,B,C
webrtc::DesktopVector DesktopDisplayInfo::CalcDisplayOffset(
    unsigned int disp_id) {
  if (disp_id >= displays_.size()) {
    LOG(INFO) << "Invalid display id for CalcDisplayOffset: " << disp_id;
    return webrtc::DesktopVector();
  }

  DisplayGeometry disp_info = displays_[disp_id];
  webrtc::DesktopVector origin(disp_info.x, disp_info.y);

  // Find topleft-most display coordinate. This is the topleft of the desktop.
  int dx = 0;
  int dy = 0;
  for (size_t id = 0; id < displays_.size(); id++) {
    DisplayGeometry disp = displays_[id];
    if (disp.x < dx)
      dx = disp.x;
    if (disp.y < dy)
      dy = disp.y;
  }
  webrtc::DesktopVector topleft(dx, dy);
  return origin.subtract(topleft);
}

void DesktopDisplayInfo::AddDisplay(DisplayGeometry* display) {
  displays_.push_back(*display);
}

void DesktopDisplayInfo::AddDisplayFrom(protocol::VideoTrackLayout track) {
  auto* display = new DisplayGeometry();
  display->x = track.position_x();
  display->y = track.position_y();
  display->width = track.width();
  display->height = track.height();
  display->dpi = track.x_dpi();
  display->bpp = 24;
  display->is_default = false;
  displays_.push_back(*display);
}

#if !defined(OS_MACOSX)
void DesktopDisplayInfo::LoadCurrentDisplayInfo() {
  displays_.clear();

#if defined(OS_WIN)
  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DisplayGeometry info;
    info.id = device_index;

    DISPLAY_DEVICE device = {};
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result)
      break;

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    info.is_default = false;
    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      info.is_default = true;

    // Get additional info about device.
    DEVMODE devmode;
    devmode.dmSize = sizeof(devmode);
    EnumDisplaySettingsEx(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode,
                          0);

    info.x = devmode.dmPosition.x;
    info.y = devmode.dmPosition.y;
    info.width = devmode.dmPelsWidth;
    info.height = devmode.dmPelsHeight;
    info.dpi = devmode.dmLogPixels;
    info.bpp = devmode.dmBitsPerPel;
    displays_.push_back(info);
  }
#endif  // OS_WIN
}
#endif  // !OS_MACOSX

}  // namespace remoting

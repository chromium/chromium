// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include <windows.h>

#include <algorithm>
#include <limits>

namespace remoting {

namespace {

class DesktopDisplayInfoLoaderWin : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderWin() = default;
  ~DesktopDisplayInfoLoaderWin() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderWin::GetCurrentDisplayInfo() {
  int32_t lowest_x = std::numeric_limits<int32_t>::max();
  int32_t lowest_y = std::numeric_limits<int32_t>::max();
  std::vector<DisplayGeometry> displays;
  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DISPLAY_DEVICE device = {};
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result) {
      break;
    }

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
      continue;
    }

    bool is_default = false;
    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      is_default = true;
    }

    // Get additional info about device.
    DEVMODE devmode;
    devmode.dmSize = sizeof(devmode);
    EnumDisplaySettingsEx(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode,
                          0);

    DisplayGeometry info;
    info.id = device_index;
    info.is_default = is_default;
    info.x = devmode.dmPosition.x;
    info.y = devmode.dmPosition.y;
    info.width = devmode.dmPelsWidth;
    info.height = devmode.dmPelsHeight;
    info.dpi = devmode.dmLogPixels;
    info.bpp = devmode.dmBitsPerPel;
    displays.push_back(info);

    lowest_x = std::min(info.x, lowest_x);
    lowest_y = std::min(info.y, lowest_y);
  }

  // Normalize the displays so the bounding-box's top-left corner is at (0, 0).
  // This matches the coordinate system used by InputInjectorWin, so that
  // the FractionalInputFilter produces correct x,y-coordinates for injection.
  DesktopDisplayInfo result;
  for (DisplayGeometry& info : displays) {
    info.x -= lowest_x;
    info.y -= lowest_y;
    result.AddDisplay(info);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderWin>();
}

}  // namespace remoting

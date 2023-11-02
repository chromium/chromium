// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include <windows.h>

namespace remoting {

namespace {

class DesktopDisplayInfoLoaderWin : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderWin() = default;
  ~DesktopDisplayInfoLoaderWin() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderWin::GetCurrentDisplayInfo() {
  DesktopDisplayInfo result;

  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DISPLAY_DEVICE device = {};
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result)
      break;

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    bool is_default = false;
    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      is_default = true;

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

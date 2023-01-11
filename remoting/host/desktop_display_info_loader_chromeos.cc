// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include "remoting/host/chromeos/ash_proxy.h"
#include "ui/gfx/geometry/size.h"

namespace remoting {

namespace {

// Get the dimensions of this display in DIP (which is the actual resolution
// visible to the user). The returned dimensions also respect rotation.
//
// Examples:
//     - A display of 3000x2000 with scale factor of 2
//         --> result 1500x1000
//     - A display of 3000x2000 with 90 degrees rotation
//         --> result 2000x3000
gfx::Size GetDimensionsInDip(const display::Display& display) {
  gfx::Size dimensions{display.bounds().size()};
  dimensions =
      gfx::ScaleToFlooredSize(dimensions, display.device_scale_factor());
  // Note: We do not need to rotate here, as display.bounds() already takes
  // the rotation into account.
  return dimensions;
}

DisplayGeometry ToDisplayGeometry(const display::Display& display,
                                  DisplayId primary_display_id) {
  gfx::Size dimensions = GetDimensionsInDip(display);

  return DisplayGeometry{
      .id = display.id(),
      .x = display.bounds().x(),
      .y = display.bounds().y(),
      .width = static_cast<uint32_t>(dimensions.width()),
      .height = static_cast<uint32_t>(dimensions.height()),
      .dpi = static_cast<uint32_t>(
          AshProxy::ScaleFactorToDpi(display.device_scale_factor())),
      .is_default = (display.id() == primary_display_id),
  };
}

class DesktopDisplayInfoLoaderChromeOs : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderChromeOs() = default;
  ~DesktopDisplayInfoLoaderChromeOs() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderChromeOs::GetCurrentDisplayInfo() {
  const DisplayId primary_display_id = AshProxy::Get().GetPrimaryDisplayId();

  auto result = DesktopDisplayInfo();
  for (auto& display : AshProxy::Get().GetActiveDisplays()) {
    result.AddDisplay(ToDisplayGeometry(display, primary_display_id));
  }

  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderChromeOs>();
}

}  // namespace remoting

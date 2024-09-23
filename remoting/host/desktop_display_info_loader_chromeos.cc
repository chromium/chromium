// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include "remoting/host/chromeos/ash_proxy.h"

namespace remoting {

namespace {

DisplayGeometry ToDisplayGeometry(const display::Display& display,
                                  DisplayId primary_display_id) {
  return DisplayGeometry{
      .id = display.id(),
      .x = display.bounds().x(),
      .y = display.bounds().y(),
      .width = static_cast<uint32_t>(display.bounds().width()),
      .height = static_cast<uint32_t>(display.bounds().height()),
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

  DesktopDisplayInfo result;
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

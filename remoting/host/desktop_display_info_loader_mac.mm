// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include <Cocoa/Cocoa.h>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

namespace remoting {

namespace {

constexpr int kDefaultScreenDpi = 96;

class DesktopDisplayInfoLoaderMac : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderMac() = default;
  ~DesktopDisplayInfoLoaderMac() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderMac::GetCurrentDisplayInfo() {
  DesktopDisplayInfo result;

  NSArray* screens = NSScreen.screens;
  DCHECK(screens);

  // Each display origin is the bottom left corner, so we need to record the
  // height of the main display (#0) so that we can adjust the origin of
  // the secondary displays.
  int main_display_height = 0;

  for (NSUInteger i = 0; i < screens.count; ++i) {
    NSScreen* screen = screens[i];
    NSDictionary* device = screen.deviceDescription;
    CGDirectDisplayID id =
        static_cast<CGDirectDisplayID>([device[@"NSScreenNumber"] intValue]);

    NSRect bounds = screen.frame;
    int32_t x = bounds.origin.x;
    int32_t y = bounds.origin.y;
    uint32_t width = bounds.size.width;
    uint32_t height = bounds.size.height;
    uint32_t dpi = kDefaultScreenDpi * screen.backingScaleFactor;
    bool is_default = false;
    std::string display_name = base::SysNSStringToUTF8(screen.localizedName);

    if (i == 0) {
      DCHECK(x == 0);
      DCHECK(y == 0);
      is_default = true;
      main_display_height = height;
    }

    // Convert origin from lower left to upper left (based on main display).
    y = main_display_height - y - height;

    result.AddDisplay({id, x, y, width, height, dpi,
                       /*bpp=*/ 24, is_default, display_name});
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderMac>();
}

}  // namespace remoting

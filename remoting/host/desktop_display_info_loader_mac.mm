// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

#include <Cocoa/Cocoa.h>

#include "base/check.h"

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
    int x = bounds.origin.x;
    int y = bounds.origin.y;
    int height = bounds.size.height;

    bool is_default = false;
    if (i == 0) {
      DCHECK(x == 0);
      DCHECK(y == 0);
      is_default = true;
      main_display_height = height;
    }

    DisplayGeometry info;
    info.id = id;
    info.x = x;
    // Convert origin from lower left to upper left (based on main display).
    info.y = main_display_height - y - height;
    info.width = bounds.size.width;
    info.height = height;
    info.dpi = (int)(kDefaultScreenDpi * screen.backingScaleFactor);
    info.bpp = 24;
    info.is_default = is_default;
    result.AddDisplay(info);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderMac>();
}

}  // namespace remoting

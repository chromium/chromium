// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info.h"

#include <Cocoa/Cocoa.h>

#include "base/check.h"

namespace remoting {

constexpr int kDefaultScreenDpi = 96;

void DesktopDisplayInfo::LoadCurrentDisplayInfo() {
  displays_.clear();

  NSArray* screens = [NSScreen screens];
  DCHECK(screens);

  // Each display origin is the bottom left corner, so we need to record the
  // height of the main display (#0) so that we can adjust the origin of
  // the secondary displays.
  int main_display_height = 0;

  for (NSUInteger i = 0; i < [screens count]; ++i) {
    std::unique_ptr<DisplayGeometry> info(new DisplayGeometry());

    NSScreen* screen = screens[i];
    NSDictionary* device = [screen deviceDescription];
    CGDirectDisplayID id =
        static_cast<CGDirectDisplayID>([device[@"NSScreenNumber"] intValue]);
    info->id = id;

    float dsf = 1.0f;
    if ([screen respondsToSelector:@selector(backingScaleFactor)])
      dsf = [screen backingScaleFactor];

    NSRect bounds = [screen frame];
    int x = bounds.origin.x;
    int y = bounds.origin.y;
    int height = bounds.size.height;

    if (i == 0) {
      DCHECK(x == 0);
      DCHECK(y == 0);
      info->is_default = true;
      main_display_height = height;
    } else {
      info->is_default = false;
    }

    info->x = x;
    // Convert origin from lower left to upper left (based on main display).
    info->y = main_display_height - y - height;
    info->width = bounds.size.width;
    info->height = height;
    info->dpi = (int)(kDefaultScreenDpi * dsf);
    info->bpp = 24;

    displays_.push_back(std::move(info));
  }
}

}  // namespace remoting

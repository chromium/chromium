// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_LAYOUT_UTIL_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_LAYOUT_UTIL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "remoting/host/desktop_geometry.h"

namespace remoting {

// Struct that allows passing in the display layout with a context, usually a
// platform-specific representation of the display.
struct DesktopLayoutWithContext {
  DesktopLayout layout;
  raw_ptr<void> context;
};

struct DisplayLayoutDiff {
  DisplayLayoutDiff();
  DisplayLayoutDiff(const DisplayLayoutDiff&);
  DisplayLayoutDiff(DisplayLayoutDiff&&);
  ~DisplayLayoutDiff();

  DesktopLayoutSet new_displays;
  std::vector<DesktopLayoutWithContext> updated_displays;
  std::vector<DesktopLayoutWithContext> removed_displays;
};

// Calculates the difference between the current display layout and the new
// display layout. Displays are matched using the screen ID.
DisplayLayoutDiff CalculateDisplayLayoutDiff(
    const std::vector<DesktopLayoutWithContext>& current_displays,
    const DesktopLayoutSet& new_layout);

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_LAYOUT_UTIL_H_

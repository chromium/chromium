// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_layout_util.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/ranges/algorithm.h"

namespace remoting {

DisplayLayoutDiff::DisplayLayoutDiff() = default;
DisplayLayoutDiff::DisplayLayoutDiff(const DisplayLayoutDiff&) = default;
DisplayLayoutDiff::DisplayLayoutDiff(DisplayLayoutDiff&&) = default;
DisplayLayoutDiff::~DisplayLayoutDiff() = default;

DisplayLayoutDiff CalculateDisplayLayoutDiff(
    const std::vector<VideoTrackLayoutWithContext>& current_displays,
    const protocol::VideoLayout& new_layout) {
  DisplayLayoutDiff diff;

  // A list where the index is the index of |current_displays| and the value
  // denotes whether the display is found in the new layout. Used to detect
  // deletion of displays.
  std::vector<bool> current_display_found(current_displays.size(), false);

  for (const protocol::VideoTrackLayout& track_layout :
       new_layout.video_track()) {
    if (!track_layout.has_screen_id()) {
      diff.new_displays.push_back(track_layout);
      continue;
    }
    auto current_display_it = base::ranges::find(
        current_displays, track_layout.screen_id(),
        [](const auto& display) { return display.layout.screen_id(); });
    if (current_display_it == current_displays.end()) {
      LOG(ERROR) << "Ignoring unknown screen_id " << track_layout.screen_id();
      continue;
    }
    current_display_found[current_display_it - current_displays.begin()] = true;
    if (track_layout.position_x() != current_display_it->layout.position_x() ||
        track_layout.position_y() != current_display_it->layout.position_y() ||
        track_layout.width() != current_display_it->layout.width() ||
        track_layout.height() != current_display_it->layout.height() ||
        track_layout.x_dpi() != current_display_it->layout.x_dpi() ||
        track_layout.y_dpi() != current_display_it->layout.y_dpi()) {
      VLOG(1) << "Video layout for screen_id " << track_layout.screen_id()
              << " has been changed.";
      diff.updated_displays.push_back(
          {.layout = track_layout, .context = current_display_it->context});
    } else {
      VLOG(1) << "Video layout for screen_id " << track_layout.screen_id()
              << " has not been changed.";
    }
  }

  for (size_t i = 0u; i < current_display_found.size(); i++) {
    if (!current_display_found[i]) {
      diff.removed_displays.push_back(current_displays[i]);
    }
  }
  return diff;
}

}  // namespace remoting

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_CHANGE_MONITOR_H_
#define CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_CHANGE_MONITOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "ui/display/display_observer.h"

namespace content {

// Monitors system screen information and runs a callback on changes.
class ScreenChangeMonitor : public display::DisplayObserver {
 public:
  // The callback is run on any screen changes; the bool parameter is true iff
  // the plurality of connected screens changed (e.g. 1 screen <-> 2 screens).
  explicit ScreenChangeMonitor(base::RepeatingCallback<void(bool)> callback);
  ~ScreenChangeMonitor() override;

  ScreenChangeMonitor(const ScreenChangeMonitor&) = delete;
  ScreenChangeMonitor& operator=(const ScreenChangeMonitor&) = delete;

 private:
  // Run the callback if a meaningful display change took place.
  void OnScreensChange();

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  display::ScopedOptionalDisplayObserver display_observer_{this};

  // The callback to run on screen change events.
  base::RepeatingCallback<void(bool)> callback_;

  // The most recent display information, cached to detect meaningful changes.
  std::vector<display::Display> cached_displays_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_CHANGE_MONITOR_H_

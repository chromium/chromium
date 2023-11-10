// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
#define REMOTING_HOST_RESIZING_HOST_OBSERVER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/base/screen_resolution.h"

namespace base {
class TickClock;
}

namespace remoting {

class DesktopDisplayInfo;
class DesktopDisplayInfoMonitor;
class DesktopResizer;

// TODO(alexeypa): Rename this class to reflect that it is not
// HostStatusObserver any more.

// Uses the specified DesktopResizer to match host desktop size to the client
// view size as closely as is possible. When the connection closes, restores
// the original desktop size if restore is true.
class ResizingHostObserver : public ScreenControls {
 public:
  explicit ResizingHostObserver(std::unique_ptr<DesktopResizer> desktop_resizer,
                                bool restore);

  ResizingHostObserver(const ResizingHostObserver&) = delete;
  ResizingHostObserver& operator=(const ResizingHostObserver&) = delete;

  ~ResizingHostObserver() override;

  void RegisterForDisplayChanges(DesktopDisplayInfoMonitor& monitor);

  // ScreenControls interface.
  void SetScreenResolution(const ScreenResolution& resolution,
                           std::optional<webrtc::ScreenId> screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& video_layout) override;

  // Allows tests to provide display-info updates.
  void SetDisplayInfoForTesting(const DesktopDisplayInfo& display_info);

  // Provide a replacement for base::TimeTicks::Now so that this class can be
  // unit-tested in a timely manner. This function will be called exactly
  // once for each call to SetScreenResolution.
  void SetClockForTesting(const base::TickClock* clock);

 private:
  // Restores the given monitor's original resolution, and removes it from the
  // stored list.
  void RestoreScreenResolution(webrtc::ScreenId screen_id);

  // Restores every monitor's resolution.
  void RestoreAllScreenResolutions();

  // Stores the original resolution for the monitor |screen_id|. This does not
  // overwrite any previously stored value, so the recorded resolutions are
  // always the first ones for each monitor.
  void RecordOriginalResolution(ScreenResolution resolution,
                                webrtc::ScreenId screen_id);

  void OnDisplayInfoChanged(const DesktopDisplayInfo& display_info);

  std::unique_ptr<DesktopResizer> desktop_resizer_;

  // List of per-monitor original resolutions to be restored.
  std::map<webrtc::ScreenId, ScreenResolution> original_resolutions_;

  // List of current monitor IDs, populated from OnDisplayInfoChanged().
  // Requests to change a resolution should be dropped if there is no
  // monitor matching the requested ID. Requests without any ID should be
  // applied to the single monitor if there is only one.
  std::set<webrtc::ScreenId> current_monitor_ids_;

  // Whether monitors should be restored when this object is destroyed.
  bool restore_;

  // If SetScreenResolution() is called without any screen_id, and the
  // video-layout is still empty, the requested resolution is stored here so it
  // can be applied when the next video-layout is received. This is needed
  // because, on Windows, DesktopSessionAgent::Start() calls
  // SetScreenResolution() immediately after creating this object.
  ScreenResolution pending_resolution_request_;

  // State to manage rate-limiting of desktop resizes.
  base::OneShotTimer deferred_resize_timer_;
  base::TimeTicks previous_resize_time_;
  raw_ptr<const base::TickClock> clock_;

  base::WeakPtrFactory<ResizingHostObserver> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_RESIZING_HOST_OBSERVER_H_

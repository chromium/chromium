// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"

#include <stdint.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/proto/control.pb.h"

namespace remoting {
namespace {

// Minimum amount of time to wait between desktop resizes. Note that this
// constant is duplicated by the ResizingHostObserverTest.RateLimited
// unit-test and must be kept in sync.
const int kMinimumResizeIntervalMs = 1000;

class CandidateResolution {
 public:
  CandidateResolution(const ScreenResolution& candidate,
                      const ScreenResolution& preferred)
      : resolution_(candidate) {
    // Protect against division by zero.
    CHECK(!candidate.IsEmpty());
    DCHECK(!preferred.IsEmpty());

    // The client scale factor is the smaller of the candidate:preferred ratios
    // for width and height.
    if ((candidate.dimensions().width() > preferred.dimensions().width()) ||
        (candidate.dimensions().height() > preferred.dimensions().height())) {
      const float width_ratio =
          static_cast<float>(preferred.dimensions().width()) /
          candidate.dimensions().width();
      const float height_ratio =
          static_cast<float>(preferred.dimensions().height()) /
          candidate.dimensions().height();
      client_scale_factor_ = std::min(width_ratio, height_ratio);
    } else {
      // Since clients do not scale up, 1.0 is the maximum.
      client_scale_factor_ = 1.0;
    }

    // The aspect ratio "goodness" is defined as being the ratio of the smaller
    // of the two aspect ratios (candidate and preferred) to the larger. The
    // best aspect ratio is the one that most closely matches the preferred
    // aspect ratio (in other words, the ideal aspect ratio "goodness" is 1.0).
    // By keeping the values < 1.0, it allows ratios that differ in opposite
    // directions to be compared numerically.
    float candidate_aspect_ratio =
        static_cast<float>(candidate.dimensions().width()) /
        candidate.dimensions().height();
    float preferred_aspect_ratio =
        static_cast<float>(preferred.dimensions().width()) /
        preferred.dimensions().height();
    if (candidate_aspect_ratio > preferred_aspect_ratio) {
      aspect_ratio_goodness_ = preferred_aspect_ratio / candidate_aspect_ratio;
    } else {
      aspect_ratio_goodness_ = candidate_aspect_ratio / preferred_aspect_ratio;
    }
  }

  const ScreenResolution& resolution() const { return resolution_; }
  float client_scale_factor() const { return client_scale_factor_; }
  float aspect_ratio_goodness() const { return aspect_ratio_goodness_; }
  int64_t area() const {
    return static_cast<int64_t>(resolution_.dimensions().width()) *
           resolution_.dimensions().height();
  }

  // TODO(jamiewalch): Also compare the DPI: http://crbug.com/172405
  bool IsBetterThan(const CandidateResolution& other) const {
    // If either resolution would require down-scaling, prefer the one that
    // down-scales the least (since the client scale factor is at most 1.0,
    // this does not differentiate between resolutions that don't require
    // down-scaling).
    if (client_scale_factor() < other.client_scale_factor()) {
      return false;
    } else if (client_scale_factor() > other.client_scale_factor()) {
      return true;
    }

    // If the scale factors are the same, pick the resolution with the largest
    // area.
    if (area() < other.area()) {
      return false;
    } else if (area() > other.area()) {
      return true;
    }

    // If the areas are equal, pick the resolution with the "best" aspect ratio.
    if (aspect_ratio_goodness() < other.aspect_ratio_goodness()) {
      return false;
    } else if (aspect_ratio_goodness() > other.aspect_ratio_goodness()) {
      return true;
    }

    // All else being equal (for example, comparing 640x480 to 480x640 w.r.t.
    // 640x640), just pick the widest, since desktop UIs are typically designed
    // for landscape aspect ratios.
    return resolution().dimensions().width() >
           other.resolution().dimensions().width();
  }

 private:
  float client_scale_factor_;
  float aspect_ratio_goodness_;
  ScreenResolution resolution_;
};

}  // namespace

ResizingHostObserver::ResizingHostObserver(
    std::unique_ptr<DesktopResizer> desktop_resizer,
    bool restore)
    : desktop_resizer_(std::move(desktop_resizer)),
      restore_(restore),
      clock_(base::DefaultTickClock::GetInstance()) {}

ResizingHostObserver::~ResizingHostObserver() {
  if (restore_) {
    RestoreAllScreenResolutions();
  }
}

void ResizingHostObserver::RegisterForDisplayChanges(
    DesktopDisplayInfoMonitor& monitor) {
  monitor.AddCallback(base::BindRepeating(
      &ResizingHostObserver::OnDisplayInfoChanged, weak_factory_.GetWeakPtr()));
}

void ResizingHostObserver::SetScreenResolution(
    const ScreenResolution& resolution,
    std::optional<webrtc::ScreenId> opt_screen_id) {
  // Get the current time. This function is called exactly once for each call
  // to SetScreenResolution to simplify the implementation of unit-tests.
  base::TimeTicks now = clock_->NowTicks();

  webrtc::ScreenId screen_id;
  if (opt_screen_id) {
    screen_id = opt_screen_id.value();
  } else {
    // If SetScreenResolution() was called without any ID, the ID of the
    // single monitor should be used. If there are no monitors yet, the request
    // is remembered, to be applied when the display-info is updated.
    if (current_monitor_ids_.empty()) {
      pending_resolution_request_ = resolution;
      return;
    }
    if (current_monitor_ids_.size() == 1) {
      screen_id = *current_monitor_ids_.begin();
    } else {
      // Drop the request if there is more than 1 monitor.
      HOST_LOG << "Ignoring ambiguous resize request.";
      return;
    }
  }

  // Drop any request for an invalid screen ID.
  if (!base::Contains(current_monitor_ids_, screen_id)) {
    HOST_LOG << "Ignoring resize request for invalid monitor ID " << screen_id
             << ".";
    return;
  }

  if (resolution.IsEmpty()) {
    RestoreScreenResolution(screen_id);
    return;
  }

  // Resizing the desktop too often is probably not a good idea, so apply a
  // simple rate-limiting scheme.
  // TODO(crbug.com/40225767): Rate-limiting should only be applied to requests
  // for the same monitor.
  base::TimeTicks next_allowed_resize =
      previous_resize_time_ + base::Milliseconds(kMinimumResizeIntervalMs);

  if (now < next_allowed_resize) {
    deferred_resize_timer_.Start(
        FROM_HERE, next_allowed_resize - now,
        base::BindOnce(&ResizingHostObserver::SetScreenResolution,
                       weak_factory_.GetWeakPtr(), resolution, opt_screen_id));
    return;
  }

  // If the implementation returns any resolutions, pick the best one according
  // to the algorithm described in CandidateResolution::IsBetterThan.
  std::list<ScreenResolution> resolutions =
      desktop_resizer_->GetSupportedResolutions(resolution, screen_id);
  if (resolutions.empty()) {
    HOST_LOG << "No valid resolutions found for monitor ID " << screen_id
             << ".";
    return;
  } else {
    HOST_LOG << "Found host resolutions for monitor ID " << screen_id << ":";
    for (const auto& host_resolution : resolutions) {
      HOST_LOG << "  " << host_resolution.dimensions().width() << "x"
               << host_resolution.dimensions().height();
    }
  }
  HOST_LOG << "Choosing best candidate for client resolution: "
           << resolution.dimensions().width() << "x"
           << resolution.dimensions().height() << " [" << resolution.dpi().x()
           << ", " << resolution.dpi().y() << "]";
  CandidateResolution best_candidate(resolutions.front(), resolution);
  for (std::list<ScreenResolution>::const_iterator i = ++resolutions.begin();
       i != resolutions.end(); ++i) {
    CandidateResolution candidate(*i, resolution);
    if (candidate.IsBetterThan(best_candidate)) {
      best_candidate = candidate;
    }
  }
  ScreenResolution current_resolution =
      desktop_resizer_->GetCurrentResolution(screen_id);
  ScreenResolution best_resolution = best_candidate.resolution();

  if (!best_resolution.Equals(current_resolution)) {
    RecordOriginalResolution(current_resolution, screen_id);
    HOST_LOG << "Resizing monitor ID " << screen_id << " to "
             << best_resolution.dimensions().width() << "x"
             << best_resolution.dimensions().height() << " ["
             << best_resolution.dpi().x() << ", " << best_resolution.dpi().y()
             << "].";
    desktop_resizer_->SetResolution(best_resolution, screen_id);
  } else {
    HOST_LOG << "Not resizing monitor ID " << screen_id
             << "; desktop dimensions already "
             << best_resolution.dimensions().width() << "x"
             << best_resolution.dimensions().height() << " ["
             << best_resolution.dpi().x() << ", " << best_resolution.dpi().y()
             << "].";
  }

  // Update the time of last resize to allow it to be rate-limited.
  previous_resize_time_ = now;
}

void ResizingHostObserver::SetVideoLayout(
    const protocol::VideoLayout& video_layout) {
  desktop_resizer_->SetVideoLayout(video_layout);
}

void ResizingHostObserver::SetDisplayInfoForTesting(
    const DesktopDisplayInfo& display_info) {
  OnDisplayInfoChanged(display_info);
}

void ResizingHostObserver::SetClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void ResizingHostObserver::RestoreScreenResolution(webrtc::ScreenId screen_id) {
  auto iter = original_resolutions_.find(screen_id);
  if (iter != original_resolutions_.end()) {
    auto [_, original_resolution] = *iter;
    HOST_LOG << "Restoring monitor ID " << screen_id << " to "
             << original_resolution.dimensions().width() << "x"
             << original_resolution.dimensions().height() << ".";
    desktop_resizer_->RestoreResolution(original_resolution, screen_id);
    original_resolutions_.erase(iter);
  } else {
    HOST_LOG << "No original resolution found for monitor ID " << screen_id
             << ".";
  }
}

void ResizingHostObserver::RestoreAllScreenResolutions() {
  while (!original_resolutions_.empty()) {
    auto [screen_id, _] = *original_resolutions_.begin();
    RestoreScreenResolution(screen_id);
  }
}

void ResizingHostObserver::RecordOriginalResolution(
    ScreenResolution resolution,
    webrtc::ScreenId screen_id) {
  if (!base::Contains(original_resolutions_, screen_id)) {
    original_resolutions_[screen_id] = resolution;
  }
}

void ResizingHostObserver::OnDisplayInfoChanged(
    const DesktopDisplayInfo& display_info) {
  current_monitor_ids_.clear();
  for (int i = 0; i < display_info.NumDisplays(); i++) {
    current_monitor_ids_.insert(display_info.GetDisplayInfo(i)->id);
  }

  // If there was a pending resolution request for an unspecifed monitor, apply
  // it now.
  if (!pending_resolution_request_.IsEmpty()) {
    SetScreenResolution(pending_resolution_request_, std::nullopt);
    pending_resolution_request_ = {};
  }
}

}  // namespace remoting

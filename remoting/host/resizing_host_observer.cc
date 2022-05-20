// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"

#include <stdint.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_resizer.h"

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
  if (restore_)
    RestoreScreenResolution();
}

void ResizingHostObserver::SetScreenResolution(
    const ScreenResolution& resolution,
    absl::optional<webrtc::ScreenId> screen_id) {
  // Get the current time. This function is called exactly once for each call
  // to SetScreenResolution to simplify the implementation of unit-tests.
  base::TimeTicks now = clock_->NowTicks();

  if (resolution.IsEmpty()) {
    RestoreScreenResolution();
    return;
  }

  // Resizing the desktop too often is probably not a good idea, so apply a
  // simple rate-limiting scheme.
  base::TimeTicks next_allowed_resize =
      previous_resize_time_ + base::Milliseconds(kMinimumResizeIntervalMs);

  if (now < next_allowed_resize) {
    deferred_resize_timer_.Start(
        FROM_HERE, next_allowed_resize - now,
        base::BindOnce(&ResizingHostObserver::SetScreenResolution,
                       weak_factory_.GetWeakPtr(), resolution, screen_id));
    return;
  }

  // If the implementation returns any resolutions, pick the best one according
  // to the algorithm described in CandidateResolution::IsBetterThan.
  std::list<ScreenResolution> resolutions =
      desktop_resizer_->GetSupportedResolutions(resolution, absl::nullopt);
  if (resolutions.empty()) {
    LOG(INFO) << "No valid resolutions found.";
    return;
  } else {
    LOG(INFO) << "Found host resolutions:";
    for (const auto& host_resolution : resolutions) {
      LOG(INFO) << "  " << host_resolution.dimensions().width() << "x"
                << host_resolution.dimensions().height();
    }
  }
  CandidateResolution best_candidate(resolutions.front(), resolution);
  for (std::list<ScreenResolution>::const_iterator i = ++resolutions.begin();
       i != resolutions.end(); ++i) {
    CandidateResolution candidate(*i, resolution);
    if (candidate.IsBetterThan(best_candidate)) {
      best_candidate = candidate;
    }
  }
  ScreenResolution current_resolution =
      desktop_resizer_->GetCurrentResolution(absl::nullopt);

  if (!best_candidate.resolution().Equals(current_resolution)) {
    if (original_resolution_.IsEmpty())
      original_resolution_ = current_resolution;
    LOG(INFO) << "Resizing to "
              << best_candidate.resolution().dimensions().width() << "x"
              << best_candidate.resolution().dimensions().height();
    desktop_resizer_->SetResolution(best_candidate.resolution(), absl::nullopt);
  } else {
    LOG(INFO) << "Not resizing; desktop dimensions already "
              << best_candidate.resolution().dimensions().width() << "x"
              << best_candidate.resolution().dimensions().height();
  }

  // Update the time of last resize to allow it to be rate-limited.
  previous_resize_time_ = now;
}

void ResizingHostObserver::SetClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void ResizingHostObserver::RestoreScreenResolution() {
  if (!original_resolution_.IsEmpty()) {
    desktop_resizer_->RestoreResolution(original_resolution_, absl::nullopt);
    original_resolution_ = ScreenResolution();
  }
}

}  // namespace remoting

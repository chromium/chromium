// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_H_

#include <cstddef>
#include <optional>

#import "base/containers/lru_cache.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "components/contextual_cueing/contextual_cueing_enums.h"
#import "components/contextual_cueing/nudge_cap_tracker.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace contextual_cueing {

// Tracks and enforces impression frequency caps, per-origin caps, navigation
// spacing, and dismissal backoff for contextual cueing on iOS, aligned 1:1 with
// Desktop and Chrome on Android.
class ContextualCueingCapTracker {
 public:
  struct Config {
    Config();
    ~Config();
    Config(const Config&);
    Config& operator=(const Config&);

    // TODO(crbug.com/544695640) For multi-arm experiments these caps below
    // should be controlled by finch params.

    // Global cap: maximum cues shown across all origins within
    // `global_duration` (default: 4 cues per 24 hours).
    size_t global_cap_count = 4;
    base::TimeDelta global_duration = base::Hours(24);

    // Per-origin cap: maximum cues shown per origin within `origin_duration`
    // (default: 1 cue per origin every 1 hour).
    size_t origin_cap_count = 1;
    base::TimeDelta origin_duration = base::Hours(1);

    // Limit on how many recently visited origins should be tracked.
    size_t visited_origins_limit = 20;

    // Minimum committed page navigations required between showing cues.
    size_t min_page_count_between_nudges = 10;

    // Base backoff cooldown applied when a cue is shown / ignored without
    // interaction (default: 60 mins with 1.5x multiplier on subsequent
    // ignores).
    base::TimeDelta min_time_between_nudges = base::Minutes(60);
    double ignore_backoff_multiplier_base = 1.5;

    // Base backoff cooldown applied after user explicitly dismisses a cue
    // (default: 24 hours with 2x multiplier on subsequent dismissals).
    base::TimeDelta base_dismiss_backoff_time = base::Hours(24);
    double dismiss_backoff_multiplier_base = 2.0;

    // Backoff cooldown applied after user accepts (clicks) a cue
    // (default: 30 mins).
    base::TimeDelta click_backoff_time = base::Minutes(30);

    // Whether all frequency capping and cooldown backoff logic is completely
    // disabled (e.g. for testing or debugging).
    bool disable_frequency_capping_and_backoff = false;
  };

  ContextualCueingCapTracker();
  explicit ContextualCueingCapTracker(Config config);
  ~ContextualCueingCapTracker();

  ContextualCueingCapTracker(const ContextualCueingCapTracker&) = delete;
  ContextualCueingCapTracker& operator=(const ContextualCueingCapTracker&) =
      delete;

  // Returns whether a contextual cue can be shown for `url`.
  ContextualCueingDecision CanShowNudge(const GURL& url) const;

  // Notifies the tracker that a cue was presented to the user for `url`.
  void RecordCueShown(const GURL& url);

  // Notifies the tracker that the user dismissed a cue for `url`.
  void RecordCueDismissed(const GURL& url);

  // Notifies the tracker that the user clicked a cue for `url`.
  void RecordCueClicked(const GURL& url);

  // Notifies the tracker of a new page navigation to update page spacing.
  void RecordPageNavigation();

  // Returns the timestamp of the most recent cue shown, if any.
  std::optional<base::TimeTicks> GetMostRecentNudgeTime() const;

 private:
  const Config config_;

  // Global timestamp tracker.
  NudgeCapTracker global_tracker_;

  // Per-origin timestamp trackers.
  base::LRUCache<url::Origin, NudgeCapTracker> origin_trackers_;

  // Remaining quiet page loads before another cue can be shown.
  size_t remaining_quiet_loads_ = 0;

  // Timestamps marking the end of various backoff cooldowns.
  std::optional<base::TimeTicks> shown_backoff_end_time_;
  std::optional<base::TimeTicks> dismiss_backoff_end_time_;
  std::optional<base::TimeTicks> click_backoff_end_time_;

  // Number of consecutive dismissals and cues shown without interaction
  // (for exponential backoff).
  size_t consecutive_dismissals_ = 0;
  size_t consecutive_shown_without_interaction_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_H_

#include <cstddef>
#include <optional>

#import "base/containers/flat_map.h"
#import "base/containers/lru_cache.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "components/contextual_cueing/contextual_cueing_enums.h"
#import "components/contextual_cueing/nudge_cap_tracker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "url/gurl.h"
#import "url/origin.h"

class PrefService;

namespace contextual_cueing {

// The UI surface type to present for a contextual cue.
enum class ContextualCueUiType {
  // High-prominence Message banner UI (initial state for a vertical).
  kMessage = 0,
  // Lower-prominence Omnibox Chip UI (permanent state once switched from
  // Message UI).
  kOmniboxChip = 1,
};

// KeyedService that tracks and enforces impression frequency caps, per-origin
// caps, navigation spacing, dismissal backoff, and per-vertical Message-to-
// Omnibox UI transitions for contextual cueing across an entire Profile on iOS.
class ContextualCueingCapTrackerService : public KeyedService {
 public:
  // Configuration parameters for frequency capping, cooldown backoffs, and UI
  // surface transitions. Default values are managed via Finch feature
  // parameters in `features.mm` and populated by `Config()`.
  struct Config {
    Config();
    ~Config();
    Config(const Config&);
    Config& operator=(const Config&);
    Config(Config&&);
    Config& operator=(Config&&);

    // Global cap: maximum cues shown across all origins within
    // `global_duration`.
    size_t global_cap_count;
    base::TimeDelta global_duration;

    // Per-origin cap: maximum cues shown per origin within `origin_duration`.
    size_t origin_cap_count;
    base::TimeDelta origin_duration;

    // Limit on how many recently visited origins should be tracked.
    size_t visited_origins_limit;

    // Minimum committed page navigations required between showing cues.
    size_t min_page_count_between_nudges;

    // Base backoff cooldown applied when a cue is shown / ignored without
    // interaction, and exponential multiplier on subsequent ignores.
    base::TimeDelta min_time_between_nudges;
    double ignore_backoff_multiplier_base;

    // Base backoff cooldown applied after user explicitly dismisses a cue, and
    // exponential multiplier on subsequent dismissals.
    base::TimeDelta base_dismiss_backoff_time;
    double dismiss_backoff_multiplier_base;

    // Backoff cooldown applied after user accepts (clicks) a cue.
    base::TimeDelta click_backoff_time;

    // Whether all frequency capping and cooldown backoff logic is completely
    // disabled (e.g. for testing or debugging).
    bool disable_frequency_capping_and_backoff;

    // Maximum consecutive Message UI impressions ignored without interaction
    // for a vertical before permanently switching to Omnibox Chip UI.
    size_t max_consecutive_message_ignores;

    // When true, forces all contextual cues to use Message UI only.
    bool force_message_ui_only;

    // When true, forces all contextual cues to use Omnibox Chip UI only.
    bool force_omnibox_chip_ui_only;
  };

  ContextualCueingCapTrackerService();
  explicit ContextualCueingCapTrackerService(PrefService* pref_service);
  explicit ContextualCueingCapTrackerService(Config config);
  ContextualCueingCapTrackerService(PrefService* pref_service, Config config);
  ~ContextualCueingCapTrackerService() override;

  ContextualCueingCapTrackerService(const ContextualCueingCapTrackerService&) =
      delete;
  ContextualCueingCapTrackerService& operator=(
      const ContextualCueingCapTrackerService&) = delete;

  // Returns whether a contextual cue can be shown for `url`.
  ContextualCueingDecision CanShowNudge(const GURL& url) const;

  // Returns the UI surface (`kMessage` or `kOmniboxChip`) that should be
  // presented for `category`. Once a vertical transitions from `kMessage` to
  // `kOmniboxChip`, it permanently stays on `kOmniboxChip`.
  ContextualCueUiType GetCueUiTypeForCategory(
      page_content_annotations::CategoryType category) const;

  // Notifies the tracker that a cue was presented to the user for `url` and
  // optional `category`.
  void RecordCueShown(const GURL& url,
                      std::optional<page_content_annotations::CategoryType>
                          category = std::nullopt);

  // Notifies the tracker that the user dismissed a cue for `url` and optional
  // `category`.
  void RecordCueDismissed(const GURL& url,
                          std::optional<page_content_annotations::CategoryType>
                              category = std::nullopt);

  // Notifies the tracker that the user clicked a cue for `url` and optional
  // `category`.
  void RecordCueClicked(const GURL& url,
                        std::optional<page_content_annotations::CategoryType>
                            category = std::nullopt);

  // Notifies the tracker of a new page navigation to update page spacing.
  void RecordPageNavigation();

  // Returns the timestamp of the most recent cue shown, if any.
  std::optional<base::TimeTicks> GetMostRecentNudgeTime() const;

  const Config& config() const { return config_; }

 private:
  struct VerticalUiState {
    // Whether this vertical has permanently switched to Omnibox Chip UI.
    bool switched_to_omnibox = false;
    // Number of consecutive Messages shown without user interaction.
    size_t consecutive_message_ignores = 0;
    // The UI surface type of the most recently shown cue for this vertical.
    ContextualCueUiType last_shown_ui_type = ContextualCueUiType::kMessage;
  };

  // Loads persisted per-vertical UI transition states from `pref_service_`.
  void LoadVerticalUiStatesFromPrefs();

  // Persists the UI transition state for `category` to `pref_service_`.
  void SaveVerticalUiState(page_content_annotations::CategoryType category);

  raw_ptr<PrefService> pref_service_ = nullptr;
  const Config config_;

  // Global timestamp tracker.
  NudgeCapTracker global_tracker_;

  // Per-origin timestamp trackers.
  base::LRUCache<url::Origin, NudgeCapTracker> origin_trackers_;

  // Per-vertical UI surface transition state.
  base::flat_map<page_content_annotations::CategoryType, VerticalUiState>
      vertical_ui_states_;

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

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_H_

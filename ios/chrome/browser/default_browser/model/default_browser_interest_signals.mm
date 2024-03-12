// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"

#import "base/metrics/user_metrics.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"

namespace default_browser {

void NotifyStartWithWidget() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyStartWithURL() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyCredentialExtensionUsed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyAutofillSuggestionsShown() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyPasswordAutofillSuggestionUsed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

void NotifyPasswordSavedOrUpdated() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

void NotifyRemoteTabsGridViewed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogRemoteTabsUseForCriteriaExperiment();
}

void NotifyBookmarkAddOrEdit() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyBookmarkManagerOpened() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyBookmarkManagerClosed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyURLFromBookmarkOpened() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyOmniboxURLCopyPaste(feature_engagement::Tracker* tracker) {
  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }

  if (HasRecentValidURLPastesAndRecordsCurrentPaste()) {
    tracker->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);
  }
}

void NotifyOmniboxURLCopyPasteAndNavigate(bool is_off_record,
                                          feature_engagement::Tracker* tracker,
                                          SceneState* scene_state) {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogCopyPasteInOmniboxForCriteriaExperiment();

  if (is_off_record) {
    return;
  }

  // Notify contextual promo.
  [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:scene_state]
      logUserPastedInOmnibox];

  base::RecordAction(
      base::UserMetricsAction("Mobile.Omnibox.iOS.PastedValidURL"));

  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }

  // Notify blue dot promo.
  if (HasRecentValidURLPastesAndRecordsCurrentPaste()) {
    tracker->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);
  }
}

void NotifyOmniboxTextCopyPasteAndNavigate() {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogCopyPasteInOmniboxForCriteriaExperiment();
}

}  // namespace default_browser

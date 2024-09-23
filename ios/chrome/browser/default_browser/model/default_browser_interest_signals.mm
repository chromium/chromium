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

void NotifyStartWithWidget(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyStartWithURL(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyCredentialExtensionUsed(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyAutofillSuggestionsShown(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyPasswordAutofillSuggestionUsed(
    feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kStaySafePromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

void NotifyPasswordSavedOrUpdated(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kStaySafePromoConditionsMet);
  }

  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

void NotifyRemoteTabsGridViewed(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogRemoteTabsUseForCriteriaExperiment();
}

void NotifyBookmarkAddOrEdit(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyBookmarkManagerOpened(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyBookmarkManagerClosed(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyURLFromBookmarkOpened(feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  LogBookmarkUseForCriteriaExperiment();
}

void NotifyOmniboxURLCopyPaste(feature_engagement::Tracker* tracker) {
  // TODO(crbug.com/348230111): Decide if we want to track this event for
  // non-modal promo.
}

void NotifyOmniboxURLCopyPasteAndNavigate(bool is_off_record,
                                          feature_engagement::Tracker* tracker,
                                          SceneState* scene_state) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
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
}

void NotifyOmniboxTextCopyPasteAndNavigate(
    feature_engagement::Tracker* tracker) {
  if (IsPromoInterestEventMigrationDone() && tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
  // TODO(crbug.com/322358517): Continue logging to UserDefault until migration
  // is verified on stable. Can be removed M127+.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);

  LogCopyPasteInOmniboxForCriteriaExperiment();
}

void NotifyDefaultBrowserFREPromoShown(feature_engagement::Tracker* tracker) {
  // Continue logging to UserDefaults for non-modal promo.
  // TODO(crbug.com/315329355): Remove once non-modal promos are migrated to
  // FET.
  LogUserInteractionWithFirstRunPromo();

  // No need to do migration for this client because it will be already
  // recording to FET.
  LogFRETimestampMigrationDone();

  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(feature_engagement::events::kIOSDefaultBrowserFREShown);
}
}  // namespace default_browser

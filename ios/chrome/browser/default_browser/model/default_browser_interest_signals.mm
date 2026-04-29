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
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
}

void NotifyStartWithURL(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
}

void NotifyCredentialExtensionUsed(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
  }
}

void NotifyAutofillSuggestionsShown(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
  }
}

void NotifyPasswordAutofillSuggestionUsed(
    feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kStaySafePromoConditionsMet);
  }
}

void NotifyPasswordSavedOrUpdated(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kStaySafePromoConditionsMet);
  }
}

void NotifyRemoteTabsGridViewed(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
}

void NotifyBookmarkAddOrEdit(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
}

void NotifyBookmarkManagerOpened(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
}

void NotifyBookmarkManagerClosed(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
}

void NotifyURLFromBookmarkOpened(feature_engagement::Tracker* tracker) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kAllTabsPromoConditionsMet);
  }
}

void NotifyOmniboxURLCopyPaste(feature_engagement::Tracker* tracker) {
  // TODO(crbug.com/348230111): Decide if we want to track this event for
  // non-modal promo.
}

void NotifyOmniboxURLCopyPasteAndNavigate(bool is_off_record,
                                          feature_engagement::Tracker* tracker,
                                          SceneState* scene_state) {
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }

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
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
}

void NotifyDefaultBrowserFREPromoShown(feature_engagement::Tracker* tracker) {
  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(feature_engagement::events::kIOSDefaultBrowserFREShown);
}
}  // namespace default_browser

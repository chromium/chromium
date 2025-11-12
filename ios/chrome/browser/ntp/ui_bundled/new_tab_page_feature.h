// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

class PrefService;

// Enum to represent arms of feature kFeedSwipeInProductHelp.
enum class FeedSwipeIPHVariation {
  kDisabled,
  kStaticAfterFRE,
  kStaticInSecondRun,
  kAnimated,
};

// Represents the possible onboarding treatments of Lens Overlay.
enum class NTPMIAEntrypointVariation {
  // The default experience.
  kDisabled = 0,
  // The entrypoint is shown in the omnibox as a single button.
  kOmniboxContainedSingleButton = 1,
  // The entrypoint is shown in the omnibox as a button inline with Lens and
  // Voice.
  kOmniboxContainedInline = 2,
  // The entrypoint is shown inside the enlarged fake omnibox.
  kOmniboxContainedEnlargedFakebox = 3,
  // The entrypoint is shown inside the enlarged fake omnibox without incognito
  // shortcut.
  kEnlargedFakeboxNoIncognito = 4,
  // The entrypoint is shown as a quick actions button, with enlarged fake
  // omnibox
  kAIMInQuickAction = 5,
  kMaxValue = kAIMInQuickAction,
};

#pragma mark - Feature declarations

// Feature flag to fix the NTP view hierarchy if it is broken before applying
// constraints.
// TODO(crbug.com/40799579): Remove this when it is fixed.
BASE_DECLARE_FEATURE(kEnableNTPViewHierarchyRepair);

// Flag to modify the feed header through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kFeedHeaderSettings);

// Flag to override feed settings through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kOverrideFeedSettings);

// Feature flag to enable sending discover feedback to an updated target
BASE_DECLARE_FEATURE(kWebFeedFeedbackReroute);

// Feature flag to enable ghost cards on the iPad feeds.
BASE_DECLARE_FEATURE(kEnableiPadFeedGhostCards);

// Feature flag to enable in-product help for swipe action on the Feed.
BASE_DECLARE_FEATURE(kFeedSwipeInProductHelp);

// Feature flag to handle feed eligibility and state in the new Discover
// eligibility service instead of the new tab page mediator.
BASE_DECLARE_FEATURE(kUseFeedEligibilityService);

// iOS counterpart for `chrome::android::kMostVisitedTilesCustomization`;
// enables customizable most visited tiles when enabled.
BASE_DECLARE_FEATURE(kMostVisitedTilesCustomizationIOS);

#pragma mark - Feature parameters

// A parameter to indicate whether Reconstructed Templates is enabled for static
// resource serving.
// TODO(crbug.com/40246814): Remove this.
extern const char kDiscoverFeedSRSReconstructedTemplatesEnabled[];

// A parameter to indicate whether Preload Templates is enabled for static
// resource serving.
// TODO(crbug.com/40246814): Remove this.
extern const char kDiscoverFeedSRSPreloadTemplatesEnabled[];

// A parameter value for the feed's refresh threshold when the feed has already
// been seen by the user.
extern const char kFeedSettingRefreshThresholdInSeconds[];

// A parameter value for the feed's refresh threshold when the feed has not been
// seen by the user.
extern const char kFeedSettingUnseenRefreshThresholdInSeconds[];

// A parameter value for the feed's maximum data cache age.
extern const char kFeedSettingMaximumDataCacheAgeInSeconds[];

// A parameter value for the timeout threshold after clearing browsing data.
extern const char kFeedSettingTimeoutThresholdAfterClearBrowsingData[];

// A parameter value for the feed referrer.
extern const char kFeedSettingDiscoverReferrerParameter[];

// Parameter to indicate which arm of feature kFeedSwipeInProductHelp is
// enabled.
extern const char kFeedSwipeInProductHelpArmParam[];

#pragma mark - Helpers

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

// Whether the sync promo should be shown on top of the feed.
bool IsDiscoverFeedTopSyncPromoEnabled();

// Whether content suggestions are enabled for supervised users.
bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service);

// YES if discover feedback is going to be sent to the updated target.
bool IsWebFeedFeedbackRerouteEnabled();

// Whether ghost cards are enabled on the iPad feeds.
bool IsiPadFeedGhostCardsEnabled();

// Returns the enabled variation of feature kFeedSwipeInProductHelp.
FeedSwipeIPHVariation GetFeedSwipeIPHVariation();

// YES if the feed visibility is handled by the eligibility service instead of
// the new tab page mediator.
bool UseFeedEligibilityService();

// Returns the enabled variation of feature kNTPMIAEntrypoint;
NTPMIAEntrypointVariation GetNTPMIAEntrypointVariation();

// Whether to show only the MIA button in the fakebox.
bool ShowOnlyMIAEntrypointInNTPFakebox();

// Whether the quick actions row should be displayed.
bool ShouldShowQuickActionsRow();

// Whether a MIA variation should increase the size of the fakebox.
bool ShouldEnlargeNTPFakeboxForMIA();

// Whether customized most visited tiles is enabled on Chrome on iOS.
bool IsContentSuggestionsCustomizable();

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_

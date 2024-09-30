// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

#pragma mark - Feature declarations

// Feature flag to enable static resource serving for the Discover feed.
// TODO(crbug.com/40246814): Remove this.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedStaticResourceServing);

// Feature flag to enable discofeed endpoint for the Discover feed.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedDiscoFeedEndpoint);

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

// Feature flag to enable signed out user view demotion.
BASE_DECLARE_FEATURE(kEnableSignedOutViewDemotion);

// Feature flag to enable ghost cards on the iPad feeds.
BASE_DECLARE_FEATURE(kEnableiPadFeedGhostCards);

// Feature flag to enable account-switching UI when tapping the NTP identity
// disc.
BASE_DECLARE_FEATURE(kIdentityDiscAccountMenu);

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

#pragma mark - Helpers

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

// Whether the sync promo should be shown on top of the feed.
bool IsDiscoverFeedTopSyncPromoEnabled();

// Whether content suggestions are enabled for supervised users.
bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service);

// YES if discover feedback is going to be sent to the updated target.
bool IsWebFeedFeedbackRerouteEnabled();

// YES if the signed out user view demotion is enabled.
bool IsSignedOutViewDemotionEnabled();

// Whether ghost cards are enabled on the iPad feeds.
bool IsiPadFeedGhostCardsEnabled();

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_

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

// Enum to represent arms of feature kNewTabPageUICleanup.
enum class NTPUICleanupVariation {
  kDisabled,
  kTightPadding,
  kMediumPadding,
  kPreferredPadding,
  kFakeboxBackgroundAndShadow,
};

// Defines the arms for the AIM Refactor Experiment.
enum class AimButtonRefactorArm {
  kDisabled = 0,
  // Present AIM button in the Quick Actions row alongside one merchandising
  // chips.
  kOneMerchandisingChip = 1,
  // Present AIM button in the Quick Actions row alongside two merchandising
  // chips.
  kTwoMerchandisingChips = 2,
  // Present the AIM button as a standalone module beside the Most Visited
  // Tiles. Remove the Quick Actions row from the NTP.
  kAimAsModule = 3,
  // Present the AIM button as a Most Visited Tile. Remove the Quick Actions row
  // from the NTP.
  kAimAsMvt = 4,
  // Remove the AIM button and the Quick Actions row from the NTP.
  kNoChips = 5,
};

#pragma mark - Feature declarations

// Feature flag to change the location of the AIM button on the NTP.
BASE_DECLARE_FEATURE(kAimButtonRefactor);

// Flag to modify the feed header through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kFeedHeaderSettings);

// Flag to override feed settings through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kOverrideFeedSettings);

// Feature flag to enable transform-based animations for the NTP header.
BASE_DECLARE_FEATURE(kNTPHeaderUseTransformsForAnimations);

// Checks if transform-based animations are enabled for the NTP header.
bool IsNTPHeaderTransformsForAnimationsEnabled();

// Feature flag to enable in-product help for swipe action on the Feed.
BASE_DECLARE_FEATURE(kFeedSwipeInProductHelp);

// Feature flag to handle feed eligibility and state in the new Discover
// eligibility service instead of the new tab page mediator.
BASE_DECLARE_FEATURE(kUseFeedEligibilityService);

// Feature flag to make the height of the NTP Logo and Doodle consistent.
BASE_DECLARE_FEATURE(kConsistentLogoDoodleHeight);

// Feature flag to enable the New Tab Page UI cleanup. The refresh includes
// padding and styling updates.
BASE_DECLARE_FEATURE(kNewTabPageUICleanup);

// Feature flag to place the Most Visited Tiles in the bottom sheet.
BASE_DECLARE_FEATURE(kMVTInBottomSheet);

// Checks if the Most Visited Tiles should be placed in the bottom sheet.
bool IsMVTInBottomSheetEnabled();

#pragma mark - Feature parameters

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

// Parameter to indicate which arm of the feature kNewTabPageUICleanup is
// enabled.
extern const char kNewTabPageUICleanupArmParam[];

// Parameter to indicate which arm of the feature kAimButtonRefactor is enabled.
extern const char kAimButtonRefactorArmParam[];

#pragma mark - Helpers

// Whether the sync promo should be shown on top of the feed.
bool IsDiscoverFeedTopSyncPromoEnabled();

// Whether content suggestions are enabled for supervised users.
bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service);

// Returns the enabled variation of feature kFeedSwipeInProductHelp.
FeedSwipeIPHVariation GetFeedSwipeIPHVariation();

// YES if the feed visibility is handled by the eligibility service instead of
// the new tab page mediator.
bool UseFeedEligibilityService();

// Whether the AIM button is allowed in NTP.
bool IsAimEnabledInNtp();

// Whether the NTP Logo and Doodle should have a consistent height.
bool IsConsistentLogoDoodleHeightEnabled();

// Feature flag to enable the New Tab Page Redesign.
BASE_DECLARE_FEATURE(kNewTabPageRedesign);

// Whether the New Tab Page Redesign is enabled.
bool IsNTPRedesignEnabled();

// Whether the full New Tab Page UI cleanup is enabled. This cleanup includes
// all color, sizing, and padding updates.
bool IsNewTabPageUICleanupEnabled();

// Whether only the fakebox background color and shadow updates are enabled.
bool IsNewTabPageUICleanupFakeboxOnlyEnabled();

// Returns the enabled variation of feature kNewTabPageUICleanup.
NTPUICleanupVariation GetNewTabPageUICleanupVariation();

// Returns the active arm for the AimButtonRefactor feature.
AimButtonRefactorArm GetAimButtonRefactorArm();

// Returns whether the AimButtonRefactor feature is enabled.
bool IsAimButtonRefactorEnabled();

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_FEATURE_H_

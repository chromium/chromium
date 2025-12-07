// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_METRICS_CONSTANTS_H_

#pragma mark - Actions

extern const char kShowBookmarksAction[];
extern const char kShowReadingListAction[];
extern const char kShowRecentTabsAction[];
extern const char kShowHistoryAction[];
extern const char kShowWhatsNewAction[];
extern const char kShowMostVisitedAction[];
extern const char kMostVisitedAction[];
extern const char kMostVisitedUrlBlacklistedAction[];
extern const char kShowReturnToRecentTabTileAction[];
extern const char kOpenMostRecentTabAction[];
extern const char kShopCardWithPriceTrackingImpression[];
extern const char kShopCardWithReviewsImpression[];
extern const char kShopCardWithPriceTrackingOpen[];
extern const char kShopCardWithReviewsOpen[];
extern const char kTabResumptionWithPriceDropOpenTab[];
extern const char kTabResumptionWithPriceTrackingOpenTab[];
extern const char kTabResumptionOpenTab[];
extern const char kTabResumptionWithPriceDropImpression[];
extern const char kTabResumptionWithPriceTrackingImpression[];
extern const char kTabResumptionImpression[];
extern const char kAppBundlePromoImpression[];
extern const char kContentNotificationSnackbarAction[];

#pragma mark - Enums

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class IOSSafetyCheckHiddenReason {
  kManuallyDisabled = 0,
  kNoIssues = 1,
  kMaxValue = kNoIssues,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSafetyCheckHiddenReason)

#pragma mark - Histograms

extern const char kTrendingQueriesHistogram[];
extern const char kMagicStackModuleEngagementHistogram[];
extern const char kMagicStackModuleEngagementMostVisitedIndexHistogram[];
extern const char kMagicStackModuleEngagementShortcutsIndexHistogram[];
extern const char kMagicStackModuleEngagementSetUpListIndexHistogram[];
extern const char kMagicStackModuleEngagementTabResumptionIndexHistogram[];
extern const char kMagicStackModuleEngagementSafetyCheckIndexHistogram[];
extern const char kMagicStackModuleEngagementPriceTrackingPromoIndexHistogram[];
extern const char kMagicStackModuleEngagementShopCardIndexHistogram[];
extern const char kMagicStackModuleEngagementSendTabPromoIndexHistogram[];
extern const char kMagicStackModuleEngagementTipsIndexHistogram[];
extern const char kMagicStackModuleEngagementAppBundlePromoIndexHistogram[];
extern const char kMagicStackModuleEngagementDefaultBrowserIndexHistogram[];
extern const char kMagicStackModuleDisabledHistogram[];
extern const char kContentNotificationSnackbarEventHistogram[];
extern const char kIOSSafetyCheckMagicStackHiddenReason[];

// The name of the histogram that records fetch time for the Segmentation
// ranking for Magic Stack.
extern const char kMagicStackStartSegmentationRankingFetchTimeHistogram[];
extern const char kMagicStackNTPSegmentationRankingFetchTimeHistogram[];

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CONTENT_SUGGESTIONS_METRICS_CONSTANTS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_constants.h"

#pragma mark - Actions

const char kShowBookmarksAction[] = "MobileNTPShowBookmarks";
const char kShowReadingListAction[] = "MobileNTPShowReadingList";
const char kShowRecentTabsAction[] = "MobileNTPShowRecentTabs";
const char kShowHistoryAction[] = "MobileNTPShowHistory";
const char kShowWhatsNewAction[] = "MobileNTPShowWhatsNew";
const char kShowMostVisitedAction[] = "MobileNTPShowMostVisited";
const char kMostVisitedAction[] = "MobileNTPMostVisited";
const char kMostVisitedUrlBlacklistedAction[] = "MostVisited_UrlBlacklisted";
const char kShowReturnToRecentTabTileAction[] =
    "IOS.StartSurface.ShowReturnToRecentTabTile";
const char kOpenMostRecentTabAction[] = "IOS.StartSurface.OpenMostRecentTab";
const char kShopCardWithPriceTrackingImpression[] =
    "IOS.MagicStack.ShopCard.PriceTracking.Impression";
const char kShopCardWithReviewsImpression[] =
    "IOS.MagicStack.ShopCard.Reviews.Impression";
const char kShopCardWithPriceTrackingOpen[] =
    "IOS.MagicStack.ShopCard.PriceTracking.Open";
const char kShopCardWithReviewsOpen[] = "IOS.MagicStack.ShopCard.Reviews.Open";

const char kTabResumptionWithPriceDropOpenTab[] =
    "IOS.MagicStack.TabResumption.PriceDrop.OpenTab";
const char kTabResumptionWithPriceTrackingOpenTab[] =
    "IOS.MagicStack.TabResumption.PriceTracking.OpenTab";
const char kTabResumptionOpenTab[] =
    "IOS.MagicStack.TabResumption.Regular.OpenTab";
const char kTabResumptionWithPriceDropImpression[] =
    "IOS.MagicStack.TabResumption.PriceDrop.Impression";
const char kTabResumptionWithPriceTrackingImpression[] =
    "IOS.MagicStack.TabResumption.PriceTracking.Impression";
const char kTabResumptionImpression[] =
    "IOS.MagicStack.TabResumption.Regular.Impression";

const char kAppBundlePromoImpression[] =
    "IOS.MagicStack.AppBundlePromo.Impression";

const char kContentNotificationSnackbarAction[] =
    "ContentNotifications.Promo.Snackbar.ActionButtonTapped";

#pragma mark - Histograms

const char kTrendingQueriesHistogram[] = "IOS.TrendingQueries";
const char kMagicStackModuleEngagementHistogram[] =
    "IOS.MagicStack.Module.Click";
const char kMagicStackModuleEngagementMostVisitedIndexHistogram[] =
    "IOS.MagicStack.Module.Click.MostVisited";
const char kMagicStackModuleEngagementShortcutsIndexHistogram[] =
    "IOS.MagicStack.Module.Click.Shortcuts";
const char kMagicStackModuleEngagementSetUpListIndexHistogram[] =
    "IOS.MagicStack.Module.Click.SetUpList";
const char kMagicStackModuleEngagementTabResumptionIndexHistogram[] =
    "IOS.MagicStack.Module.Click.TabResumption";
const char kMagicStackModuleEngagementSafetyCheckIndexHistogram[] =
    "IOS.MagicStack.Module.Click.SafetyCheck";
const char kMagicStackModuleEngagementPriceTrackingPromoIndexHistogram[] =
    "IOS.MagicStack.Module.Click.PriceTrackingPromo";
const char kMagicStackModuleEngagementShopCardIndexHistogram[] =
    "IOS.MagicStack.Module.Click.ShopCard";
const char kMagicStackModuleEngagementSendTabPromoIndexHistogram[] =
    "IOS.MagicStack.Module.Click.SendTabPromo";
const char kMagicStackModuleEngagementTipsIndexHistogram[] =
    "IOS.MagicStack.Module.Click.Tips";
const char kMagicStackModuleEngagementAppBundlePromoIndexHistogram[] =
    "IOS.MagicStack.Module.Click.AppBundlePromo";
const char kMagicStackModuleEngagementDefaultBrowserIndexHistogram[] =
    "IOS.MagicStack.Module.Click.DefaultBrowser";
const char kMagicStackModuleDisabledHistogram[] =
    "IOS.MagicStack.Module.Disabled";
const char kContentNotificationSnackbarEventHistogram[] =
    "ContentNotifications.Promo.Snackbar.Event";
const char kIOSSafetyCheckMagicStackHiddenReason[] =
    "IOS.SafetyCheck.MagicStack.HiddenReason";

const char kMagicStackStartSegmentationRankingFetchTimeHistogram[] =
    "IOS.MagicStack.Start.SegmentationRankingFetchTime";
const char kMagicStackNTPSegmentationRankingFetchTimeHistogram[] =
    "IOS.MagicStack.NTP.SegmentationRankingFetchTime";

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_

namespace prefs {

extern const char kApplicationLocale[];
extern const char kAppStoreRatingPolicyEnabled[];
extern const char kArticlesForYouEnabled[];
extern const char kBrowserStateInfoCache[];
extern const char kBrowserStateLastUsed[];
extern const char kBrowserStatesLastActive[];
extern const char kBrowserStatesNumCreated[];
extern const char kBrowsingDataMigrationHasBeenPossible[];
extern const char kClearBrowsingDataHistoryNoticeShownTimes[];
extern const char kContextualSearchEnabled[];
extern const char kDefaultCharset[];
extern const char kEnableDoNotTrack[];
extern const char kFeaturePushNotificationPermissions[];
extern const char kFirstFollowUIShownCount[];
extern const char kHttpServerProperties[];
extern const char kIncognitoModeAvailability[];
extern const char kIncognitoInterstitialEnabled[];
extern const char kIosBookmarkCachedFolderId[];
extern const char kIosBookmarkCachedTopMostRow[];
extern const char kIosBookmarkFolderDefault[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kIosBookmarkSigninPromoDisplayedCount[];
extern const char kIosShareChromeCount[];
extern const char kIosShareChromeLastShare[];
extern const char kIosDiscoverFeedLastRefreshTime[];
extern const char kIosPreRestoreAccountInfo[];
extern const char kIosPromosManagerActivePromos[];
extern const char kIosPromosManagerImpressions[];
extern const char kIosPromosManagerSingleDisplayActivePromos[];
extern const char kIosPromosManagerSingleDisplayPendingPromos[];
extern const char kIosSettingsPromoAlreadySeen[];
extern const char kIosSettingsSigninPromoDisplayedCount[];
extern const char kIosNtpFeedTopPromoAlreadySeen[];
extern const char kIosNtpFeedTopSigninPromoDisplayedCount[];
extern const char kLinkPreviewEnabled[];
extern const char kNTPContentSuggestionsEnabled[];
extern const char kNTPContentSuggestionsForSupervisedUserEnabled[];
extern const char kNTPFollowingFeedSortType[];
extern const char kDefaultFollowingFeedSortTypeChanged[];
extern const char kOverflowMenuDestinationUsageHistory[];
extern const char kOverflowMenuNewDestinations[];
extern const char kPrintingEnabled[];
extern const char kSearchSuggestEnabled[];
extern const char kTrackPricesOnTabsEnabled[];

extern const char kNetworkPredictionSetting[];

extern const char kNtpShownBookmarksFolder[];
extern const char kShowMemoryDebuggingTools[];

extern const char kSigninLastAccounts[];
extern const char kSigninLastAccountsMigrated[];
extern const char kSigninShouldPromptForSigninAgain[];
extern const char kSigninWebSignDismissalCount[];

extern const char kIosUserZoomMultipliers[];

extern const char kIncognitoAuthenticationSetting[];

extern const char kBrowserSigninPolicy[];
extern const char kAllowChromeDataInBackups[];

extern const char kNewTabPageLocationOverride[];

extern const char kHttpsOnlyModeEnabled[];
extern const char kMixedContentAutoupgradeEnabled[];

extern const char kAutofillBrandingIconAnimationRemainingCountPrefName[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_

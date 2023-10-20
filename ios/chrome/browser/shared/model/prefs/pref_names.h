// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_

namespace prefs {

extern const char kAddressBarSettingsNewBadgeShownCount[];
extern const char kApplicationLocale[];
extern const char kAppStoreRatingPolicyEnabled[];
extern const char kArticlesForYouEnabled[];
extern const char kBottomOmnibox[];
extern const char kBottomOmniboxByDefault[];
extern const char kBrowserLockdownModeEnabled[];
extern const char kBrowserStateInfoCache[];
extern const char kBrowserStateLastUsed[];
extern const char kBrowserStatesLastActive[];
extern const char kBrowserStatesNumCreated[];
extern const char kBrowsingDataMigrationHasBeenPossible[];
extern const char kClearBrowsingDataHistoryNoticeShownTimes[];
extern const char kContextualSearchEnabled[];
extern const char kDefaultCharset[];
extern const char kDetectAddressesAccepted[];
extern const char kDetectAddressesEnabled[];
extern const char kEnableDoNotTrack[];
extern const char kFeaturePushNotificationPermissions[];
extern const char kFirstFollowUIShownCount[];
extern const char kFirstFollowUpdateUIShownCount[];
extern const char kHttpServerProperties[];
extern const char kInactiveTabsTimeThreshold[];
extern const char kIncognitoInterstitialEnabled[];
extern const char kIosCredentialProviderPromoLastActionTaken[];
extern const char kIosCredentialProviderPromoPolicyEnabled[];
extern const char kIosCredentialProviderPromoStopPromo[];
extern const char kIosCredentialProviderPromoSource[];
extern const char kIosCredentialProviderPromoHasRegisteredWithPromoManager[];
extern const char kIosBookmarkCachedFolderId[];
extern const char kIosBookmarkCachedTopMostRow[];
extern const char kIosBookmarkLastUsedFolderReceivingBookmarks[];
extern const char kIosBookmarkLastUsedStorageReceivingBookmarks[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kIosBookmarkSigninPromoDisplayedCount[];
extern const char kIosBringAndroidTabsPromptDisplayed[];
extern const char kIosShareChromeCount[];
extern const char kIosShareChromeLastShare[];
extern const char kIosSyncSegmentsNewTabPageDisplayCount[];
extern const char kIosDiscoverFeedLastRefreshTime[];
extern const char kIosDiscoverFeedLastUnseenRefreshTime[];
extern const char kIosLatestMostVisitedSites[];
extern const char kIosMagicStackSegmentationMVTImpressionsSinceFreshness[];
extern const char
    kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness[];
extern const char
    kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness[];
extern const char
    kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness[];
extern const char
    kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness[];
extern const char kIosParcelTrackingOptInPromptDisplayed[];
extern const char kIosParcelTrackingOptInStatus[];
extern const char kIosPasswordBottomSheetDismissCount[];
extern const char kIosPreRestoreAccountInfo[];
extern const char kIosPromosManagerActivePromos[];
extern const char kIosPromosManagerImpressions[];
extern const char kIosPromosManagerSingleDisplayActivePromos[];
extern const char kIosPromosManagerSingleDisplayPendingPromos[];
extern const char kIosSafetyCheckManagerLastRunTime[];
extern const char kIosSafetyCheckManagerPasswordCheckResult[];
extern const char kIosSafetyCheckManagerUpdateCheckResult[];
extern const char kIosSafetyCheckManagerSafeBrowsingCheckResult[];
extern const char kIosSaveToPhotosDefaultGaiaId[];
extern const char kIosSaveToPhotosSkipAccountPicker[];
extern const char kIosSettingsSafetyCheckLastRunTime[];
extern const char kIosNtpFeedTopPromoAlreadySeen[];
extern const char kIosNtpFeedTopSigninPromoDisplayedCount[];
extern const char kIosReadingListPromoAlreadySeen[];
extern const char kIosReadingListSigninPromoDisplayedCount[];
extern const char kLinkPreviewEnabled[];
extern const char kNTPContentSuggestionsEnabled[];
extern const char kNTPContentSuggestionsForSupervisedUserEnabled[];
extern const char kNTPFollowingFeedSortType[];
extern const char kDefaultFollowingFeedSortTypeChanged[];
extern const char kOSLockdownModeEnabled[];
extern const char kOverflowMenuDestinationUsageHistory[];
extern const char kOverflowMenuDestinationUsageHistoryEnabled[];
extern const char kOverflowMenuNewDestinations[];
extern const char kOverflowMenuDestinationsOrder[];
extern const char kOverflowMenuHiddenDestinations[];
extern const char kOverflowMenuDestinationBadgeData[];
extern const char kOverflowMenuActionsOrder[];
extern const char kPrintingEnabled[];
extern const char kSearchSuggestEnabled[];
extern const char kTabPickupEnabled[];
extern const char kTabPickupLastDisplayedTime[];
extern const char kTabPickupLastDisplayedURL[];
extern const char kTrackPricesOnTabsEnabled[];
extern const char kLensCameraAssistedSearchPolicyAllowed[];
extern const char kWebInspectorEnabled[];

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

extern const char kAutofillBrandingIconAnimationRemainingCount[];
extern const char kAutofillBrandingIconDisplayCount[];
extern const char kAutofillBrandingKeyboardAccessoriesTapped[];

extern const char kPriceNotificationsHasBeenShown[];

extern const char kPasswordSharingFlowHasBeenEntered[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREFS_PREF_NAMES_H_

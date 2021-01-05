// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "Availability.h"

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to control the autofill query cache.
extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control card nickname management.
extern const char kAutofillEnableCardNicknameManagementName[];
extern const char kAutofillEnableCardNicknameManagementDescription[];

// Title and description for the flag to control card nickname upstream.
extern const char kAutofillEnableCardNicknameUpstreamName[];
extern const char kAutofillEnableCardNicknameUpstreamDescription[];

// Title and description for the flag to control enabling Google-issued cards in
// autofill suggestions.
extern const char kAutofillEnableGoogleIssuedCardName[];
extern const char kAutofillEnableGoogleIssuedCardDescription[];

// Title and description for the flag to control offers in downstream.
extern const char kAutofillEnableOffersInDownstreamName[];
extern const char kAutofillEnableOffersInDownstreamDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to control dismissing the Save Card
// Infobar on Navigation.
extern const char kAutofillSaveCardDismissOnNavigationName[];
extern const char kAutofillSaveCardDismissOnNavigationDescription[];

// Title and description for the flag that enables editing on the Messages UI
// for SaveCard Infobars.
extern const char kAutofillSaveCardInfobarEditSupportName[];
extern const char kAutofillSaveCardInfobarEditSupportDescription[];

// Title and description for the flag to restrict extraction of formless forms
// to checkout flows.
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

// Title and description for the flag to enable rich autofill queries on
// Canary/Dev.
extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag that controls whether Autofill's
// logic is using numeric unique renderer IDs instead of string IDs for
// form and field elements.
extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

// Title and description for the flag that controls whether event breadcrumbs
// are captured.
extern const char kLogBreadcrumbsName[];
extern const char kLogBreadcrumbsDescription[];

// Title and description for the flag that controls the sign-in notification
// infobar title.
extern const char kSigninNotificationInfobarUsernameInTitleName[];
extern const char kSigninNotificationInfobarUsernameInTitleDescription[];

// Title and description for the flag that controls synthetic crash reports
// generation for Unexplained Termination Events.
extern const char kSyntheticCrashReportsForUteName[];
extern const char kSyntheticCrashReportsForUteDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Title and description for the flag to control if change password url should
// be obtained by affiliation service.
extern const char kChangePasswordAffiliationInfoName[];
extern const char kChangePasswordAffiliationInfoDescription[];

// Title and description for the flag that controls whether Collections are
// presented using the new iOS13 Card style or the custom legacy one.
extern const char kCollectionsCardPresentationStyleName[];
extern const char kCollectionsCardPresentationStyleDescription[];

#if defined(DCHECK_IS_CONFIGURABLE)
// Title and description for the flag to enable configurable DCHECKs.
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Title and description for the flag to add the button in the settings to send
// the users in the Settings.app to update the default browser.
extern const char kDefaultBrowserSettingsName[];
extern const char kDefaultBrowserSettingsDescription[];

// Title and description for the flag to request the desktop version of web site
// by default on iPad
extern const char kDefaultToDesktopOnIPadName[];
extern const char kDefaultToDesktopOnIPadDescription[];

// Title and description for the flag to control the delay (in minutes) for
// polling for the existence of Gaia cookies for google.com.
extern const char kDelayThresholdMinutesToUpdateGaiaCookieName[];
extern const char kDelayThresholdMinutesToUpdateGaiaCookieDescription[];

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to disable progress bar animation.
extern const char kDisableProgressBarAnimationName[];
extern const char kDisableProgressBarAnimationDescription[];

// Title and description for the flag to replace the Zine feed with the
// Discover feed in the Bling NTP.
extern const char kDiscoverFeedInNtpName[];
extern const char kDiscoverFeedInNtpDescription[];

// Title and description for the flag to enable EditBookmarks enterprise
// policy on iOS.
extern const char kEditBookmarksIOSName[];
extern const char kEditBookmarksIOSDescription[];

// Title and description for the flag to enable kEditPasswordsInSettings flag on
// iOS.
extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

// Title and description for the flag to block restore urls.
extern const char kEmbedderBlockRestoreUrlName[];
extern const char kEmbedderBlockRestoreUrlDescription[];

// Title and description for the flag to enable the confirmational action sheet
// for the tab grid "Close All" action.
extern const char kEnableCloseAllTabsConfirmationName[];
extern const char kEnableCloseAllTabsConfirmationDescription[];

// Title and description for the flag to enable fullpage screenshots.
extern const char kEnableFullPageScreenshotName[];
extern const char kEnableFullPageScreenshotDescription[];

// Title and description for the flag to enable incognito mode management.
extern const char kEnableIncognitoModeAvailabilityIOSName[];
extern const char kEnableIncognitoModeAvailabilityIOSDescription[];

// Title and description for the flag to enable to show a different UI when the
// setting is managed by an enterprise policy.
extern const char kEnableIOSManagedSettingsUIName[];
extern const char kEnableIOSManagedSettingsUIDescription[];

// Title and description for the flag to enable new context menus for native UI.
extern const char kEnableNativeContextMenusName[];
extern const char kEnableNativeContextMenusDescription[];

// Title and description for the flag to enable an expanded tab strip.
extern const char kExpandedTabStripName[];
extern const char kExpandedTabStripDescription[];

// Title and description for the flag to extend Open in toolbar files support.
extern const char kExtendOpenInFilesSupportName[];
extern const char kExtendOpenInFilesSupportDescription[];

// Title and description for the flag to trigger the startup sign-in promo.
extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

// Title and description for the flag to force an unstacked tabstrip.
extern const char kForceUnstackedTabstripName[];
extern const char kForceUnstackedTabstripDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

// Title and dscription for the flag to allow biometric authentication for
// accessing incognito.
extern const char kIncognitoAuthenticationName[];
extern const char kIncognitoAuthenticationDescription[];

// Title and description for the flag to enable new illustrations and
// UI on empty states.
extern const char kIllustratedEmptyStatesName[];
extern const char kIllustratedEmptyStatesDescription[];

// Title and description for the flag to present the new UI Reboot on Infobars
// using OverlayPresenter.
extern const char kInfobarOverlayUIName[];
extern const char kInfobarOverlayUIDescription[];

// Title and description for the flag to enable the new UI Reboot on Infobars.
extern const char kInfobarUIRebootName[];
extern const char kInfobarUIRebootDescription[];

// Title and description for the flag to enable the new UI Reboot on Infobars
// only on iOS13.
extern const char kInfobarUIRebootOnlyiOS13Name[];
extern const char kInfobarUIRebootOnlyiOS13Description[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable interstitials on legacy TLS
// connections.
extern const char kIOSLegacyTLSInterstitialsName[];
extern const char kIOSLegacyTLSInterstitialsDescription[];

// Title and description for the flag to persist the Crash Restore Infobar
// across navigations.
extern const char kIOSPersistCrashRestoreName[];
extern const char kIOSPersistCrashRestoreDescription[];

// Title and description for the flag to enable Shared Highlighting color
// change in iOS.
extern const char kIOSSharedHighlightingColorChangeName[];
extern const char kIOSSharedHighlightingColorChangeDescription[];

// Title and description for the flag to experiment with different location
// permission user experiences.
extern const char kLocationPermissionsPromptName[];
extern const char kLocationPermissionsPromptDescription[];

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title and description for the flag to enable ManagedBookmarks enterprise
// policy on iOS.
extern const char kManagedBookmarksIOSName[];
extern const char kManagedBookmarksIOSDescription[];

// Title and description for the flag where the Google SRP is requested in
// mobile mode by default.
extern const char kMobileGoogleSRPName[];
extern const char kMobileGoogleSRPDescription[];

// Title and description for the flag to enable mobile identity consistency.
extern const char kMobileIdentityConsistencyName[];
extern const char kMobileIdentityConsistencyDescription[];

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
// Title and description for the flag used to test the newly
// implemented tabstrip.
extern const char kModernTabStripName[];
extern const char kModernTabStripDescription[];

// Title and description for the flag to change the max number of autocomplete
// matches in the omnibox popup.
extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (non incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

// Title and description for the flag to control Omnibox on-focus suggestions.
extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

// Title and description for the flag to control Omnibox Local zero-prefix
// suggestions.
extern const char kOmniboxLocalHistoryZeroSuggestName[];
extern const char kOmniboxLocalHistoryZeroSuggestDescription[];

// Title and description for the flag that enables the refactored new tab page.
extern const char kRefactoredNTPName[];
extern const char kRefactoredNTPDescription[];

// Title and description for the flag that makes Safe Browsing available.
extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

// Title and description for the flag to enable real-time Safe Browsing lookups.
extern const char kSafeBrowsingRealTimeLookupName[];
extern const char kSafeBrowsingRealTimeLookupDescription[];

// Title and description for the flag to enable safety check on iOS.
extern const char kSafetyCheckIOSName[];
extern const char kSafetyCheckIOSDescription[];

// Title and description for the flag that enables Messages UI on
// SaveCard Infobars.
extern const char kSaveCardInfobarMessagesUIName[];
extern const char kSaveCardInfobarMessagesUIDescription[];

// Title and description for the flag to enable integration with the ScreenTime
// system.
extern const char kScreenTimeIntegrationName[];
extern const char kScreenTimeIntegrationDescription[];

// Title and description for the flag to enable the Scroll to Text feature.
extern const char kScrollToTextIOSName[];
extern const char kScrollToTextIOSDescription[];

// Title and description for the flag to enable the send tab to self receiving
// feature.
extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

// Title and description for the flag to send UMA data over any network.
extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

// Title and description for the flag to toggle the flag for the settings UI
// Refresh.
extern const char kSettingsRefreshName[];
extern const char kSettingsRefreshDescription[];

// Title and description for the flag to enable Shared Highlighting (Link to
// Text Edit Menu option).
extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to use |-drawViewHierarchy:| for taking
// snapshots.
extern const char kSnapshotDrawViewName[];
extern const char kSnapshotDrawViewDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable the Messages UI for Translate
// Infobars.
extern const char kTranslateInfobarMessagesUIName[];
extern const char kTranslateInfobarMessagesUIDescription[];

// Title and description for the flag to enable URLBlocklist/URLAllowlist
// enterprise policy.
extern const char kURLBlocklistIOSName[];
extern const char kURLBlocklistIOSDescription[];

// Title and description for the flag to enable the new error page workflow.
extern const char kUseJSForErrorPageName[];
extern const char kUseJSForErrorPageDescription[];

// Title and description for the flag to enable use of hash affiliation service.
extern const char kUseOfHashAffiliationFetcherName[];
extern const char kUseOfHashAffiliationFetcherDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to tie the default text zoom level to
// the dynamic type setting.
extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

// Title and description for the flag to enable text accessibility in webpages.
extern const char kWebPageTextAccessibilityName[];
extern const char kWebPageTextAccessibilityDescription[];

// Title and description for the flag to enable a different method of zooming
// web pages.
extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

// Title and description for the flag to enable the native context menus in the
// WebView.
extern const char kWebViewNativeContextMenuName[];
extern const char kWebViewNativeContextMenuDescription[];

// Title and description for the flag to restore Gaia cookies as soon as Chrome
// detects that they have been removed.
extern const char kRestoreGaiaCookiesIfDeletedName[];
extern const char kRestoreGaiaCookiesIfDeletedDescription[];

// Title and description for the flag to restore Gaia cookies when the user
// explicitly requests to be signed in to a Google service.
extern const char kRestoreGaiaCookiesOnUserActionName[];
extern const char kRestoreGaiaCookiesOnUserActionDescription[];

extern const char kRecordSnapshotSizeName[];
extern const char kRecordSnapshotSizeDescription[];

// Title and description for the flag to show a modified fullscreen modal promo
// with a button that would send the users in the Settings.app to update the
// default browser.
extern const char kDefaultBrowserFullscreenPromoExperimentName[];
extern const char kDefaultBrowserFullscreenPromoExperimentDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "Availability.h"

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to enable
// kSupportForAddPasswordsInSettings flag on iOS.
extern const char kAddPasswordsInSettingsName[];
extern const char kAddPasswordsInSettingsDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control offers in downstream.
extern const char kAutofillEnableOffersInDownstreamName[];
extern const char kAutofillEnableOffersInDownstreamDescription[];

// Title and description for the flag to fill promo code fields with Autofill.
extern const char kAutofillFillMerchantPromoCodeFieldsName[];
extern const char kAutofillFillMerchantPromoCodeFieldsDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to parse promo code fields in Autofill.
extern const char kAutofillParseMerchantPromoCodeFieldsName[];
extern const char kAutofillParseMerchantPromoCodeFieldsDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to control dismissing the Save Card
// Infobar on Navigation.
extern const char kAutofillSaveCardDismissOnNavigationName[];
extern const char kAutofillSaveCardDismissOnNavigationDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag that controls whether Autofill's
// logic is using numeric unique renderer IDs instead of string IDs for
// form and field elements.
extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Enables the user to track prices of the Shopping URLs they are visiting.
// The first variation is to display price drops in the Tab Switching UI when
// they are identified.
extern const char kCommercePriceTrackingName[];
extern const char kCommercePriceTrackingDescription[];

// Title and description for the flag to enable context menu actions refresh.
extern const char kContextMenuActionsRefreshName[];
extern const char kContextMenuActionsRefreshDescription[];

// Title and description for the flag to control which crash generation tool
// is used.
extern const char kCrashpadIOSName[];
extern const char kCrashpadIOSDescription[];

// Title and description for the flag to trigger credentials provider extension
// promo.
extern const char kCredentialProviderExtensionPromoName[];
extern const char kCredentialProviderExtensionPromoDescription[];

#if defined(DCHECK_IS_CONFIGURABLE)
// Title and description for the flag to enable configurable DCHECKs.
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Title and description for the flag to show a modified fullscreen modal promo
// with a button that would send the users in the Settings.app to update the
// default browser.
extern const char kDefaultBrowserFullscreenPromoExperimentName[];
extern const char kDefaultBrowserFullscreenPromoExperimentDescription[];

// Title and description for the flag that is used to let the user choose the
// default mode (Mobile/Desktop) they would like to use when requesting a page.
extern const char kAddSettingForDefaultPageModeName[];
extern const char kAddSettingForDefaultPageModeDescription[];

// Title and description for the flag to use default WebKit context menu in web
// content.
extern const char kDefaultWebViewContextMenuName[];
extern const char kDefaultWebViewContextMenuDescription[];

// Title and description for the flag to control the delay (in minutes) for
// polling for the existence of Gaia cookies for google.com.
extern const char kDelayThresholdMinutesToUpdateGaiaCookieName[];
extern const char kDelayThresholdMinutesToUpdateGaiaCookieDescription[];

// Title and description for the flag to detect change password form
// submisison when the form is cleared by the website.
extern const char kDetectFormSubmissionOnFormClearIOSName[];
extern const char kDetectFormSubmissionOnFormClearIOSDescription[];

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to replace the Zine feed with the
// Discover feed in the Bling NTP.
extern const char kDiscoverFeedInNtpName[];
extern const char kDiscoverFeedInNtpDescription[];

// Title and description for the flag to enable Vcard support.
extern const char kDownloadVcardName[];
extern const char kDownloadVcardDescription[];

// Title and description for the flag to enable kEditPasswordsInSettings flag on
// iOS.
extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

// Title and description for the flag to enable address verification support in
// autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptAddressVerificationName[];
extern const char
    kEnableAutofillAddressSavePromptAddressVerificationDescription[];

// Title and description for the flag to enable autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

// Title and description for the flag to enable the discover feed discofeed
// endpoint.
extern const char kEnableDiscoverFeedDiscoFeedEndpointName[];
extern const char kEnableDiscoverFeedDiscoFeedEndpointDescription[];

// Title and description for the flag to enable the discover feed live preview
// in long-press feed context menu.
extern const char kEnableDiscoverFeedPreviewName[];
extern const char kEnableDiscoverFeedPreviewDescription[];

// Title and description for the flag to shorten the cache.
extern const char kEnableDiscoverFeedShorterCacheName[];
extern const char kEnableDiscoverFeedShorterCacheDescription[];

// Title and description for the flag to enable the discover feed static
// resource serving.
extern const char kEnableDiscoverFeedStaticResourceServingName[];
extern const char kEnableDiscoverFeedStaticResourceServingDescription[];

// Title and description for the flag to test the FRE default browser promo
// experiment.
extern const char kEnableFREDefaultBrowserScreenTestingName[];
extern const char kEnableFREDefaultBrowserScreenTestingDescription[];

// Title and description for the flag to enable FRE UI module.
extern const char kEnableFREUIModuleIOSName[];
extern const char kEnableFREUIModuleIOSDescription[];

// Title and description for the flag to enable long message duration.
extern const char kEnableLongMessageDurationName[];
extern const char kEnableLongMessageDurationDescription[];

// Title and description for the flag to enable UI that allows the user to
// create a strong password even if the field wasn't parsed as a new password
// field.
extern const char kEnableManualPasswordGenerationName[];
extern const char kEnableManualPasswordGenerationDescription[];

// Title and description for the flag to enable the new download API.
extern const char kEnableNewDownloadAPIName[];
extern const char kEnableNewDownloadAPIDescription[];

// Title and description for the flag to enable optimization guide.
extern const char kEnableOptimizationGuideName[];
extern const char kEnableOptimizationGuideDescription[];

// Title and description for the flag to enable optimization guide metadata
// validation.
extern const char kEnableOptimizationGuideMetadataValidationName[];
extern const char kEnableOptimizationGuideMetadataValidationDescription[];

// Title and description for the flag to enable optimization guide hints
// fetching for MSBB users.
extern const char kEnableOptimizationHintsFetchingMSBBName[];
extern const char kEnableOptimizationHintsFetchingMSBBDescription[];

// Title and description for the flag to enable shorted password auto-fill
// instructions and button.
extern const char kEnableShortenedPasswordAutoFillInstructionName[];
extern const char kEnableShortenedPasswordAutoFillInstructionDescription[];

// Title and description for the flag to introduce following web channels on
// Chrome iOS.
extern const char kEnableWebChannelsName[];
extern const char kEnableWebChannelsDescription[];

// Title and description for the flag to enable an expanded tab strip.
extern const char kExpandedTabStripName[];
extern const char kExpandedTabStripDescription[];

// Title and description for the flag to enable filling across affiliated
// websites.
extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

// Title and description for the flag to disable all extended sync promos.
extern const char kForceDisableExtendedSyncPromosName[];
extern const char kForceDisableExtendedSyncPromosDescription[];

// Title and description for the flag to trigger the startup sign-in promo.
extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

// Title and description for the flag to set the major version the UA string to
// 100.
extern const char kForceMajorVersion100InUserAgentName[];
extern const char kForceMajorVersion100InUserAgentDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

// Title and description for the flag to enable dark mode colors while in
// Incognito mode.
extern const char kIncognitoBrandConsistencyForIOSName[];
extern const char kIncognitoBrandConsistencyForIOSDescription[];

// Title and description for the flag to enable revamped Incognito NTP page.
extern const char kIncognitoNtpRevampName[];
extern const char kIncognitoNtpRevampDescription[];

// Title and description for the flag that conditionally uploads clicks and view
// actions in the feed (e.g., the user needs to view X cards).
extern const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable updated password manager
// branding.
extern const char kIOSEnablePasswordManagerBrandingUpdateName[];
extern const char kIOSEnablePasswordManagerBrandingUpdateDescription[];

// Title and description for the flag to persist the Crash Restore Infobar
// across navigations.
extern const char kIOSPersistCrashRestoreName[];
extern const char kIOSPersistCrashRestoreDescription[];

// Title and description for the flag to enable Shared Highlighting color
// change in iOS.
extern const char kIOSSharedHighlightingColorChangeName[];
extern const char kIOSSharedHighlightingColorChangeDescription[];

// Title and description for the flag to enable Shared Highlighting on AMP pages
// in iOS.
extern const char kIOSSharedHighlightingAmpName[];
extern const char kIOSSharedHighlightingAmpDescription[];

// Title and description for the flag to enable browser-layer improvements to
// the text fragments UI.
extern const char kIOSSharedHighlightingV2Name[];
extern const char kIOSSharedHighlightingV2Description[];

// Title and description for the flag to enable unrealized WebStates.
extern const char kLazilyCreateWebStateOnRestorationName[];
extern const char kLazilyCreateWebStateOnRestorationDescription[];

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title and description for the flag that controls whether event breadcrumbs
// are captured.
extern const char kLogBreadcrumbsName[];
extern const char kLogBreadcrumbsDescription[];

// Title and description for the flag to control camera and/or microphone access
// for a specific site through site settings during its lifespan.
extern const char kMediaPermissionsControlName[];
extern const char kMediaPermissionsControlDescription[];

// Title and description for the flag that controls sending metrickit crash
// reports.
extern const char kMetrickitCrashReportName[];
extern const char kMetrickitCrashReportDescription[];

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
// Title and description for the flag used to test the newly
// implemented tabstrip.
extern const char kModernTabStripName[];
extern const char kModernTabStripDescription[];

// Title and description for the flag to enable the new overflow menu.
extern const char kNewOverflowMenuName[];
extern const char kNewOverflowMenuDescription[];

// Title and description for temporary bug fix to broken NTP view hierarhy.
// TODO(crbug.com/1262536): Remove this when fixed.
extern const char kNTPViewHierarchyRepairName[];
extern const char kNTPViewHierarchyRepairDescription[];

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

// Title and description for the flag to swap Omnibox Textfield implementation
// to a new experimental one.
extern const char kOmniboxNewImplementationName[];
extern const char kOmniboxNewImplementationDescription[];

// Title and description for the flag to enable PhishGuard password reuse
// detection.
extern const char kPasswordReuseDetectionName[];
extern const char kPasswordReuseDetectionDescription[];

// Title and description for the flag to enable the Reading List Messages.
extern const char kReadingListMessagesName[];
extern const char kReadingListMessagesDescription[];

// Title and description for the flag to enable Reading List Time to Read.
extern const char kReadingListTimeToReadName[];
extern const char kReadingListTimeToReadDescription[];

// Title and description for the flag to native restore web states.
extern const char kRestoreSessionFromCacheName[];
extern const char kRestoreSessionFromCacheDescription[];

extern const char kRecordSnapshotSizeName[];
extern const char kRecordSnapshotSizeDescription[];

// Title and description for the flag to remove excess NTP tabs that don't have
// navigation history.
extern const char kRemoveExcessNTPsExperimentName[];
extern const char kRemoveExcessNTPsExperimentDescription[];

// Title and description for the flag that makes Safe Browsing available.
extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

// Title and description for the flag to enable real-time Safe Browsing lookups.
extern const char kSafeBrowsingRealTimeLookupName[];
extern const char kSafeBrowsingRealTimeLookupDescription[];

// Title and description for the flag to enable saving one file per webstate.
extern const char kSaveSessionTabsToSeparateFilesName[];
extern const char kSaveSessionTabsToSeparateFilesDescription[];

// Title and description for the flag to enable integration with the ScreenTime
// system.
extern const char kScreenTimeIntegrationName[];
extern const char kScreenTimeIntegrationDescription[];

// Title and description for the flag to enable the Search History Link feature.
extern const char kSearchHistoryLinkIOSName[];
extern const char kSearchHistoryLinkIOSDescription[];

// Title and description for the flag to enable the send-tab-to-self for a
// signed-in user (non-syncing).
extern const char kSendTabToSelfWhenSignedInName[];
extern const char kSendTabToSelfWhenSignedInDescription[];

// Title and description for the flag to enable the "Manage devices" link in
// the send-tab-to-self feature UI.
extern const char kSendTabToSelfManageDevicesLinkName[];
extern const char kSendTabToSelfManageDevicesLinkDescription[];

// Title and description for the flag to send UMA data over any network.
extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

// Title and description for the flag to enable Shared Highlighting (Link to
// Text Edit Menu option).
extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to use one NTP for all tabs in a Browser.
extern const char kSingleNtpName[];
extern const char kSingleNtpDescription[];

// Title and description for the flag to enable the Start Surface.
extern const char kStartSurfaceName[];
extern const char kStartSurfaceDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag that controls synthetic crash reports
// generation for Unexplained Termination Events.
extern const char kSyntheticCrashReportsForUteName[];
extern const char kSyntheticCrashReportsForUteDescription[];

// Title and description for the flag to synthesize native restore web states.
extern const char kSynthesizedRestoreSessionName[];
extern const char kSynthesizedRestoreSessionDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault RPC.
extern const char kSyncTrustedVaultPassphraseiOSRPCName[];
extern const char kSyncTrustedVaultPassphraseiOSRPCDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault passphrase promos.
extern const char kSyncTrustedVaultPassphrasePromoName[];
extern const char kSyncTrustedVaultPassphrasePromoDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault passphrase type with improved recovery.
extern const char kSyncTrustedVaultPassphraseRecoveryName[];
extern const char kSyncTrustedVaultPassphraseRecoveryDescription[];

// Title and description for the flag to enable tabs search feature.
extern const char kTabsSearchName[];
extern const char kTabsSearchDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable removing any entry points to the
// history UI from Incognito mode.
extern const char kUpdateHistoryEntryPointsInIncognitoName[];
extern const char kUpdateHistoryEntryPointsInIncognitoDescription[];

// Title and description for the flag to enable using Lens to search for an
// image from the long press context menu.
extern const char kUseLensToSearchForImageName[];
extern const char kUseLensToSearchForImageDescription[];

// Title and description for the flag to enable the replacement of some images
// by SFSymbols.
extern const char kUseSFSymbolsSamplesName[];
extern const char kUseSFSymbolsSamplesDescription[];

// Title and description for the flag to replace the popup menus by UIKit menus.
extern const char kUseUIKitPopupMenuName[];
extern const char kUseUIKitPopupMenuDescription[];

// Title and description for the flag to control the maximum wait time (in
// seconds) for a response from the Account Capabilities API.
extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to tie the default text zoom level to
// the dynamic type setting.
extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

// Title and description for the flag to enable a different method of zooming
// web pages.
extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

// Title and description for the flag to enable the phase 2 of context menus in
// the WebView.
extern const char kWebViewNativeContextMenuPhase2Name[];
extern const char kWebViewNativeContextMenuPhase2Description[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

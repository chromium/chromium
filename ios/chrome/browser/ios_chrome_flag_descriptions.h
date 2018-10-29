// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_

namespace flag_descriptions {

// Title and description for the flag to control the autofill query cache.
extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

// Title and description for the flag to control deprecating company name.
extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control GPay branding in credit card
// downstream keyboard accessory.
extern const char kAutofillDownstreamUseGooglePayBrandingOniOSName[];
extern const char kAutofillDownstreamUseGooglePayBrandingOniOSDescription[];

// Title and description for the flag to control the updated prompt explanation
// when offering credit card upload.
extern const char kEnableAutofillCreditCardUploadUpdatePromptExplanationName[];
extern const char
    kEnableAutofillCreditCardUploadUpdatePromptExplanationDescription[];

// Title and description for the flag to control if credit card save should
// utilize the Autofill StrikeDatabase when determining whether save should be
// offered.
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[];
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag to control if Chrome Sync can start up in
// standalone transport mode.
extern const char kSyncStandaloneTransportName[];
extern const char kSyncStandaloneTransportDescription[];

// Title and description for the flag to control if Chrome Sync (in standalone
// transport mode) supports non-primary accounts.
extern const char kSyncSupportSecondaryAccountName[];
extern const char kSyncSupportSecondaryAccountDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to control the new app launcher.
extern const char kAppLauncherRefreshName[];
extern const char kAppLauncherRefreshDescription[];

// Title and description for the flag to control the dynamic autofill.
extern const char kAutofillDynamicFormsName[];
extern const char kAutofillDynamicFormsDescription[];

// Title and description for the flag to control the dynamic autofill.
extern const char kAutofillPrefilledFieldsName[];
extern const char kAutofillPrefilledFieldsDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to control if manual fallback is enabled.
extern const char kAutofillManualFallbackName[];
extern const char kAutofillManualFallbackDescription[];

// Title and description for the flag to control if manual fallback is enabled.
extern const char kAutofillManualFallbackPhaseTwoName[];
extern const char kAutofillManualFallbackPhaseTwoDescription[];

// Title and description for the flag to control if prefilled value filter
// profiles.
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsName[];
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[];

// Title and description for the flag to restrict extraction of formless forms
// to checkout flows.
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

// Title and description for the flag to control GPay branding in credit card
// upstream infobar.
extern const char kAutofillUpstreamUseGooglePayBrandingOnMobileName[];
extern const char kAutofillUpstreamUseGooglePayBrandingOnMobileDescription[];

// Title and description for the flag to make browser container fullscreen.
extern const char kBrowserContainerFullscreenName[];
extern const char kBrowserContainerFullscreenDescription[];

// Title and description for the flag to make browser container contain the NTP
// directly.
extern const char kBrowserContainerContainsNTPName[];
extern const char kBrowserContainerContainsNTPDescription[];

// Title and description for the flag to control redirection to the task
// scheduler.
extern const char kBrowserTaskScheduler[];
extern const char kBrowserTaskSchedulerDescription[];

// Title and description for the flag to enable Captive Portal metrics logging.
extern const char kCaptivePortalMetricsName[];
extern const char kCaptivePortalMetricsDescription[];

// Title and description for the flag to enable automatically switching to the
// regular tabs after closing the last incognito tab.
extern const char kClosingLastIncognitoTabName[];
extern const char kClosingLastIncognitoTabDescription[];

// Title and description for the flag to enable Contextual Search.
extern const char kContextualSearch[];
extern const char kContextualSearchDescription[];

// Title and description for the flag to enable copying image.
extern const char kCopyImageName[];
extern const char kCopyImageDescription[];

// Title and description for the flag to enable drag and drop.
extern const char kDragAndDropName[];
extern const char kDragAndDropDescription[];

// Title and description for the flag to enable new Clear Browsing Data UI.
extern const char kNewClearBrowsingDataUIName[];
extern const char kNewClearBrowsingDataUIDescription[];

// Title and description for the flag to enable External Search.
extern const char kExternalSearchName[];
extern const char kExternalSearchDescription[];

// Title and description for the flag to enable invaliations delivery via FCM.
extern const char kFCMInvalidationsName[];
extern const char kFCMInvalidationsDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenViewportAdjustmentExperimentName[];
extern const char kFullscreenViewportAdjustmentExperimentDescription[];

// Title and description for the flag to enable History batch filtering.
extern const char kHistoryBatchUpdatesFilterName[];
extern const char kHistoryBatchUpdatesFilterDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable ITunes links store kit handling.
extern const char kITunesUrlsStoreKitHandlingName[];
extern const char kITunesUrlsStoreKitHandlingDescription[];

// Title, description, and options for Google UI menu for handling mailto links.
extern const char kMailtoHandlingWithGoogleUIName[];
extern const char kMailtoHandlingWithGoogleUIDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];

// Title and description for the flag to enable the Memex Tab Switcher.
extern const char kMemexTabSwitcherName[];
extern const char kMemexTabSwitcherDescription[];

// Title and description for the flag to enable new password form parsing.
extern const char kNewPasswordFormParsingName[];
extern const char kNewPasswordFormParsingDescription[];

// Title and description for the flag to show most visited sites and collection
// shortcuts in the omnibox popup instead of ZeroSuggest.
extern const char kOmniboxPopupShortcutIconsInZeroStateName[];
extern const char kOmniboxPopupShortcutIconsInZeroStateDescription[];

// Title and description for the flag to enable the "switch to this tab" option
// in the omnibox suggestion. It doesn't add new suggestions.
extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

// Title and description for the flag to enable elision of the URL path, query,
// and ref in omnibox URL suggestions.
extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

// Title and description for the flag to control the out of web implementation
// of fullscreen.
extern const char kOutOfWebFullscreenName[];
extern const char kOutOfWebFullscreenDescription[];

// Title and description for the flag to enable Physical Web in the omnibox.
extern const char kPhysicalWeb[];
extern const char kPhysicalWebDescription[];

// Title and description for the flag to ignore viewport scale limits.
extern const char kIgnoresViewportScaleLimitsName[];
extern const char kIgnoresViewportScaleLimitsDescription[];

// Title and description for the flag to toggle the flag of the search button.
extern const char kSearchIconToggleName[];
extern const char kSearchIconToggleDescription[];

// Title and description for the flag to enable WKBackForwardList based
// navigation manager.
extern const char kSlimNavigationManagerName[];
extern const char kSlimNavigationManagerDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to enable WKWebView in SSO.
extern const char kSSOWithWKWebViewName[];
extern const char kSSOWithWKWebViewDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable the unified consent.
extern const char kUnifiedConsentName[];
extern const char kUnifiedConsentDescription[];

// Title and description for the flag to enable Gaia Auth Mutlilogin endpoint
// for identity consistency.
extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

// Title and description for the flag to force the consent bump.
extern const char kForceUnifiedConsentBumpName[];
extern const char kForceUnifiedConsentBumpDescription[];

// Title and description for the flag to enable the ddljson Doodle API.
extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

// Title and description for the flag to enable web frame messaging.
extern const char kWebFrameMessagingName[];
extern const char kWebFrameMessagingDescription[];

// Title and description for the flag to enable text accessibility in webpages.
extern const char kWebPageTextAccessibilityName[];
extern const char kWebPageTextAccessibilityDescription[];

// Title and description for the flag to enable WKHTTPSystemCookieStore usage
// for main context URL requests.
extern const char kWKHTTPSystemCookieStoreName[];
extern const char kWKHTTPSystemCookieStoreDescription[];

// Title and description for the flag to use the WKWebView snapshotting API.
extern const char kWKWebViewSnapshotsName[];
extern const char kWKWebViewSnapshotsDescription[];

// Title and description for the flag to allow custom search engines.
extern const char kCustomSearchEnginesName[];
extern const char kCustomSearchEnginesDescription[];

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_

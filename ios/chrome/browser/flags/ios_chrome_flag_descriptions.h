// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "Availability.h"

#include "base/debug/debugging_buildflags.h"

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to enable add to home screen button in
// share menu.
extern const char kAddToHomeScreenName[];
extern const char kAddToHomeScreenDescription[];

// Title and description for the flag to enable the App Store Rating promo.
extern const char kAppStoreRatingName[];
extern const char kAppStoreRatingDescription[];

// Title and description for the flag to enable save of profiles in Google
// Account.
extern const char kAutofillAccountProfilesStorageName[];
extern const char kAutofillAccountProfilesStorageDescription[];

// Title and description for the flag to enable compatibility with GAS profiles.
extern const char kAutofillAccountProfilesUnionViewName[];
extern const char kAutofillAccountProfilesUnionViewDescription[];

// Title and description for the flag to enable Chrome branding on form input
// suggestions.
extern const char kAutofillBrandingIOSName[];
extern const char kAutofillBrandingIOSDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to enable custom card art images for
// autofill Payments UI, instead of built-in network images.
extern const char kAutofillEnableCardArtImageName[];
extern const char kAutofillEnableCardArtImageDescription[];

// Title and description for the flag to control the new autofill suggestion
// ranking formula for address profiles.
extern const char kAutofillEnableRankingFormulaAddressProfilesName[];
extern const char kAutofillEnableRankingFormulaAddressProfilesDescription[];

// Title and description for the flag to control the new autofill suggestion
// ranking formula for credit cards.
extern const char kAutofillEnableRankingFormulaCreditCardsName[];
extern const char kAutofillEnableRankingFormulaCreditCardsDescription[];

// Title and description for the flag that controls whether the remade Autofill
// Downstream metrics are enabled.
extern const char kAutofillEnableRemadeDownstreamMetricsName[];
extern const char kAutofillEnableRemadeDownstreamMetricsDescription[];

// Title and description for flag to enable showing card product name (instead
// of issuer network) in Payments UI.
extern const char kAutofillEnableCardProductNameName[];
extern const char kAutofillEnableCardProductNameDescription[];

// Title and description for flag to enforce delays between offering Autofill
// opportunities.
extern const char kAutofillEnforceDelaysInStrikeDatabaseName[];
extern const char kAutofillEnforceDelaysInStrikeDatabaseDescription[];

// Title and description for the flag to fill promo code fields with Autofill.
extern const char kAutofillFillMerchantPromoCodeFieldsName[];
extern const char kAutofillFillMerchantPromoCodeFieldsDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to control whether the autofill should
// ffer credit card save for cards with same last-4 but different expiration
// dates.
extern const char kAutofillOfferToSaveCardWithSameLastFourName[];
extern const char kAutofillOfferToSaveCardWithSameLastFourDescription[];

// Title and description for the flag to parse IBAN fields in Autofill.
extern const char kAutofillParseIBANFieldsName[];
extern const char kAutofillParseIBANFieldsDescription[];

// Title and description for the flag to parse standalone CVC fields for VCN
// card on file in Autofill.
extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[];
extern const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to suggest Server card instead of a
// deduped Local card.
extern const char kAutofillSuggestServerCardInsteadOfLocalCardName[];
extern const char kAutofillSuggestServerCardInsteadOfLocalCardDescription[];

// Title and description for the flag to control allowing credit card upload
// save for accounts from common email providers.
extern const char kAutofillUpstreamAllowAdditionalEmailDomainsName[];
extern const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[];

// Title and description for the flag to control allowing credit card upload
// save for all accounts, regardless of the email domain.
extern const char kAutofillUpstreamAllowAllEmailDomainsName[];
extern const char kAutofillUpstreamAllowAllEmailDomainsDescription[];

// Title and description for the flag to control whether the preflight credit
// card upload call contains an authentication token or not.
extern const char kAutofillUpstreamAuthenticatePreflightCallName[];
extern const char kAutofillUpstreamAuthenticatePreflightCallDescription[];

// Title and description for the flag to control whether a different data type
// is used for passing the card number during credit card upload save.
extern const char kAutofillUpstreamUseAlternateSecureDataTypeName[];
extern const char kAutofillUpstreamUseAlternateSecureDataTypeDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag that controls whether Autofill's
// logic is using numeric unique renderer IDs instead of string IDs for
// form and field elements.
extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

// Title and description for the flag that moves the omnibox to the bottom in
// the steady state.
extern const char kBottomOmniboxSteadyStateName[];
extern const char kBottomOmniboxSteadyStateDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Title and description for the flag to turn on "Bring Your Own Tabs" prompt
// for new Android switchers.
extern const char kBringYourOwnTabsIOSName[];
extern const char kBringYourOwnTabsIOSDescription[];

// Title and description for the flag to enable the the sign-in-only flow
// when no device level account is detected.
extern const char kConsistencyNewAccountInterfaceName[];
extern const char kConsistencyNewAccountInterfaceDescription[];

// Title and description for the flag to enable experience kit apple calendar
// events.
extern const char kAppleCalendarExperienceKitName[];
extern const char kAppleCalendarExperienceKitDescription[];

// Title and description for the flag to enable emails detection and processing
extern const char kEmailName[];
extern const char kEmailDescription[];

// Title and description for the flag to enable phone numbers detection and
// processing.
extern const char kPhoneNumberName[];
extern const char kPhoneNumberDescription[];

// Title and description for the flag to enable text classifier entity detection
// in experience kit for different entity types.
extern const char kEnableExpKitTextClassifierName[];
extern const char kEnableExpKitTextClassifierDescription[];

// Title and description for popout omnibox on iPad feature.
extern const char kEnablePopoutOmniboxIpadName[];
extern const char kEnablePopoutOmniboxIpadDescription[];

// Title and description for UIButtonConfiguration.
extern const char kEnableUIButtonConfigurationName[];
extern const char kEnableUIButtonConfigurationDescription[];

// Title and description for the flag to enable the Credential
// Provider Extension promo.
extern const char kCredentialProviderExtensionPromoName[];
extern const char kCredentialProviderExtensionPromoDescription[];

// Title and description for the flag to enable the default browser blue dot
// promo.
extern const char kDefaultBrowserBlueDotPromoName[];
extern const char kDefaultBrowserBlueDotPromoDescription[];

// Title and description for the flag to show a modified fullscreen modal promo
// with a button that would send the users in the Settings.app to update the
// default browser.
extern const char kDefaultBrowserFullscreenPromoExperimentName[];
extern const char kDefaultBrowserFullscreenPromoExperimentDescription[];

// Title and description for the flag to show the default browser tutorial from
// an external app.
extern const char kDefaultBrowserIntentsShowSettingsName[];
extern const char kDefaultBrowserIntentsShowSettingsDescription[];

// Title and description for the flag to allow the fullscreen default browser
// promos to be added to the promo manager.
extern const char kDefaultBrowserRefactoringPromoManagerName[];
extern const char kDefaultBrowserRefactoringPromoManagerDescription[];

// Title and description for the flag to enable the default browser video promo.
extern const char kDefaultBrowserVideoPromoName[];
extern const char kDefaultBrowserVideoPromoDescription[];

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to enable the bookmarks account storage
// and related UI features.
extern const char kEnableBookmarksAccountStorageName[];
extern const char kEnableBookmarksAccountStorageDescription[];

// Title and description for the flag to enable browser lockdown mode.
extern const char kBrowserLockdownModeAvailableName[];
extern const char kBrowserLockdownModeAvailableDescription[];

// Title and description for the flag to enable checking feed visibility on
// attention log start.
extern const char kEnableCheckVisibilityOnAttentionLogStartName[];
extern const char kEnableCheckVisibilityOnAttentionLogStartDescription[];

// Title and description for the flag to enable the muting of compromised
// passwords in the Password Manager.
extern const char kEnableCompromisedPasswordsMutingName[];
extern const char kEnableCompromisedPasswordsMutingDescription[];

// Title and description for the flag to enable the sync promotion on top of the
// discover feed.
extern const char kEnableDiscoverFeedTopSyncPromoName[];
extern const char kEnableDiscoverFeedTopSyncPromoDescription[];

// Title and description for the flag to enable the email in the snackbar
// indicating that a new bookmark or reading list item is added.
extern const char kEnableEmailInBookmarksReadingListSnackbarName[];
extern const char kEnableEmailInBookmarksReadingListSnackbarDescription[];

// Title and description for the flag to modify the feed header settings.
extern const char kEnableFeedHeaderSettingsName[];
extern const char kEnableFeedHeaderSettingsDescription[];

// Title and description for the flag to enable the sign-in promotion at the
// bottom of the discover feed.
extern const char kEnableFeedBottomSignInPromoName[];
extern const char kEnableFeedBottomSignInPromoDescription[];

// Title and description for the flag to enable the sign-in promotion triggered
// by the discover feed card menu.
extern const char kEnableFeedCardMenuSignInPromoName[];
extern const char kEnableFeedCardMenuSignInPromoDescription[];

// Title and description for the flag to enable the Feed image caching.
extern const char kEnableFeedImageCachingName[];
extern const char kEnableFeedImageCachingDescription[];

// Title and description for the flag to enable Feed synthetic capabilities.
extern const char kEnableFeedSyntheticCapabilitiesName[];
extern const char kEnableFeedSyntheticCapabilitiesDescription[];

// Title and description for the flag to enable follow IPH experiment
// parameters.
extern const char kEnableFollowIPHExpParamsName[];
extern const char kEnableFollowIPHExpParamsDescription[];

// Title and description for the flag to enable follow management page instant
// reload when opening.
extern const char kEnableFollowManagementInstantReloadName[];
extern const char kEnableFollowManagementInstantReloadDescription[];

// Title and description for the flag to enable kEditPasswordsInSettings flag on
// iOS.
extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

// Title and description for the flag to enable kTailoredSecurityIntegration
// flag on iOS.
extern const char kTailoredSecurityIntegrationName[];
extern const char kTailoredSecurityIntegrationDescription[];

// Title and description for the flag to enable address verification support in
// autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptAddressVerificationName[];
extern const char
    kEnableAutofillAddressSavePromptAddressVerificationDescription[];

// Title and description for the flag to enable autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

// Title and description for the flag to enable sign-out in Clear Browser Data
// settings.
extern const char kEnableCBDSignOutName[];
extern const char kEnableCBDSignOutDescription[];

// Title and description for the flag to enable the discover feed discofeed
// endpoint.
extern const char kEnableDiscoverFeedDiscoFeedEndpointName[];
extern const char kEnableDiscoverFeedDiscoFeedEndpointDescription[];

// Title and description for the flag to enable the hiding the Most Visited
// Tiles and Shortcuts for new users.
extern const char kTileAblationName[];
extern const char kTileAblationDescription[];

// Title and description for the flag to remove the Feed from the NTP.
extern const char kEnableFeedAblationName[];
extern const char kEnableFeedAblationDescription[];

// Title and description for the flag to enable the Fullscreen API.
extern const char kEnableFullscreenAPIName[];
extern const char kEnableFullscreenAPIDescription[];

// Title and description for the flag to enable password grouping for the
// Password Manager.
extern const char kPasswordsGroupingName[];
extern const char kPasswordsGroupingDescription[];

// Title and description for the flag to enable the passwords account storage.
extern const char kEnablePasswordsAccountStorageName[];
extern const char kEnablePasswordsAccountStorageDescription[];

// Title and description for the flag to enable the preferences account storage.
extern const char kEnablePreferencesAccountStorageName[];
extern const char kEnablePreferencesAccountStorageDescription[];

// Title and description for the flag to enable pinned tabs.
extern const char kEnablePinnedTabsName[];
extern const char kEnablePinnedTabsDescription[];

// Title and description for the flag to enable the account storage.
extern const char kEnableReadingListAccountStorageName[];
extern const char kEnableReadingListAccountStorageDescription[];

// Title and description for the flag to enable the sign-in promo in the reading
// list screen.
extern const char kEnableReadingListSignInPromoName[];
extern const char kEnableReadingListSignInPromoDescription[];

// Title and description for the flag to enable refining data source reload
// reporting when having a very short attention log.
extern const char kEnableRefineDataSourceReloadReportingName[];
extern const char kEnableRefineDataSourceReloadReportingDescription[];

// Title and description for the flag to enable omnibox suggestions scrolling on
// iPad.
extern const char kEnableSuggestionsScrollingOnIPadName[];
extern const char kEnableSuggestionsScrollingOnIPadDescription[];

// Title and description for the flag to enable signed out user view demotion.
extern const char kEnableSignedOutViewDemotionName[];
extern const char kEnableSignedOutViewDemotionDescription[];

// Title and description for the flag to enable user policies.
extern const char kEnableUserPolicyName[];
extern const char kEnableUserPolicyDescription[];

// Title and description for the flag to introduce following web channels on
// Chrome iOS.
extern const char kEnableWebChannelsName[];
extern const char kEnableWebChannelsDescription[];

// Title and description for the flag to enable an expanded tab strip.
extern const char kExpandedTabStripName[];
extern const char kExpandedTabStripDescription[];

// Title and description for the flag to enable feed background refresh.
extern const char kFeedBackgroundRefreshName[];
extern const char kFeedBackgroundRefreshDescription[];

// Title and description for the flag to disable Discover-controlled
// foregrounding refresh.
extern const char kFeedDisableHotStartRefreshName[];
extern const char kFeedDisableHotStartRefreshDescription[];

// Title and description for the flag to enable feed experiment tagging.
extern const char kFeedExperimentTaggingName[];
extern const char kFeedExperimentTaggingDescription[];

// Title and description for the flag to enable feed invisible foreground
// refresh.
extern const char kFeedInvisibleForegroundRefreshName[];
extern const char kFeedInvisibleForegroundRefreshDescription[];

// Title and description for the flag to enable filling across affiliated
// websites.
extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

// Title and description for the flag to set the default Following feed sort
// type.
extern const char kFollowingFeedDefaultSortTypeName[];
extern const char kFollowingFeedDefaultSortTypeDescription[];

// Title and description for the flag to trigger the startup sign-in promo.
extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

// Title and description for the flag to show signed-out avatar on NTP.
extern const char kIdentityStatusConsistencyName[];
extern const char kIdentityStatusConsistencyDescription[];

// Title and description for the flag to enable skipping the internal impression
// limits of the Fullscreen Promos Manager.
extern const char kFullscreenPromosManagerSkipInternalLimitsName[];
extern const char kFullscreenPromosManagerSkipInternalLimitsDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

// Title and description for the flag that hides the content suggestion tiles
// from the NTP.
extern const char kHideContentSuggestionTilesName[];
extern const char kHideContentSuggestionTilesDescription[];

// Title and description for the flag to enable history-sync opt-in.
extern const char kHistorySyncOptInName[];
extern const char kHistorySyncOptInDescription[];

// Title and description for the flag to enable HTTPS-Only Mode setting.
extern const char kHttpsOnlyModeName[];
extern const char kHttpsOnlyModeDescription[];

// Title and description for the flag to enable revamped Incognito NTP page.
extern const char kIncognitoNtpRevampName[];
extern const char kIncognitoNtpRevampDescription[];

// Title and description for the flag to indicate the Account Storage error in
// the account cell when Sync is turned OFF.
extern const char kIndicateAccountStorageErrorInAccountCellName[];
extern const char kIndicateAccountStorageErrorInAccountCellDescription[];

// Title and description for the flag to indicate the identity error in
// the overflow menu.
extern const char kIndicateIdentityErrorInOverflowMenuName[];
extern const char kIndicateIdentityErrorInOverflowMenuDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable metrics collection for edit
// menu.
extern const char kIOSBrowserEditMenuMetricsName[];
extern const char kIOSBrowserEditMenuMetricsDescription[];

// Title and description for the flag to enable new API for browser edit menu.
extern const char kIOSCustomBrowserEditMenuName[];
extern const char kIOSCustomBrowserEditMenuDescription[];

// Title and description for the flag to enable partial translate.
extern const char kIOSEditMenuPartialTranslateName[];
extern const char kIOSEditMenuPartialTranslateDescription[];

// Title and description for the flag to enable Search With edit menu entry.
extern const char kIOSEditMenuSearchWithName[];
extern const char kIOSEditMenuSearchWithDescription[];

// Title and description for the flag to hide Search Web edit menu entry.
extern const char kIOSEditMenuHideSearchWebName[];
extern const char kIOSEditMenuHideSearchWebDescription[];

// Title and description for the flag to enable force translate when language
// detection failed.
extern const char kIOSForceTranslateEnabledName[];
extern const char kIOSForceTranslateEnabledDescription[];

// Title and description for the flag to enable the new iOS post-restore
// sign-in prompt.
extern const char kIOSNewPostRestoreExperienceName[];
extern const char kIOSNewPostRestoreExperienceDescription[];

// Title and description for the flag to enabled displaying and managing
// compromised, weak and reused credentials in the Password Manager.
extern const char kIOSPasswordCheckupName[];
extern const char kIOSPasswordCheckupDescription[];

// Title and description for the flag to split password settings and password
// management into two separate UIs.
extern const char kIOSPasswordUISplitName[];
extern const char kIOSPasswordUISplitDescription[];

// Title and description for the flag to display the Set Up List.
extern const char kIOSSetUpListName[];
extern const char kIOSSetUpListDescription[];

// Title and description for the flag to enable password bottom sheet on IOS.
extern const char kIOSPasswordBottomSheetName[];
extern const char kIOSPasswordBottomSheetDescription[];

// Title and description of the flag to enable client side new tab page
// experiments aimed at improving user retention.
extern const char kNewTabPageFieldTrialName[];
extern const char kNewTabPageFieldTrialDescription[];

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

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title and description for the flag that controls whether event breadcrumbs
// are captured.
extern const char kLogBreadcrumbsName[];
extern const char kLogBreadcrumbsDescription[];

// Title and Description for the flag that controls displaying the Magic Stack
// in the Home Surface,
extern const char kMagicStackName[];
extern const char kMagicStackDescription[];

// Title and description for the flag that controls sending metrickit non-crash
// reports.
extern const char kMetrickitNonCrashReportName[];
extern const char kMetrickitNonCrashReportDescription[];

// Title and description for the flag to enable Mixed Content auto-upgrading.
extern const char kMixedContentAutoupgradeName[];
extern const char kMixedContentAutoupgradeDescription[];

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
// Title and description for the flag used to test the newly
// implemented tabstrip.
extern const char kModernTabStripName[];
extern const char kModernTabStripDescription[];

// Title and description of the flag to enable the most visited tiles in the
// omnibox.
extern const char kMostVisitedTilesName[];
extern const char kMostVisitedTilesDescription[];

// Title and description of the flag to enable multiline gradient support in
// FadeTruncatingLabel.
extern const char kMultilineFadeTruncatingLabelName[];
extern const char kMultilineFadeTruncatingLabelDescription[];

// Title and description of the flag to enable the native Find in Page API
// for iOS 16 and later.
extern const char kNativeFindInPageName[];
extern const char kNativeFindInPageDescription[];

// Title and description for the flag to enable the new NTP omnibox layout.
extern const char kNewNTPOmniboxLayoutName[];
extern const char kNewNTPOmniboxLayoutDescription[];

// Title and description for the flag to enable the new overflow menu.
extern const char kNewOverflowMenuName[];
extern const char kNewOverflowMenuDescription[];

// Title and description for the flag to enable overflow menu customization
extern const char kOverflowMenuCustomizationName[];
extern const char kOverflowMenuCustomizationDescription[];

// Title and description for temporary bug fix to broken NTP view hierarhy.
// TODO(crbug.com/1262536): Remove this when fixed.
extern const char kNTPViewHierarchyRepairName[];
extern const char kNTPViewHierarchyRepairDescription[];

// Title and description for the flag to fetch contextual zero-prefix
// suggestions related to current page (on normal web pages).
extern const char kOmniboxFocusTriggersContextualWebZeroSuggestName[];
extern const char kOmniboxFocusTriggersContextualWebZeroSuggestDescription[];

// Title and description for the flag to fetch contextual zero-prefix
// suggestions on search results page.
extern const char kOmniboxFocusTriggersSRPZeroSuggestName[];
extern const char kOmniboxFocusTriggersSRPZeroSuggestDescription[];

// Title and description for fuzzy URL suggestions feature.
extern const char kOmniboxFuzzyUrlSuggestionsName[];
extern const char kOmniboxFuzzyUrlSuggestionsDescription[];

// Title and description for the flag to enable Omnibox HTTPS upgrades for
// schemeless navigations.
extern const char kOmniboxHttpsUpgradesName[];
extern const char kOmniboxHttpsUpgradesDescription[];

// Title and description for the flag to enable Omnibox Grouping implementation
// for ZPS.
extern const char kOmniboxGroupingFrameworkForZPSName[];
extern const char kOmniboxGroupingFrameworkForZPSDescription[];

// Title and description for the flag to enable Omnibox Grouping implementation
// for Typed Suggestions.
extern const char kOmniboxGroupingFrameworkForTypedSuggestionsName[];
extern const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[];

// Title and description for the flag to enable paste button in the omnibox's
// keyboard accessory.
extern const char kOmniboxKeyboardPasteButtonName[];
extern const char kOmniboxKeyboardPasteButtonDescription[];

// Title and description for local history zero-prefix suggestions beyond NTP.
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[];
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[];

// Title and description for the maximum number of URL matches.
extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

// Title and description for the flag to change the max number of ZPS
// matches in the omnibox popup.
extern const char kOmniboxMaxZPSMatchesName[];
extern const char kOmniboxMaxZPSMatchesDescription[];

// Title and description for the flag to inscrease the maximum number of lines
// for search suggestions.
extern const char kOmniboxMultilineSearchSuggestName[];
extern const char kOmniboxMultilineSearchSuggestDescription[];

// Title and description for the flag to swap Omnibox Textfield implementation
// to a new experimental one.
extern const char kOmniboxNewImplementationName[];
extern const char kOmniboxNewImplementationDescription[];

// Title and description for the flag to show most visited on SRP.
extern const char kOmniboxMostVisitedTilesOnSrpName[];
extern const char kOmniboxMostVisitedTilesOnSrpDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (non incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

// Title and description for omnibox on device tail suggest.
extern const char kOmniboxOnDeviceTailSuggestionsName[];
extern const char kOmniboxOnDeviceTailSuggestionsDescription[];

// Title and description for the flag to control Omnibox on-focus suggestions.
extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

// Title and description for assisted query stats param reporting.
extern const char kOmniboxReportAssistedQueryStatsName[];
extern const char kOmniboxReportAssistedQueryStatsDescription[];

// Title and description for searchbox stats flag.
extern const char kOmniboxReportSearchboxStatsName[];
extern const char kOmniboxReportSearchboxStatsDescription[];

// Title and description for tail suggestions in the omnibox.
extern const char kOmniboxTailSuggestName[];
extern const char kOmniboxTailSuggestDescription[];

// Title and description for the flag to change the max number of autocomplete
// matches in the omnibox popup.
extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

// Title and description for the use of in-memory zero-suggest caching.
extern const char kOmniboxZeroSuggestInMemoryCachingName[];
extern const char kOmniboxZeroSuggestInMemoryCachingDescription[];

// Title and description for the zero-suggest prefetching on the New Tab Page.
extern const char kOmniboxZeroSuggestPrefetchingName[];
extern const char kOmniboxZeroSuggestPrefetchingDescription[];

// Title and description for the zero-suggest prefetching on the Search Results
// Page.
extern const char kOmniboxZeroSuggestPrefetchingOnSRPName[];
extern const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[];

// Title and description for the zero-suggest prefetching on any Web Page.
extern const char kOmniboxZeroSuggestPrefetchingOnWebName[];
extern const char kOmniboxZeroSuggestPrefetchingOnWebDescription[];

// Title and description for the flag to force clipboard access to be
// asynchronous.
extern const char kOnlyAccessClipboardAsyncName[];
extern const char kOnlyAccessClipboardAsyncDescription[];

// Title and description for the flag to enable Optimization Guide debug logs.
extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

// Title and description for the flag to enable Optimization Guide new
// install-wide model store.
extern const char kOptimizationGuideInstallWideModelStoreName[];
extern const char kOptimizationGuideInstallWideModelStoreDescription[];

// Title and description for the flag enable download service to download in
// foreground.
extern const char kDownloadServiceForegroundSessionName[];
extern const char kDownloadServiceForegroundSessionDescription[];

// Title and description for the flag to enable optimization guide's push
// notifications
extern const char kOptimizationGuidePushNotificationClientName[];
extern const char kOptimizationGuidePushNotificationClientDescription[];

// Title and description for the flag to enable one tap experience for maps
// experience kit.
extern const char kOneTapForMapsName[];
extern const char kOneTapForMapsDescription[];

// Title and description for the flag to enable adding notes to password in
// settings.
extern const char kPasswordNotesWithBackupName[];
extern const char kPasswordNotesWithBackupDescription[];

// Title and description for the flag to enable PhishGuard password reuse
// detection.
extern const char kPasswordReuseDetectionName[];
extern const char kPasswordReuseDetectionDescription[];

// Title and description for the flag to enable chrome://policy/logs on iOS
extern const char kPolicyLogsPageIOSName[];
extern const char kPolicyLogsPageIOSDescription[];

// Title and description for the flag to have the Promos Manager use the FET as
// its impression tracking system.
extern const char kPromosManagerUsesFETName[];
extern const char kPromosManagerUsesFETDescription[];

// Title and description for the flag to enable PriceNotifications IPH to be
// alwayws be displayed.
extern const char kIPHPriceNotificationsWhileBrowsingName[];
extern const char kIPHPriceNotificationsWhileBrowsingDescription[];

// Title and description for the flag to enable the notification menu item in
// the settings menu.
extern const char kNotificationSettingsMenuItemName[];
extern const char kNotificationSettingsMenuItemDescription[];

// Title and description for the flag to native restore web states.
extern const char kRestoreSessionFromCacheName[];
extern const char kRestoreSessionFromCacheDescription[];

extern const char kRecordSnapshotSizeName[];
extern const char kRecordSnapshotSizeDescription[];

// Title and description for the flag to remove excess NTP tabs that don't have
// navigation history.
extern const char kRemoveExcessNTPsExperimentName[];
extern const char kRemoveExcessNTPsExperimentDescription[];

// Title and description for the flag to replace all sync-related UI with
// sign-in ones.
extern const char kReplaceSyncPromosWithSignInPromosName[];
extern const char kReplaceSyncPromosWithSignInPromosDescription[];

// Title and description for the flag that makes Safe Browsing available.
extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

// Title and description for the flag to enable real-time Safe Browsing lookups.
extern const char kSafeBrowsingRealTimeLookupName[];
extern const char kSafeBrowsingRealTimeLookupDescription[];

// Title and description for the flag to enable integration with the ScreenTime
// system.
extern const char kScreenTimeIntegrationName[];
extern const char kScreenTimeIntegrationDescription[];

// Title and description for the flag to show a sign-in promo if the user tries
// to use send-tab-to-self while being signed-out.
extern const char kSendTabToSelfSigninPromoName[];
extern const char kSendTabToSelfSigninPromoDescription[];

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

// Title and description for the flag to show the count of Inactive Tabs in the
// Tab Grid button.
extern const char kShowInactiveTabsCountName[];
extern const char kShowInactiveTabsCountDescription[];

// Title and description for the flag to add the Price Tracking destination
// (with Smart Sorting) to the new overflow menu.
extern const char kSmartSortingPriceTrackingDestinationName[];
extern const char kSmartSortingPriceTrackingDestinationDescription[];

// Title and description for th eflag to index Reading List items in Spotlight.
extern const char kSpotlightReadingListSourceName[];
extern const char kSpotlightReadingListSourceDescription[];

// Title and description for the flag to enable the Share Chrome App action
// in the new overflow menu.
extern const char kNewOverflowMenuShareChromeActionName[];
extern const char kNewOverflowMenuShareChromeActionDescription[];

// Title and description for the flag to enable the Start Surface.
extern const char kStartSurfaceName[];
extern const char kStartSurfaceDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag to control if history's segments should
// include foreign visits from syncing devices.
extern const char kSyncSegmentsDataName[];
extern const char kSyncSegmentsDataDescription[];

// Title and description for the flag to synthesize native restore web states.
extern const char kSynthesizedRestoreSessionName[];
extern const char kSynthesizedRestoreSessionDescription[];

// Title and description for the flag to enable the Sync History data type.
extern const char kSyncEnableHistoryDataTypeName[];
extern const char kSyncEnableHistoryDataTypeDescription[];

// Title and description for the flag to enable Sync standalone invalidations.
extern const char kSyncInvalidationsName[];
extern const char kSyncInvalidationsDescription[];

// Title and description for the flag to enable Sync standalone invalidations
// for the Wallet and Offer data types.
extern const char kSyncInvalidationsWalletAndOfferName[];
extern const char kSyncInvalidationsWalletAndOfferDescription[];

// Title and description for the flag to enable TFLite for language detection.
extern const char kTFLiteLanguageDetectionName[];
extern const char kTFLiteLanguageDetectionDescription[];

// Title and description for the flag to compute both TFLite and CLD3 detection
// and ignore TFLite one.
extern const char kTFLiteLanguageDetectionIgnoreName[];
extern const char kTFLiteLanguageDetectionIgnoreDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable using Lens to search using
// the device camera from the home screen widget.
extern const char kEnableLensInHomeScreenWidgetName[];
extern const char kEnableLensInHomeScreenWidgetDescription[];

// Title and description for the flag to enable using Lens to search using
// the device camera from the keyboard.
extern const char kEnableLensInKeyboardName[];
extern const char kEnableLensInKeyboardDescription[];

// Title and description for the flag to enable using Lens to search using
// the device camera from the ntp.
extern const char kEnableLensInNTPName[];
extern const char kEnableLensInNTPDescription[];

// Title and description for the flag to enable using alternate Lens context
// menu string.
extern const char kEnableLensContextMenuAltTextName[];
extern const char kEnableLensContextMenuAltTextDescription[];

// Title and description for the flag to enable using Lens to search using
// copied images in the omnibox.
extern const char kEnableLensInOmniboxCopiedImageName[];
extern const char kEnableLensInOmniboxCopiedImageDescription[];

// Title and description for the flag to enable session serialization
// optimizations (go/bling-session-restoration).
extern const char kEnableSessionSerializationOptimizationsName[];
extern const char kEnableSessionSerializationOptimizationsDescription[];

// Title and description for the flag to enable the follow up of the SF Symbols.
extern const char kSFSymbolsFollowUpName[];
extern const char kSFSymbolsFollowUpDescription[];

// Title and description for the flag to sort the tab by recency in the TabGrid.
extern const char kTabGridRecencySortName[];
extern const char kTabGridRecencySortDescription[];

// Title and description for the flag to enable the new transitions in the
// TabGrid.
extern const char kTabGridNewTransitionsName[];
extern const char kTabGridNewTransitionsDescription[];

// Title and description for the flag to determine tab inactivity in the
// TabGrid.
extern const char kTabInactivityThresholdName[];
extern const char kTabInactivityThresholdDescription[];

// Title and description for the flag to enable using the
// loadSimulatedRequest:responseHTMLString: API for displaying error pages in
// CRWWKNavigationHandler.
extern const char kUseLoadSimulatedRequestForOfflinePageName[];
extern const char kUseLoadSimulatedRequestForOfflinePageDescription[];

// Title and description for the flag to control the maximum wait time (in
// seconds) for a response from the Account Capabilities API.
extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to control whether to send discover
// feedback to a new product destination
extern const char kWebFeedFeedbackRerouteName[];
extern const char kWebFeedFeedbackRerouteDescription[];

// Title and description for the flag to tie the default text zoom level to
// the dynamic type setting.
extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

// Title and description for the flag to enable a different method of zooming
// web pages.
extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

// Title and description for the flag to (re)-enable text zoom on iPad.
extern const char kWebPageTextZoomIPadName[];
extern const char kWebPageTextZoomIPadDescription[];

// Title and description for the flag to enable What's New.
extern const char kWhatsNewIOSName[];
extern const char kWhatsNewIOSDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

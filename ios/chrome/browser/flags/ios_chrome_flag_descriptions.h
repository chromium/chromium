// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "base/debug/debugging_buildflags.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.
//
// Comments are not necessary. The contents of the strings (which appear in the
// UI) should be good enough documentation for what flags do and when they
// apply. If they aren't, fix them.
//
// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

extern const char kAddAddressManuallyName[];
extern const char kAddAddressManuallyDescription[];

extern const char kAppBackgroundRefreshName[];
extern const char kAppBackgroundRefreshDescription[];

extern const char kAutofillAcrossIframesName[];
extern const char kAutofillAcrossIframesDescription[];

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

extern const char kAutofillDisableDefaultSaveCardFixFlowDetectionName[];
extern const char kAutofillDisableDefaultSaveCardFixFlowDetectionDescription[];

extern const char kAutofillDisableProfileUpdatesName[];
extern const char kAutofillDisableProfileUpdatesDescription[];

extern const char kAutofillDisableSilentProfileUpdatesName[];
extern const char kAutofillDisableSilentProfileUpdatesDescription[];

extern const char kAutofillEnableAllowlistForBmoCardCategoryBenefitsName[];
extern const char
    kAutofillEnableAllowlistForBmoCardCategoryBenefitsDescription[];

extern const char kAutofillEnableCardBenefitsForAmericanExpressName[];
extern const char kAutofillEnableCardBenefitsForAmericanExpressDescription[];

extern const char kAutofillEnableCardBenefitsForBmoName[];
extern const char kAutofillEnableCardBenefitsForBmoDescription[];

extern const char kAutofillEnableCardBenefitsSyncName[];
extern const char kAutofillEnableCardBenefitsSyncDescription[];

extern const char kAutofillEnableCardInfoRuntimeRetrievalName[];
extern const char kAutofillEnableCardInfoRuntimeRetrievalDescription[];

// Title and description for the flag to enable dynamically loading the fields
// for address input based on the country value.
extern const char kAutofillEnableDynamicallyLoadingFieldsForAddressInputName[];
extern const char
    kAutofillEnableDynamicallyLoadingFieldsForAddressInputDescription[];

extern const char kAutofillEnableFlatRateCardBenefitsFromCurinosName[];
extern const char kAutofillEnableFlatRateCardBenefitsFromCurinosDescription[];

extern const char kAutofillEnableFpanRiskBasedAuthenticationName[];
extern const char kAutofillEnableFpanRiskBasedAuthenticationDescription[];

extern const char kAutofillEnableLogFormEventsToAllParsedFormTypesName[];
extern const char kAutofillEnableLogFormEventsToAllParsedFormTypesDescription[];

extern const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName[];
extern const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription
        [];

extern const char kAutofillEnablePrefetchingRiskDataForRetrievalName[];
extern const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[];

extern const char kAutofillEnableRankingFormulaAddressProfilesName[];
extern const char kAutofillEnableRankingFormulaAddressProfilesDescription[];

extern const char kAutofillEnableRankingFormulaCreditCardsName[];
extern const char kAutofillEnableRankingFormulaCreditCardsDescription[];

extern const char kAutofillEnableSupportForHomeAndWorkName[];
extern const char kAutofillEnableSupportForHomeAndWorkDescription[];

extern const char kAutofillIsolatedWorldForJavascriptIOSName[];
extern const char kAutofillIsolatedWorldForJavascriptIOSDescription[];

extern const char kAutofillPaymentsSheetV2Name[];
extern const char kAutofillPaymentsSheetV2Description[];

extern const char kPasswordSuggestionBottomSheetV2Name[];
extern const char kPasswordSuggestionBottomSheetV2Description[];

extern const char kAutofillLocalSaveCardBottomSheetName[];
extern const char kAutofillLocalSaveCardBottomSheetDescription[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

extern const char kAutofillSaveCardBottomSheetName[];
extern const char kAutofillSaveCardBottomSheetDescription[];

extern const char kAutofillShowManualFillForVirtualCardsName[];
extern const char kAutofillShowManualFillForVirtualCardsDescription[];

extern const char kAutofillStickyInfobarName[];
extern const char kAutofillStickyInfobarDescription[];

extern const char kAutofillThrottleDocumentFormScanName[];
extern const char kAutofillThrottleDocumentFormScanDescription[];

extern const char kAutofillThrottleFilteredDocumentFormScanName[];
extern const char kAutofillThrottleFilteredDocumentFormScanDescription[];

extern const char kAutofillUnmaskCardRequestTimeoutName[];
extern const char kAutofillUnmaskCardRequestTimeoutDescription[];

extern const char kAutofillUploadCardRequestTimeoutName[];
extern const char kAutofillUploadCardRequestTimeoutDescription[];

extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

extern const char kAutofillVcnEnrollRequestTimeoutName[];
extern const char kAutofillVcnEnrollRequestTimeoutDescription[];

extern const char kAutofillVcnEnrollStrikeExpiryTimeName[];
extern const char kAutofillVcnEnrollStrikeExpiryTimeDescription[];

extern const char kBestFeaturesScreenInFirstRunName[];
extern const char kBestFeaturesScreenInFirstRunDescription[];

extern const char kBestOfAppFREName[];
extern const char kBestOfAppFREDescription[];

extern const char kBlueDotOnToolsMenuButtonName[];
extern const char kBlueDotOnToolsMenuButtonDescription[];

extern const char kBottomOmniboxDefaultSettingName[];
extern const char kBottomOmniboxDefaultSettingDescription[];

extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

extern const char kCollaborationMessagingName[];
extern const char kCollaborationMessagingDescription[];

extern const char kColorfulTabGroupName[];
extern const char kColorfulTabGroupDescription[];

extern const char kContainedTabGroupName[];
extern const char kContainedTabGroupDescription[];

extern const char kContentNotificationExperimentName[];
extern const char kContentNotificationExperimentDescription[];

extern const char kContentNotificationProvisionalIgnoreConditionsName[];
extern const char kContentNotificationProvisionalIgnoreConditionsDescription[];

extern const char kContentPushNotificationsName[];
extern const char kContentPushNotificationsDescription[];

extern const char kContextualPanelName[];
extern const char kContextualPanelDescription[];

extern const char kCredentialProviderAutomaticPasskeyUpgradeName[];
extern const char kCredentialProviderAutomaticPasskeyUpgradeDescription[];

extern const char kCredentialProviderPasskeyPRFName[];
extern const char kCredentialProviderPasskeyPRFDescription[];

extern const char kCredentialProviderPerformanceImprovementsName[];
extern const char kCredentialProviderPerformanceImprovementsDescription[];

extern const char kChromeStartupParametersAsyncName[];
extern const char kChromeStartupParametersAsyncDescription[];

extern const char kAppleCalendarExperienceKitName[];
extern const char kAppleCalendarExperienceKitDescription[];

extern const char kPhoneNumberName[];
extern const char kPhoneNumberDescription[];

extern const char kMeasurementsName[];
extern const char kMeasurementsDescription[];

extern const char kEnableExpKitTextClassifierDateName[];
extern const char kEnableExpKitTextClassifierDateDescription[];

extern const char kEnableExpKitTextClassifierAddressName[];
extern const char kEnableExpKitTextClassifierAddressDescription[];

extern const char kEnableExpKitTextClassifierPhoneNumberName[];
extern const char kEnableExpKitTextClassifierPhoneNumberDescription[];

extern const char kEnableExpKitTextClassifierEmailName[];
extern const char kEnableExpKitTextClassifierEmailDescription[];

extern const char kEnableFamilyLinkControlsName[];
extern const char kEnableFamilyLinkControlsDescription[];

extern const char kCredentialProviderExtensionPromoName[];
extern const char kCredentialProviderExtensionPromoDescription[];

extern const char kDataSharingName[];
extern const char kDataSharingDescription[];

extern const char kDataSharingDebugLogsName[];
extern const char kDataSharingDebugLogsDescription[];

extern const char kDataSharingJoinOnlyName[];
extern const char kDataSharingJoinOnlyDescription[];

extern const char kDefaultBrowserBannerPromoName[];
extern const char kDefaultBrowserBannerPromoDescription[];

extern const char kDefaultBrowserTriggerCriteriaExperimentName[];
extern const char kDefaultBrowserTriggerCriteriaExperimentDescription[];

extern const char kDeprecateFeedHeaderExperimentName[];
extern const char kDeprecateFeedHeaderExperimentDescription[];

extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

extern const char kDownloadAutoDeletionName[];
extern const char kDownloadAutoDeletionDescription[];

extern const char kDownloadedPDFOpeningName[];
extern const char kDownloadedPDFOpeningDescription[];

extern const char kEnableCompromisedPasswordsMutingName[];
extern const char kEnableCompromisedPasswordsMutingDescription[];

extern const char kEnableFeedHeaderSettingsName[];
extern const char kEnableFeedHeaderSettingsDescription[];

extern const char kEnableFeedCardMenuSignInPromoName[];
extern const char kEnableFeedCardMenuSignInPromoDescription[];

extern const char kEnableTraitCollectionRegistrationName[];
extern const char kEnableTraitCollectionRegistrationDescription[];

extern const char kDisableLensCameraName[];
extern const char kDisableLensCameraDescription[];

extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableASWebAuthenticationSessionName[];
extern const char kEnableASWebAuthenticationSessionDescription[];

extern const char kEnableDiscoverFeedDiscoFeedEndpointName[];
extern const char kEnableDiscoverFeedDiscoFeedEndpointDescription[];

extern const char kEnableFeedAblationName[];
extern const char kEnableFeedAblationDescription[];

extern const char kEnableiPadFeedGhostCardsName[];
extern const char kEnableiPadFeedGhostCardsDescription[];

extern const char kEnableReadingListAccountStorageName[];
extern const char kEnableReadingListAccountStorageDescription[];

extern const char kEnableReadingListSignInPromoName[];
extern const char kEnableReadingListSignInPromoDescription[];

extern const char kEnableSignedOutViewDemotionName[];
extern const char kEnableSignedOutViewDemotionDescription[];

extern const char kEnableIdentityInAuthErrorName[];
extern const char kEnableIdentityInAuthErrorDescription[];

extern const char kEnableVariationsGoogleGroupFilteringName[];
extern const char kEnableVariationsGoogleGroupFilteringDescription[];

extern const char kEnhancedCalendarName[];
extern const char kEnhancedCalendarDescription[];

extern const char kEnhancedSafeBrowsingPromoName[];
extern const char kEnhancedSafeBrowsingPromoDescription[];

extern const char kEnterpriseRealtimeEventReportingOnIOSName[];
extern const char kEnterpriseRealtimeEventReportingOnIOSDescription[];

extern const char kFeedBackgroundRefreshName[];
extern const char kFeedBackgroundRefreshDescription[];

extern const char kFeedSwipeInProductHelpName[];
extern const char kFeedSwipeInProductHelpDescription[];

extern const char kFeedbackIncludeVariationsName[];
extern const char kFeedbackIncludeVariationsDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

extern const char kAnimatedDefaultBrowserPromoInFREName[];
extern const char kAnimatedDefaultBrowserPromoInFREDescription[];

extern const char kFeedbackIncludeGWSVariationsName[];
extern const char kFeedbackIncludeGWSVariationsDescription[];

extern const char kFullscreenImprovementName[];
extern const char kFullscreenImprovementDescription[];

extern const char kFullscreenPromosManagerSkipInternalLimitsName[];
extern const char kFullscreenPromosManagerSkipInternalLimitsDescription[];

extern const char kFullscreenTransitionName[];
extern const char kFullscreenTransitionDescription[];

extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

extern const char kHomeMemoryImprovementsName[];
extern const char kHomeMemoryImprovementsDescription[];

extern const char kHttpsUpgradesName[];
extern const char kHttpsUpgradesDescription[];

extern const char kIdentityDiscAccountMenuName[];
extern const char kIdentityDiscAccountMenuDescription[];

extern const char kIdentityConfirmationSnackbarName[];
extern const char kIdentityConfirmationSnackbarDescription[];

extern const char kImportPasswordsFromSafariName[];
extern const char kImportPasswordsFromSafariDescription[];

extern const char kIndicateIdentityErrorInOverflowMenuName[];
extern const char kIndicateIdentityErrorInOverflowMenuDescription[];

extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

extern const char kIOSBrowserEditMenuMetricsName[];
extern const char kIOSBrowserEditMenuMetricsDescription[];

extern const char kIOSDockingPromoName[];
extern const char kIOSDockingPromoDescription[];

extern const char kIOSEnableDeleteAllSavedCredentialsName[];
extern const char kIOSEnableDeleteAllSavedCredentialsDescription[];

extern const char kIOSEnablePasswordManagerTrustedVaultWidgetName[];
extern const char kIOSEnablePasswordManagerTrustedVaultWidgetDescription[];

extern const char kIOSEnableRealtimeEventReportingName[];
extern const char kIOSEnableRealtimeEventReportingDescription[];

extern const char kIOSKeyboardAccessoryUpgradeName[];
extern const char kIOSKeyboardAccessoryUpgradeDescription[];

extern const char kIOSKeyboardAccessoryUpgradeForIPadName[];
extern const char kIOSKeyboardAccessoryUpgradeForIPadDescription[];

extern const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuName[];
extern const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuDescription[];

extern const char kIOSOneTapMiniMapRemoveSectionBreaksName[];
extern const char kIOSOneTapMiniMapRemoveSectionBreaksDescription[];

extern const char kIOSOneTapMiniMapRestrictionsName[];
extern const char kIOSOneTapMiniMapRestrictionsDescription[];

extern const char kIOSPasskeysM2Name[];
extern const char kIOSPasskeysM2Description[];

extern const char kIOSPasswordBottomSheetAutofocusName[];
extern const char kIOSPasswordBottomSheetAutofocusDescription[];

extern const char kIOSProactivePasswordGenerationBottomSheetName[];
extern const char kIOSProactivePasswordGenerationBottomSheetDescription[];

extern const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[];
extern const char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[];

extern const char kIOSQuickDeleteName[];
extern const char kIOSQuickDeleteDescription[];

extern const char kIOSChooseFromDriveName[];
extern const char kIOSChooseFromDriveDescription[];

extern const char kIOSChooseFromDriveSimulatedClickName[];
extern const char kIOSChooseFromDriveSimulatedClickDescription[];

extern const char kIOSManageAccountStorageName[];
extern const char kIOSManageAccountStorageDescription[];

extern const char kIOSSaveToPhotosImprovementsName[];
extern const char kIOSSaveToPhotosImprovementsDescription[];

extern const char kIOSEnterpriseRealtimeUrlFilteringName[];
extern const char kIOSEnterpriseRealtimeUrlFilteringDescription[];

extern const char kNewShareExtensionName[];
extern const char kNewShareExtensionDescription[];

extern const char kNewTabPageFieldTrialName[];
extern const char kNewTabPageFieldTrialDescription[];

extern const char kNonModalSignInPromoName[];
extern const char kNonModalSignInPromoDescription[];

extern const char kNotificationCollisionManagementName[];
extern const char kNotificationCollisionManagementDescription[];

extern const char kIOSSharedHighlightingColorChangeName[];
extern const char kIOSSharedHighlightingColorChangeDescription[];

extern const char kIOSSharedHighlightingAmpName[];
extern const char kIOSSharedHighlightingAmpDescription[];

extern const char kIOSSoftLockName[];
extern const char kIOSSoftLockDescription[];

extern const char kIOSStartTimeBrowserBackgroundRemediationsName[];
extern const char kIOSStartTimeBrowserBackgroundRemediationsDescription[];

extern const char kIOSStartTimeStartupRemediationsName[];
extern const char kIOSStartTimeStartupRemediationsDescription[];

extern const char kIOSReactivationNotificationsName[];
extern const char kIOSReactivationNotificationsDescription[];

extern const char kIOSExpandedTipsName[];
extern const char kIOSExpandedTipsDescription[];

extern const char kIOSProvidesAppNotificationSettingsName[];
extern const char kIOSProvidesAppNotificationSettingsDescription[];

extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName[];
extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription[];

extern const char kLensClearcutBackgroundUploadEnabledName[];
extern const char kLensClearcutBackgroundUploadEnabledDescription[];

extern const char kLensClearcutLoggerFastQosEnabledName[];
extern const char kLensClearcutLoggerFastQosEnabledDescription[];

extern const char kLensExactMatchesEnabledName[];
extern const char kLensExactMatchesEnabledDescription[];

extern const char kLensFetchSrpApiEnabledName[];
extern const char kLensFetchSrpApiEnabledDescription[];

extern const char kLensFiltersAblationModeEnabledName[];
extern const char kLensFiltersAblationModeEnabledDescription[];

extern const char kLensGestureTextSelectionDisabledName[];
extern const char kLensGestureTextSelectionDisabledDescription[];

extern const char kLensInkMultiSampleModeDisabledName[];
extern const char kLensInkMultiSampleModeDisabledDescription[];

extern const char kLensLoadAIMInLensResultPageName[];
extern const char kLensLoadAIMInLensResultPageDescription[];

extern const char kLensOverlayForceShowOnboardingScreenName[];
extern const char kLensOverlayForceShowOnboardingScreenDescription[];

extern const char kLensOverlayAlternativeOnboardingName[];
extern const char kLensOverlayAlternativeOnboardingDescription[];

extern const char kLensOverlayDisableIPHPanGestureName[];
extern const char kLensOverlayDisableIPHPanGestureDescription[];

extern const char kLensOverlayDisablePriceInsightsName[];
extern const char kLensOverlayDisablePriceInsightsDescription[];

extern const char kLensOverlayPriceInsightsCounterfactualName[];
extern const char kLensOverlayPriceInsightsCounterfactualDescription[];

extern const char kLensOverlayEnableIPadCompatibilityName[];
extern const char kLensOverlayEnableIPadCompatibilityDescription[];

extern const char kLensOverlayEnableLandscapeCompatibilityName[];
extern const char kLensOverlayEnableLandscapeCompatibilityDescription[];

extern const char kLensOverlayEnableLVFEscapeHatchName[];
extern const char kLensOverlayEnableLVFEscapeHatchDescription[];

extern const char kLensOverlayEnableLocationBarEntrypointName[];
extern const char kLensOverlayEnableLocationBarEntrypointDescription[];

extern const char kLensOverlayEnableLocationBarEntrypointOnSRPName[];
extern const char kLensOverlayEnableLocationBarEntrypointOnSRPDescription[];

extern const char kLensOverlayEnableSameTabNavigationName[];
extern const char kLensOverlayEnableSameTabNavigationDescription[];

extern const char kLensOverlayNavigationHistoryName[];
extern const char kLensOverlayNavigationHistoryDescription[];

extern const char kLensPrewarmHardStickinessInInputSelectionName[];
extern const char kLensPrewarmHardStickinessInInputSelectionDescription[];

extern const char kLensPrewarmHardStickinessInQueryFormulationName[];
extern const char kLensPrewarmHardStickinessInQueryFormulationDescription[];

extern const char kLensQRCodeParsingFixName[];
extern const char kLensQRCodeParsingFixDescription[];

extern const char kLensSingleTapTextSelectionDisabledName[];
extern const char kLensSingleTapTextSelectionDisabledDescription[];

extern const char kLensTranslateToggleModeEnabledName[];
extern const char kLensTranslateToggleModeEnabledDescription[];

extern const char kLensUnaryApisWithHttpTransportEnabledName[];
extern const char kLensUnaryApisWithHttpTransportEnabledDescription[];

extern const char kLensUnaryApiSalientTextEnabledName[];
extern const char kLensUnaryApiSalientTextEnabledDescription[];

extern const char kLensUnaryClientDataHeaderEnabledName[];
extern const char kLensUnaryClientDataHeaderEnabledDescription[];

extern const char kLensUnaryHttpTransportEnabledName[];
extern const char kLensUnaryHttpTransportEnabledDescription[];

extern const char kLensVsintParamEnabledName[];
extern const char kLensVsintParamEnabledDescription[];

extern const char kLensWebPageLoadOptimizationEnabledName[];
extern const char kLensWebPageLoadOptimizationEnabledDescription[];

extern const char kLinkedServicesSettingIosName[];
extern const char kLinkedServicesSettingIosDescription[];

extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

extern const char kMagicStackName[];
extern const char kMagicStackDescription[];

// Title and description for the flag to enable UMA log uploads after each page
// in the FRE.
extern const char kManualLogUploadsInFREName[];
extern const char kManualLogUploadsInFREDescription[];

// Title and description for the flag that controls sending metrickit non-crash
// reports.
extern const char kMetrickitNonCrashReportName[];
extern const char kMetrickitNonCrashReportDescription[];

extern const char kMostVisitedTilesHorizontalRenderGroupName[];
extern const char kMostVisitedTilesHorizontalRenderGroupDescription[];

extern const char kNativeFindInPageName[];
extern const char kNativeFindInPageDescription[];

extern const char kNTPViewHierarchyRepairName[];
extern const char kNTPViewHierarchyRepairDescription[];

extern const char kOmniboxHttpsUpgradesName[];
extern const char kOmniboxHttpsUpgradesDescription[];

extern const char kOmniboxInspireMeSignedOutName[];
extern const char kOmniboxInspireMeSignedOutDescription[];

extern const char kOmniboxGroupingFrameworkForZPSName[];
extern const char kOmniboxGroupingFrameworkForZPSDescription[];

extern const char kOmniboxGroupingFrameworkForTypedSuggestionsName[];
extern const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[];

extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[];
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxMaxZPSMatchesName[];
extern const char kOmniboxMaxZPSMatchesDescription[];

extern const char kOmniboxMiaZps[];
extern const char kOmniboxMiaZpsDescription[];

extern const char kOmniboxMlLogUrlScoringSignalsName[];
extern const char kOmniboxMlLogUrlScoringSignalsDescription[];

extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[];
extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[];

extern const char kOmniboxMlUrlScoreCachingName[];
extern const char kOmniboxMlUrlScoreCachingDescription[];

extern const char kOmniboxMlUrlScoringName[];
extern const char kOmniboxMlUrlScoringDescription[];

extern const char kOmniboxMlUrlScoringModelName[];
extern const char kOmniboxMlUrlScoringModelDescription[];

extern const char kOmniboxMlUrlSearchBlendingName[];
extern const char kOmniboxMlUrlSearchBlendingDescription[];

extern const char kOmniboxMobileParityUpdateName[];
extern const char kOmniboxMobileParityUpdateDescription[];

extern const char kOmniboxOnClobberFocusTypeOnIOSName[];
extern const char kOmniboxOnClobberFocusTypeOnIOSDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

extern const char kOmniboxOnDeviceTailSuggestionsName[];
extern const char kOmniboxOnDeviceTailSuggestionsDescription[];

extern const char kOmniboxSuggestionAnswerMigrationName[];
extern const char kOmniboxSuggestionAnswerMigrationDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxZeroSuggestInMemoryCachingName[];
extern const char kOmniboxZeroSuggestInMemoryCachingDescription[];

extern const char kOmniboxZeroSuggestPrefetchingName[];
extern const char kOmniboxZeroSuggestPrefetchingDescription[];

extern const char kOmniboxZeroSuggestPrefetchingOnSRPName[];
extern const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[];

extern const char kOmniboxZeroSuggestPrefetchingOnWebName[];
extern const char kOmniboxZeroSuggestPrefetchingOnWebDescription[];

extern const char kOnlyAccessClipboardAsyncName[];
extern const char kOnlyAccessClipboardAsyncDescription[];

extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

extern const char kDownloadServiceForegroundSessionName[];
extern const char kDownloadServiceForegroundSessionDescription[];

extern const char kOptimizationGuidePushNotificationClientName[];
extern const char kOptimizationGuidePushNotificationClientDescription[];

extern const char kOneTapForMapsName[];
extern const char kOneTapForMapsDescription[];

extern const char kPageActionMenuName[];
extern const char kPageActionMenuDescription[];

extern const char kGLICPromoConsentName[];
extern const char kGLICPromoConsentDescription[];

extern const char kPageContentAnnotationsName[];
extern const char kPageContentAnnotationsDescription[];

extern const char kPageContentAnnotationsPersistSalientImageMetadataName[];
extern const char
    kPageContentAnnotationsPersistSalientImageMetadataDescription[];

extern const char kPageContentAnnotationsRemotePageMetadataName[];
extern const char kPageContentAnnotationsRemotePageMetadataDescription[];

extern const char kPageImageServiceSalientImageName[];
extern const char kPageImageServiceSalientImageDescription[];

extern const char kPageInfoLastVisitedIOSName[];
extern const char kPageInfoLastVisitedIOSDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kPasswordFormClientsideClassifierName[];
extern const char kPasswordFormClientsideClassifierDescription[];

extern const char kPasswordReuseDetectionName[];
extern const char kPasswordReuseDetectionDescription[];

extern const char kPasswordSharingName[];
extern const char kPasswordSharingDescription[];

extern const char kPriceTrackingPromoName[];
extern const char kPriceTrackingPromoDescription[];

extern const char kPrivacyGuideIosName[];
extern const char kPrivacyGuideIosDescription[];

extern const char kProvisionalNotificationAlertName[];
extern const char kProvisionalNotificationAlertDescription[];

extern const char kIpadZpsSuggestionMatchesLimitName[];
extern const char kIpadZpsSuggestionMatchesLimitDescription[];

extern const char kIPHPriceNotificationsWhileBrowsingName[];
extern const char kIPHPriceNotificationsWhileBrowsingDescription[];

extern const char kNotificationSettingsMenuItemName[];
extern const char kNotificationSettingsMenuItemDescription[];

extern const char kReaderModeName[];
extern const char kReaderModeDescription[];

extern const char kReaderModeDebugInfoName[];
extern const char kReaderModeDebugInfoDescription[];

extern const char kReaderModePageEligibilityHeuristicName[];
extern const char kReaderModePageEligibilityHeuristicDescription[];

// Title and description for the flag to refactor the toolbarsSize.
extern const char kRefactorToolbarsSizeName[];
extern const char kRefactorToolbarsSizeDescription[];

extern const char kRemoveExcessNTPsExperimentName[];
extern const char kRemoveExcessNTPsExperimentDescription[];

extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

extern const char kSafeBrowsingLocalListsUseSBv5Name[];
extern const char kSafeBrowsingLocalListsUseSBv5Description[];

extern const char kSafeBrowsingRealTimeLookupName[];
extern const char kSafeBrowsingRealTimeLookupDescription[];

extern const char kSafeBrowsingTrustedURLName[];
extern const char kSafeBrowsingTrustedURLDescription[];

extern const char kSafetyCheckMagicStackName[];
extern const char kSafetyCheckMagicStackDescription[];

extern const char kSafetyCheckNotificationsName[];
extern const char kSafetyCheckNotificationsDescription[];

extern const char kScreenTimeIntegrationName[];
extern const char kScreenTimeIntegrationDescription[];

extern const char kSegmentationPlatformEphemeralCardRankerName[];
extern const char kSegmentationPlatformEphemeralCardRankerDescription[];

extern const char kSegmentationPlatformIosModuleRankerCachingName[];
extern const char kSegmentationPlatformIosModuleRankerCachingDescription[];

extern const char kSegmentationPlatformIosModuleRankerName[];
extern const char kSegmentationPlatformIosModuleRankerDescription[];

extern const char kSegmentationPlatformIosModuleRankerSplitBySurfaceName[];
extern const char
    kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription[];

extern const char kSegmentationPlatformTipsEphemeralCardName[];
extern const char kSegmentationPlatformTipsEphemeralCardDescription[];

extern const char kSegmentedDefaultBrowserPromoName[];
extern const char kSegmentedDefaultBrowserPromoDescription[];

extern const char kSendTabToSelfIOSPushNotificationsName[];
extern const char kSendTabToSelfIOSPushNotificationsDescription[];

extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

extern const char kSeparateProfilesForManagedAccountsName[];
extern const char kSeparateProfilesForManagedAccountsDescription[];

extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

extern const char kShopCardName[];
extern const char kShopCardDescription[];

extern const char kShopCardImpressionLimitsName[];
extern const char kShopCardImpressionLimitsDescription[];

extern const char kSetUpListShortenedDurationName[];
extern const char kSetUpListShortenedDurationDescription[];

extern const char kSetUpListWithoutSignInItemName[];
extern const char kSetUpListWithoutSignInItemDescription[];

extern const char kShareInWebContextMenuIOSName[];
extern const char kShareInWebContextMenuIOSDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kSignInButtonNoAvatarName[];
extern const char kSignInButtonNoAvatarDescription[];

extern const char kNTPBackgroundCustomizationName[];
extern const char kNTPBackgroundCustomizationDescription[];

extern const char kNtpAlphaBackgroundCollectionsName[];
extern const char kNtpAlphaBackgroundCollectionsDescription[];

extern const char kSpotlightNeverRetainIndexName[];
extern const char kSpotlightNeverRetainIndexDescription[];

extern const char kStartSurfaceName[];
extern const char kStartSurfaceDescription[];

extern const char kSuggestStrongPasswordInAddPasswordName[];
extern const char kSuggestStrongPasswordInAddPasswordDescription[];

extern const char kSupervisedUserBlockInterstitialV3Name[];
extern const char kSupervisedUserBlockInterstitialV3Description[];

extern const char kSupervisedUserLocalWebApprovalsName[];
extern const char kSupervisedUserLocalWebApprovalsDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncTrustedVaultInfobarImprovementsName[];
extern const char kSyncTrustedVaultInfobarImprovementsDescription[];

extern const char kSyncTrustedVaultInfobarMessageImprovementsName[];
extern const char kSyncTrustedVaultInfobarMessageImprovementsDescription[];

extern const char kTabGroupIndicatorName[];
extern const char kTabGroupIndicatorDescription[];

extern const char kTabGroupSyncName[];
extern const char kTabGroupSyncDescription[];

extern const char kTFLiteLanguageDetectionName[];
extern const char kTFLiteLanguageDetectionDescription[];

extern const char kThemeColorInTopToolbarName[];
extern const char kThemeColorInTopToolbarDescription[];

extern const char kEnableLensInOmniboxCopiedImageName[];
extern const char kEnableLensInOmniboxCopiedImageDescription[];

extern const char kEnableLensOverlayName[];
extern const char kEnableLensOverlayDescription[];

extern const char kEnableLensViewFinderUnifiedExperienceName[];
extern const char kEnableLensViewFinderUnifiedExperienceDescription[];

extern const char kEnableLensContextMenuUnifiedExperienceName[];
extern const char kEnableLensContextMenuUnifiedExperienceDescription[];

extern const char kTabGridNewTransitionsName[];
extern const char kTabGridNewTransitionsDescription[];

extern const char kTabResumptionName[];
extern const char kTabResumptionDescription[];

extern const char kTabResumptionImagesName[];
extern const char kTabResumptionImagesDescription[];

extern const char kUpdatedFRESequenceName[];
extern const char kUpdatedFRESequenceDescription[];

extern const char kUseFeedEligibilityServiceName[];
extern const char kUseFeedEligibilityServiceDescription[];

extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebFeedFeedbackRerouteName[];
extern const char kWebFeedFeedbackRerouteDescription[];

extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

extern const char kWebPageTextZoomIPadName[];
extern const char kWebPageTextZoomIPadDescription[];

extern const char kWelcomeBackInFirstRunName[];
extern const char kWelcomeBackInFirstRunDescription[];

extern const char kWidgetsForMultiprofileName[];
extern const char kWidgetsForMultiprofileDescription[];

extern const char kYoutubeIncognitoName[];
extern const char kYoutubeIncognitoDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

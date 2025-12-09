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
// Do not add comments or pre-processor lines. The contents of the strings
// (which appear in the UI) should be good enough documentation for what flags
// do and when they apply. If they aren't, fix them.
//
// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

extern const char kAIHubNewBadgeName[];
extern const char kAIHubNewBadgeDescription[];

extern const char kAIMEligibilityRefreshNTPModulesName[];
extern const char kAIMEligibilityRefreshNTPModulesDescription[];

extern const char kAIMEligibilityServiceStartWithProfileName[];
extern const char kAIMEligibilityServiceStartWithProfileDescription[];

extern const char kAIMNTPEntrypointTabletName[];
extern const char kAIMNTPEntrypointTabletDescription[];

extern const char kAnimatedDefaultBrowserPromoInFREName[];
extern const char kAnimatedDefaultBrowserPromoInFREDescription[];

extern const char kAppBackgroundRefreshName[];
extern const char kAppBackgroundRefreshDescription[];

extern const char kAppleCalendarExperienceKitName[];
extern const char kAppleCalendarExperienceKitDescription[];

extern const char kApplyClientsideModelPredictionsForPasswordTypesName[];
extern const char kApplyClientsideModelPredictionsForPasswordTypesDescription[];

extern const char kAskGeminiChipName[];
extern const char kAskGeminiChipDescription[];

extern const char kAutofillAcrossIframesName[];
extern const char kAutofillAcrossIframesDescription[];

extern const char kAutofillBottomSheetNewBlurName[];
extern const char kAutofillBottomSheetNewBlurDescription[];

extern const char kAutofillCreditCardScannerIosName[];
extern const char kAutofillCreditCardScannerIosDescription[];

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

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

extern const char kAutofillEnableCvcStorageAndFillingEnhancementName[];
extern const char kAutofillEnableCvcStorageAndFillingEnhancementDescription[];

extern const char kAutofillEnableCvcStorageAndFillingName[];
extern const char kAutofillEnableCvcStorageAndFillingDescription[];

extern const char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName[];
extern const char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription[];

extern const char kAutofillEnableFlatRateCardBenefitsFromCurinosName[];
extern const char kAutofillEnableFlatRateCardBenefitsFromCurinosDescription[];

extern const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName[];
extern const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription
        [];

extern const char kAutofillEnablePrefetchingRiskDataForRetrievalName[];
extern const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[];

extern const char kAutofillEnableSupportForHomeAndWorkName[];
extern const char kAutofillEnableSupportForHomeAndWorkDescription[];

extern const char kAutofillEnableSupportForNameAndEmailName[];
extern const char kAutofillEnableSupportForNameAndEmailDescription[];

extern const char kAutofillIsolatedWorldForJavascriptIOSName[];
extern const char kAutofillIsolatedWorldForJavascriptIOSDescription[];

extern const char kAutofillLocalSaveCardBottomSheetName[];
extern const char kAutofillLocalSaveCardBottomSheetDescription[];

extern const char kAutofillManualTestingDataName[];
extern const char kAutofillManualTestingDataDescription[];

extern const char kAutofillPaymentsFieldSwappingName[];
extern const char kAutofillPaymentsFieldSwappingDescription[];

extern const char kAutofillPaymentsSheetV2Name[];
extern const char kAutofillPaymentsSheetV2Description[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

extern const char kAutofillSaveCardBottomSheetName[];
extern const char kAutofillSaveCardBottomSheetDescription[];

extern const char kAutofillShowManualFillForVirtualCardsName[];
extern const char kAutofillShowManualFillForVirtualCardsDescription[];

extern const char kAutofillThrottleDocumentFormScanName[];
extern const char kAutofillThrottleDocumentFormScanDescription[];

extern const char kAutofillThrottleFilteredDocumentFormScanName[];
extern const char kAutofillThrottleFilteredDocumentFormScanDescription[];

extern const char kAutofillUnmaskCardRequestTimeoutName[];
extern const char kAutofillUnmaskCardRequestTimeoutDescription[];

extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

extern const char kAutofillVcnEnrollStrikeExpiryTimeName[];
extern const char kAutofillVcnEnrollStrikeExpiryTimeDescription[];

extern const char kBWGPreciseLocationName[];
extern const char kBWGPreciseLocationDescription[];

extern const char kBWGPromoConsentName[];
extern const char kBWGPromoConsentDescription[];

extern const char kBeginCursorAtPointTentativeFixName[];
extern const char kBeginCursorAtPointTentativeFixDescription[];

extern const char kBestFeaturesScreenInFirstRunName[];
extern const char kBestFeaturesScreenInFirstRunDescription[];

extern const char kBestOfAppFREName[];
extern const char kBestOfAppFREDescription[];

extern const char kBlueDotOnToolsMenuButtonName[];
extern const char kBlueDotOnToolsMenuButtonDescription[];

extern const char kBottomOmniboxEvolutionName[];
extern const char kBottomOmniboxEvolutionDescription[];

extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

extern const char kCacheIdentityListInChromeName[];
extern const char kCacheIdentityListInChromeDescription[];

extern const char kChromeStartupParametersAsyncName[];
extern const char kChromeStartupParametersAsyncDescription[];

extern const char kCollaborationMessagingName[];
extern const char kCollaborationMessagingDescription[];

extern const char kComposeboxAIMNudgeName[];
extern const char kComposeboxAIMNudgeDescription[];

extern const char kComposeboxAutoattachTabName[];
extern const char kComposeboxAutoattachTabDescription[];

extern const char kComposeboxCompactModeName[];
extern const char kComposeboxCompactModeDescription[];

extern const char kComposeboxDevToolsName[];
extern const char kComposeboxDevToolsDescription[];

extern const char kComposeboxForceTopName[];
extern const char kComposeboxForceTopDescription[];

extern const char kComposeboxIOSName[];
extern const char kComposeboxIOSDescription[];

extern const char kComposeboxImmersiveSRPName[];
extern const char kComposeboxImmersiveSRPDescription[];

extern const char kComposeboxMenuTitleName[];
extern const char kComposeboxMenuTitleDescription[];

extern const char kComposeboxTabPickerVariationName[];
extern const char kComposeboxTabPickerVariationDescription[];

extern const char kConfirmationButtonSwapOrderName[];
extern const char kConfirmationButtonSwapOrderDescription[];

extern const char kContentNotificationProvisionalIgnoreConditionsName[];
extern const char kContentNotificationProvisionalIgnoreConditionsDescription[];

extern const char kContentPushNotificationsName[];
extern const char kContentPushNotificationsDescription[];

extern const char kContextualPanelName[];
extern const char kContextualPanelDescription[];

extern const char kCredentialProviderExtensionPromoName[];
extern const char kCredentialProviderExtensionPromoDescription[];

extern const char kCredentialProviderPasskeyLargeBlobName[];
extern const char kCredentialProviderPasskeyLargeBlobDescription[];

extern const char kCredentialProviderPasskeyPRFName[];
extern const char kCredentialProviderPasskeyPRFDescription[];

extern const char kCredentialProviderPerformanceImprovementsName[];
extern const char kCredentialProviderPerformanceImprovementsDescription[];

extern const char kCredentialProviderSignalAPIName[];
extern const char kCredentialProviderSignalAPIDescription[];

extern const char kDataSharingDebugLogsName[];
extern const char kDataSharingDebugLogsDescription[];

extern const char kDataSharingJoinOnlyName[];
extern const char kDataSharingJoinOnlyDescription[];

extern const char kDataSharingName[];
extern const char kDataSharingDescription[];

extern const char kDataSharingSharedDataTypesEnabled[];
extern const char kDataSharingSharedDataTypesEnabledWithUi[];

extern const char kDefaultBrowserBannerPromoName[];
extern const char kDefaultBrowserBannerPromoDescription[];

extern const char kDataSharingVersioningStatesName[];
extern const char kDataSharingVersioningStatesDescription[];

extern const char kDefaultBrowserMagicStackIosName[];
extern const char kDefaultBrowserMagicStackIosDescription[];

extern const char kDefaultBrowserOffCyclePromoName[];
extern const char kDefaultBrowserOffCyclePromoDescription[];

extern const char kDefaultBrowserPromoIpadInstructionsName[];
extern const char kDefaultBrowserPromoIpadInstructionsDescription[];

extern const char kDefaultBrowserPromoPropensityModelName[];
extern const char kDefaultBrowserPromoPropensityModelDescription[];

extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

extern const char kDiamondPrototypeName[];
extern const char kDiamondPrototypeDescription[];

extern const char kDisableAutofillStrikeSystemName[];
extern const char kDisableAutofillStrikeSystemDescription[];

extern const char kDisableKeyboardAccessoryName[];
extern const char kDisableKeyboardAccessoryDescription[];

extern const char kDisableLensCameraName[];
extern const char kDisableLensCameraDescription[];

extern const char kDownloadAutoDeletionClearFilesOnEveryStartupName[];
extern const char kDownloadAutoDeletionClearFilesOnEveryStartupDescription[];

extern const char kDownloadAutoDeletionName[];
extern const char kDownloadAutoDeletionDescription[];

extern const char kDownloadListName[];
extern const char kDownloadListDescription[];

extern const char kDownloadServiceForegroundSessionName[];
extern const char kDownloadServiceForegroundSessionDescription[];

extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

extern const char kEnableACPrefetchName[];
extern const char kEnableACPrefetchDescription[];

extern const char kEnableASWebAuthenticationSessionName[];
extern const char kEnableASWebAuthenticationSessionDescription[];

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableClipboardDataControlsIOSName[];
extern const char kEnableClipboardDataControlsIOSDescription[];

extern const char kEnableCompromisedPasswordsMutingName[];
extern const char kEnableCompromisedPasswordsMutingDescription[];

extern const char kEnableCrossDevicePrefTrackerName[];
extern const char kEnableCrossDevicePrefTrackerDescription[];

extern const char kEnableFamilyLinkControlsName[];
extern const char kEnableFamilyLinkControlsDescription[];

extern const char kEnableFeedAblationName[];
extern const char kEnableFeedAblationDescription[];

extern const char kEnableFeedCardMenuSignInPromoName[];
extern const char kEnableFeedCardMenuSignInPromoDescription[];

extern const char kEnableFeedHeaderSettingsName[];
extern const char kEnableFeedHeaderSettingsDescription[];

extern const char kEnableIdentityInAuthErrorName[];
extern const char kEnableIdentityInAuthErrorDescription[];

extern const char kEnableLensInOmniboxCopiedImageName[];
extern const char kEnableLensInOmniboxCopiedImageDescription[];

extern const char kEnableLensOverlayName[];
extern const char kEnableLensOverlayDescription[];

extern const char kEnableLensViewFinderUnifiedExperienceName[];
extern const char kEnableLensViewFinderUnifiedExperienceDescription[];

extern const char kEnableReadingListAccountStorageName[];
extern const char kEnableReadingListAccountStorageDescription[];

extern const char kEnableReadingListSignInPromoName[];
extern const char kEnableReadingListSignInPromoDescription[];

extern const char kEnableTraitCollectionRegistrationName[];
extern const char kEnableTraitCollectionRegistrationDescription[];

extern const char kEnableVariationsGoogleGroupFilteringName[];
extern const char kEnableVariationsGoogleGroupFilteringDescription[];

extern const char kEnableiPadFeedGhostCardsName[];
extern const char kEnableiPadFeedGhostCardsDescription[];

extern const char kEnhancedCalendarName[];
extern const char kEnhancedCalendarDescription[];

extern const char kEnhancedSafeBrowsingPromoName[];
extern const char kEnhancedSafeBrowsingPromoDescription[];

extern const char kFRESignInHeaderTextUpdateName[];
extern const char kFRESignInHeaderTextUpdateDescription[];

extern const char kFeedBackgroundRefreshName[];
extern const char kFeedBackgroundRefreshDescription[];

extern const char kFeedSwipeInProductHelpName[];
extern const char kFeedSwipeInProductHelpDescription[];

extern const char kFeedbackIncludeGWSVariationsName[];
extern const char kFeedbackIncludeGWSVariationsDescription[];

extern const char kFeedbackIncludeVariationsName[];
extern const char kFeedbackIncludeVariationsDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

extern const char kFullscreenPromosManagerSkipInternalLimitsName[];
extern const char kFullscreenPromosManagerSkipInternalLimitsDescription[];

extern const char kFullscreenScrollThresholdName[];
extern const char kFullscreenScrollThresholdDescription[];

extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

extern const char kFullscreenTransitionSpeedName[];
extern const char kFullscreenTransitionSpeedDescription[];

extern const char kGeminiCrossTabName[];
extern const char kGeminiCrossTabDescription[];

extern const char kGeminiFullChatHistoryName[];
extern const char kGeminiFullChatHistoryDescription[];

extern const char kGeminiImmediateOverlayName[];
extern const char kGeminiImmediateOverlayDescription[];

extern const char kGeminiLatencyImprovementName[];
extern const char kGeminiLatencyImprovementDescription[];

extern const char kGeminiLiveName[];
extern const char kGeminiLiveDescription[];

extern const char kGeminiLoadingStateRedesignName[];
extern const char kGeminiLoadingStateRedesignDescription[];

extern const char kGeminiNavigationPromoName[];
extern const char kGeminiNavigationPromoDescription[];

extern const char kGeminiOnboardingCardsName[];
extern const char kGeminiOnboardingCardsDescription[];

extern const char kGeminiPersonalizationName[];
extern const char kGeminiPersonalizationDescription[];

extern const char kHandleMdmErrorsForDasherAccountsName[];
extern const char kHandleMdmErrorsForDasherAccountsDescription[];

extern const char kHideToolbarsInOverflowMenuName[];
extern const char kHideToolbarsInOverflowMenuDescription[];

extern const char kHomeMemoryImprovementsName[];
extern const char kHomeMemoryImprovementsDescription[];

extern const char kHttpsUpgradesName[];
extern const char kHttpsUpgradesDescription[];

extern const char kIOSAppBundlePromoEphemeralCardName[];
extern const char kIOSAppBundlePromoEphemeralCardDescription[];

extern const char kIOSAutoOpenRemoteTabGroupsSettingsName[];
extern const char kIOSAutoOpenRemoteTabGroupsSettingsDescription[];

extern const char kIOSBrowserEditMenuMetricsName[];
extern const char kIOSBrowserEditMenuMetricsDescription[];

extern const char kIOSBrowserReportIncludeAllProfilesName[];
extern const char kIOSBrowserReportIncludeAllProfilesDescription[];

extern const char kIOSChooseFromDriveName[];
extern const char kIOSChooseFromDriveDescription[];

extern const char kIOSCustomFileUploadMenuName[];
extern const char kIOSCustomFileUploadMenuDescription[];

extern const char kIOSDockingPromoName[];
extern const char kIOSDockingPromoDescription[];

extern const char kIOSEnableCloudProfileReportingName[];
extern const char kIOSEnableCloudProfileReportingDescription[];

extern const char kIOSEnableRealtimeEventReportingName[];
extern const char kIOSEnableRealtimeEventReportingDescription[];

extern const char kIOSExpandedTipsName[];
extern const char kIOSExpandedTipsDescription[];

extern const char kIOSFillRecoveryPasswordName[];
extern const char kIOSFillRecoveryPasswordDescription[];

extern const char kIOSKeyboardAccessoryDefaultViewName[];
extern const char kIOSKeyboardAccessoryDefaultViewDescription[];

extern const char kIOSKeyboardAccessoryTwoBubbleName[];
extern const char kIOSKeyboardAccessoryTwoBubbleDescription[];

extern const char kIOSManageAccountStorageName[];
extern const char kIOSManageAccountStorageDescription[];

extern const char kIOSMiniMapUniversalLinkName[];
extern const char kIOSMiniMapUniversalLinkDescription[];

extern const char kIOSOmniboxAimServerEligibilityEnName[];
extern const char kIOSOmniboxAimServerEligibilityEnDescription[];

extern const char kIOSOmniboxAimServerEligibilityName[];
extern const char kIOSOmniboxAimServerEligibilityDescription[];

extern const char kIOSOmniboxAimShortcutName[];
extern const char kIOSOmniboxAimShortcutDescription[];

extern const char kIOSOneTapMiniMapRemoveSectionBreaksName[];
extern const char kIOSOneTapMiniMapRemoveSectionBreaksDescription[];

extern const char kIOSOneTapMiniMapRestrictionsName[];
extern const char kIOSOneTapMiniMapRestrictionsDescription[];

extern const char kIOSOneTimeDefaultBrowserNotificationName[];
extern const char kIOSOneTimeDefaultBrowserNotificationDescription[];

extern const char kIOSProactivePasswordGenerationBottomSheetName[];
extern const char kIOSProactivePasswordGenerationBottomSheetDescription[];

extern const char kIOSProvidesAppNotificationSettingsName[];
extern const char kIOSProvidesAppNotificationSettingsDescription[];

extern const char kIOSReactivationNotificationsName[];
extern const char kIOSReactivationNotificationsDescription[];

extern const char kIOSSaveToDriveClientFolderName[];
extern const char kIOSSaveToDriveClientFolderDescription[];

extern const char kIOSSoftLockName[];
extern const char kIOSSoftLockDescription[];

extern const char kIOSStartTimeBrowserBackgroundRemediationsName[];
extern const char kIOSStartTimeBrowserBackgroundRemediationsDescription[];

extern const char kIOSStartTimeStartupRemediationsName[];
extern const char kIOSStartTimeStartupRemediationsDescription[];

extern const char kIOSSyncedSetUpName[];
extern const char kIOSSyncedSetUpDescription[];

extern const char kIOSTipsNotificationsStringAlternativesName[];
extern const char kIOSTipsNotificationsStringAlternativesDescription[];

extern const char kIOSTrustedVaultNotificationName[];
extern const char kIOSTrustedVaultNotificationDescription[];

extern const char kIOSWebContextMenuNewTitleName[];
extern const char kIOSWebContextMenuNewTitleDescription[];

extern const char kIPHPriceNotificationsWhileBrowsingName[];
extern const char kIPHPriceNotificationsWhileBrowsingDescription[];

extern const char kIdentityConfirmationSnackbarName[];
extern const char kIdentityConfirmationSnackbarDescription[];

extern const char kImageContextMenuGeminiEntryPointName[];
extern const char kImageContextMenuGeminiEntryPointDescription[];

extern const char kImportPasswordsFromSafariName[];
extern const char kImportPasswordsFromSafariDescription[];

extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

extern const char kIndicateIdentityErrorInOverflowMenuName[];
extern const char kIndicateIdentityErrorInOverflowMenuDescription[];

extern const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[];
extern const char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[];

extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName[];
extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription[];

extern const char kLensCameraNoStillOutputRequiredName[];
extern const char kLensCameraNoStillOutputRequiredDescription[];

extern const char kLensCameraUnbinnedCaptureFormatsPreferredName[];
extern const char kLensCameraUnbinnedCaptureFormatsPreferredDescription[];

extern const char kLensContinuousZoomEnabledName[];
extern const char kLensContinuousZoomEnabledDescription[];

extern const char kLensExactMatchesEnabledName[];
extern const char kLensExactMatchesEnabledDescription[];

extern const char kLensFetchSrpApiEnabledName[];
extern const char kLensFetchSrpApiEnabledDescription[];

extern const char kLensFiltersAblationModeEnabledName[];
extern const char kLensFiltersAblationModeEnabledDescription[];

extern const char kLensGestureTextSelectionDisabledName[];
extern const char kLensGestureTextSelectionDisabledDescription[];

extern const char kLensInitialLvfZoomLevel90PercentName[];
extern const char kLensInitialLvfZoomLevel90PercentDescription[];

extern const char kLensLoadAIMInLensResultPageName[];
extern const char kLensLoadAIMInLensResultPageDescription[];

extern const char kLensOmnientShaderV2EnabledName[];
extern const char kLensOmnientShaderV2EnabledDescription[];

extern const char kLensOverlayCustomBottomSheetName[];
extern const char kLensOverlayCustomBottomSheetDescription[];

extern const char kLensOverlayDisableIPHPanGestureName[];
extern const char kLensOverlayDisableIPHPanGestureDescription[];

extern const char kLensOverlayEnableIPadCompatibilityName[];
extern const char kLensOverlayEnableIPadCompatibilityDescription[];

extern const char kLensOverlayEnableLandscapeCompatibilityName[];
extern const char kLensOverlayEnableLandscapeCompatibilityDescription[];

extern const char kLensOverlayForceShowOnboardingScreenName[];
extern const char kLensOverlayForceShowOnboardingScreenDescription[];

extern const char kLensOverlayNavigationHistoryName[];
extern const char kLensOverlayNavigationHistoryDescription[];

extern const char kLensPrewarmHardStickinessInInputSelectionName[];
extern const char kLensPrewarmHardStickinessInInputSelectionDescription[];

extern const char kLensPrewarmHardStickinessInQueryFormulationName[];
extern const char kLensPrewarmHardStickinessInQueryFormulationDescription[];

extern const char kLensSearchHeadersCheckEnabledName[];
extern const char kLensSearchHeadersCheckEnabledDescription[];

extern const char kLensSingleTapTextSelectionDisabledName[];
extern const char kLensSingleTapTextSelectionDisabledDescription[];

extern const char kLensStreamServiceWebChannelTransportEnabledName[];
extern const char kLensStreamServiceWebChannelTransportEnabledDescription[];

extern const char kLensTranslateToggleModeEnabledName[];
extern const char kLensTranslateToggleModeEnabledDescription[];

extern const char kLensTripleCameraEnabledName[];
extern const char kLensTripleCameraEnabledDescription[];

extern const char kLensUnaryApiSalientTextEnabledName[];
extern const char kLensUnaryApiSalientTextEnabledDescription[];

extern const char kLensUnaryApisWithHttpTransportEnabledName[];
extern const char kLensUnaryApisWithHttpTransportEnabledDescription[];

extern const char kLensUnaryHttpTransportEnabledName[];
extern const char kLensUnaryHttpTransportEnabledDescription[];

extern const char kLensWebPageLoadOptimizationEnabledName[];
extern const char kLensWebPageLoadOptimizationEnabledDescription[];

extern const char kLinkedServicesSettingIosName[];
extern const char kLinkedServicesSettingIosDescription[];

extern const char kLocationBarBadgeMigrationName[];
extern const char kLocationBarBadgeMigrationDescription[];

extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

extern const char kManualLogUploadsInFREName[];
extern const char kManualLogUploadsInFREDescription[];

extern const char kMeasurementsName[];
extern const char kMeasurementsDescription[];

extern const char kMetrickitNonCrashReportName[];
extern const char kMetrickitNonCrashReportDescription[];

extern const char kMigrateAccountPrefsOnMobileName[];
extern const char kMigrateAccountPrefsOnMobileDescription[];

extern const char kMigrateIOSKeychainAccessibilityName[];
extern const char kMigrateIOSKeychainAccessibilityDescription[];

extern const char kMobilePromoOnDesktopName[];
extern const char kMobilePromoOnDesktopDescription[];

extern const char kMostVisitedTilesCustomizationName[];
extern const char kMostVisitedTilesCustomizationDescription[];

extern const char kMostVisitedTilesHorizontalRenderGroupName[];
extern const char kMostVisitedTilesHorizontalRenderGroupDescription[];

extern const char kMultilineBrowserOmniboxName[];
extern const char kMultilineBrowserOmniboxDescription[];

extern const char kNTPBackgroundColorSliderName[];
extern const char kNTPBackgroundColorSliderDescription[];

extern const char kNTPBackgroundCustomizationName[];
extern const char kNTPBackgroundCustomizationDescription[];

extern const char kNTPMIAEntrypointName[];
extern const char kNTPMIAEntrypointDescription[];

extern const char kNTPViewHierarchyRepairName[];
extern const char kNTPViewHierarchyRepairDescription[];

extern const char kNativeFindInPageName[];
extern const char kNativeFindInPageDescription[];

extern const char kNewShareExtensionName[];
extern const char kNewShareExtensionDescription[];

extern const char kNewTabPageFieldTrialName[];
extern const char kNewTabPageFieldTrialDescription[];

extern const char kNonModalSignInPromoName[];
extern const char kNonModalSignInPromoDescription[];

extern const char kNotificationCollisionManagementName[];
extern const char kNotificationCollisionManagementDescription[];

extern const char kNotificationSettingsMenuItemName[];
extern const char kNotificationSettingsMenuItemDescription[];

extern const char kNtpAlphaBackgroundCollectionsName[];
extern const char kNtpAlphaBackgroundCollectionsDescription[];

extern const char kNtpComposeboxUsesChromeComposeClientName[];
extern const char kNtpComposeboxUsesChromeComposeClientDescription[];

extern const char kOmniboxDRSPrototypeName[];
extern const char kOmniboxDRSPrototypeDescription[];

extern const char kOmniboxDRSPrototypeName[];
extern const char kOmniboxDRSPrototypeDescription[];

extern const char kOmniboxGroupingFrameworkForTypedSuggestionsName[];
extern const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[];

extern const char kOmniboxGroupingFrameworkForZPSName[];
extern const char kOmniboxGroupingFrameworkForZPSDescription[];

extern const char kOmniboxHttpsUpgradesName[];
extern const char kOmniboxHttpsUpgradesDescription[];

extern const char kOmniboxInspireMeSignedOutName[];
extern const char kOmniboxInspireMeSignedOutDescription[];

extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[];
extern const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[];

extern const char kOmniboxMaxURLMatchesName[];
extern const char kOmniboxMaxURLMatchesDescription[];

extern const char kOmniboxMiaZpsName[];
extern const char kOmniboxMiaZpsDescription[];

extern const char kOmniboxMlLogUrlScoringSignalsName[];
extern const char kOmniboxMlLogUrlScoringSignalsDescription[];

extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[];
extern const char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[];

extern const char kOmniboxMlUrlScoreCachingName[];
extern const char kOmniboxMlUrlScoreCachingDescription[];

extern const char kOmniboxMlUrlScoringModelName[];
extern const char kOmniboxMlUrlScoringModelDescription[];

extern const char kOmniboxMlUrlScoringName[];
extern const char kOmniboxMlUrlScoringDescription[];

extern const char kOmniboxMlUrlSearchBlendingName[];
extern const char kOmniboxMlUrlSearchBlendingDescription[];

extern const char kOmniboxMobileParityUpdateName[];
extern const char kOmniboxMobileParityUpdateDescription[];

extern const char kOmniboxMobileParityUpdateV2Name[];
extern const char kOmniboxMobileParityUpdateV2Description[];

extern const char kOmniboxMobileParityUpdateV3Name[];
extern const char kOmniboxMobileParityUpdateV3Description[];

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

extern const char kPageActionMenuName[];
extern const char kPageActionMenuDescription[];

extern const char kPageContentAnnotationsName[];
extern const char kPageContentAnnotationsDescription[];

extern const char kPageContentAnnotationsRemotePageMetadataName[];
extern const char kPageContentAnnotationsRemotePageMetadataDescription[];

extern const char kPageContextAnchorTagsName[];
extern const char kPageContextAnchorTagsDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kPasswordFormClientsideClassifierName[];
extern const char kPasswordFormClientsideClassifierDescription[];

extern const char kPasswordReuseDetectionName[];
extern const char kPasswordReuseDetectionDescription[];

extern const char kPasswordSharingName[];
extern const char kPasswordSharingDescription[];

extern const char kPersistTabContextName[];
extern const char kPersistTabContextDescription[];

extern const char kPersistentDefaultBrowserPromoName[];
extern const char kPersistentDefaultBrowserPromoDescription[];

extern const char kPhoneNumberName[];
extern const char kPhoneNumberDescription[];

extern const char kPriceTrackingPromoName[];
extern const char kPriceTrackingPromoDescription[];

extern const char kPrivacyGuideIosName[];
extern const char kPrivacyGuideIosDescription[];

extern const char kProactiveSuggestionsFrameworkName[];
extern const char kProactiveSuggestionsFrameworkDescription[];

extern const char kProvisionalNotificationAlertName[];
extern const char kProvisionalNotificationAlertDescription[];

extern const char kRcapsDynamicProfileCountryName[];
extern const char kRcapsDynamicProfileCountryDescription[];

extern const char kReaderModeName[];
extern const char kReaderModeDescription[];

extern const char kReaderModeNewCssName[];
extern const char kReaderModeNewCssDescription[];

extern const char kReaderModeOptimizationGuideEligibilityName[];
extern const char kReaderModeOptimizationGuideEligibilityDescription[];

extern const char kReaderModePageEligibilityHeuristicName[];
extern const char kReaderModePageEligibilityHeuristicDescription[];

extern const char kReaderModeReadabilityDistillerName[];
extern const char kReaderModeReadabilityDistillerDescription[];

extern const char kReaderModeReadabilityHeuristicName[];
extern const char kReaderModeReadabilityHeuristicDescription[];

extern const char kReaderModeTranslationName[];
extern const char kReaderModeTranslationDescription[];

extern const char kReaderModeTranslationWithInfobarName[];
extern const char kReaderModeTranslationWithInfobarDescription[];

extern const char kReaderModeUSEnabledName[];
extern const char kReaderModeUSEnabledDescription[];

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

extern const char kSendTabToSelfIOSPushNotificationsName[];
extern const char kSendTabToSelfIOSPushNotificationsDescription[];

extern const char kSetUpListShortenedDurationName[];
extern const char kSetUpListShortenedDurationDescription[];

extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

extern const char kShopCardImpressionLimitsName[];
extern const char kShopCardImpressionLimitsDescription[];

extern const char kShopCardName[];
extern const char kShopCardDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kShowTabGroupInGridOnStartName[];
extern const char kShowTabGroupInGridOnStartDescription[];

extern const char kSignInButtonNoAvatarName[];
extern const char kSignInButtonNoAvatarDescription[];

extern const char kSkipDefaultBrowserPromoInFirstRunName[];
extern const char kSkipDefaultBrowserPromoInFirstRunDescription[];

extern const char kSmartTabGroupingName[];
extern const char kSmartTabGroupingDescription[];

extern const char kSpotlightNeverRetainIndexName[];
extern const char kSpotlightNeverRetainIndexDescription[];

extern const char kStartSurfaceName[];
extern const char kStartSurfaceDescription[];

extern const char kStrokesAPIEnabledName[];
extern const char kStrokesAPIEnabledDescription[];

extern const char kSupervisedUserBlockInterstitialV3Name[];
extern const char kSupervisedUserBlockInterstitialV3Description[];

extern const char kSyncAutofillWalletCredentialDataName[];
extern const char kSyncAutofillWalletCredentialDataDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncTrustedVaultInfobarMessageImprovementsName[];
extern const char kSyncTrustedVaultInfobarMessageImprovementsDescription[];

extern const char kTabGridDragAndDropName[];
extern const char kTabGridDragAndDropDescription[];

extern const char kTabGridEmptyThumbnailName[];
extern const char kTabGridEmptyThumbnailDescription[];

extern const char kTabGridNewTransitionsName[];
extern const char kTabGridNewTransitionsDescription[];

extern const char kTabGroupColorOnSurfaceName[];
extern const char kTabGroupColorOnSurfaceDescription[];

extern const char kTabGroupInOverflowMenuName[];
extern const char kTabGroupInOverflowMenuDescription[];

extern const char kTabGroupInTabIconContextMenuName[];
extern const char kTabGroupInTabIconContextMenuDescription[];

extern const char kTabGroupIndicatorName[];
extern const char kTabGroupIndicatorDescription[];

extern const char kTabGroupSyncName[];
extern const char kTabGroupSyncDescription[];

extern const char kTabRecallNewTabGroupButtonName[];
extern const char kTabRecallNewTabGroupButtonDescription[];

extern const char kTabResumptionImagesName[];
extern const char kTabResumptionImagesDescription[];

extern const char kTabResumptionName[];
extern const char kTabResumptionDescription[];

extern const char kTabSwitcherOverflowMenuName[];
extern const char kTabSwitcherOverflowMenuDescription[];

extern const char kTaiyakiName[];
extern const char kTaiyakiDescription[];

extern const char kUpdatedFRESequenceName[];
extern const char kUpdatedFRESequenceDescription[];

extern const char kUseDefaultAppsDestinationForPromosName[];
extern const char kUseDefaultAppsDestinationForPromosDescription[];

extern const char kUseFeedEligibilityServiceName[];
extern const char kUseFeedEligibilityServiceDescription[];

extern const char kVariationsSeedCorpusName[];
extern const char kVariationsSeedCorpusDescription[];

extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebFeedFeedbackRerouteName[];
extern const char kWebFeedFeedbackRerouteDescription[];

extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

extern const char kWebPageReportedImagesSheetName[];
extern const char kWebPageReportedImagesSheetDescription[];

extern const char kWebPageTextZoomIPadName[];
extern const char kWebPageTextZoomIPadDescription[];

extern const char kWelcomeBackName[];
extern const char kWelcomeBackDescription[];

extern const char kYoutubeIncognitoName[];
extern const char kYoutubeIncognitoDescription[];

extern const char kZeroStateSuggestionsName[];
extern const char kZeroStateSuggestionsDescription[];

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

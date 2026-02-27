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

extern const char kAIMCobrowseDebugEntrypointName[];
extern const char kAIMCobrowseDebugEntrypointDescription[];

extern const char kAIMEligibilityRefreshNTPModulesName[];
extern const char kAIMEligibilityRefreshNTPModulesDescription[];

extern const char kAIMEligibilityServiceStartWithProfileName[];
extern const char kAIMEligibilityServiceStartWithProfileDescription[];

extern const char kAIMNTPEntrypointTabletName[];
extern const char kAIMNTPEntrypointTabletDescription[];

extern const char kAimCobrowseName[];
extern const char kAimCobrowseDescription[];

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

extern const char kAssistantContainerName[];
extern const char kAssistantContainerDescription[];

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

extern const char kAutofillEnableBottomSheetScanCardAndFillName[];
extern const char kAutofillEnableBottomSheetScanCardAndFillDescription[];

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

extern const char kAutofillEnablePrefetchingRiskDataForRetrievalName[];
extern const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[];

extern const char kAutofillEnableSupportForHomeAndWorkName[];
extern const char kAutofillEnableSupportForHomeAndWorkDescription[];

extern const char kAutofillEnableSupportForNameAndEmailName[];
extern const char kAutofillEnableSupportForNameAndEmailDescription[];

extern const char kAutofillEnableWalletBrandingName[];
extern const char kAutofillEnableWalletBrandingDescription[];

extern const char kAutofillManualTestingDataName[];
extern const char kAutofillManualTestingDataDescription[];

extern const char kAutofillPaymentsFieldSwappingName[];
extern const char kAutofillPaymentsFieldSwappingDescription[];

extern const char kAutofillPaymentsSheetV2Name[];
extern const char kAutofillPaymentsSheetV2Description[];

extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

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

extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

extern const char kCacheIdentityListInChromeName[];
extern const char kCacheIdentityListInChromeDescription[];

extern const char kChromeNextIaName[];
extern const char kChromeNextIaDescription[];

extern const char kCloseOtherTabsName[];
extern const char kCloseOtherTabsDescription[];

extern const char kCollaborationMessagingName[];
extern const char kCollaborationMessagingDescription[];

extern const char kComposeboxAIMDisabledName[];
extern const char kComposeboxAIMDisabledDescription[];

extern const char kComposeboxAIMNudgeName[];
extern const char kComposeboxAIMNudgeDescription[];

extern const char kComposeboxAdditionalAdvancedToolsName[];
extern const char kComposeboxAdditionalAdvancedToolsDescription[];

extern const char kComposeboxAttachmentsTypedStateName[];
extern const char kComposeboxAttachmentsTypedStateDescription[];

extern const char kComposeboxCloseButtonTopAlignName[];
extern const char kComposeboxCloseButtonTopAlignDescription[];

extern const char kComposeboxCompactModeName[];
extern const char kComposeboxCompactModeDescription[];

extern const char kComposeboxDeepSearchName[];
extern const char kComposeboxDeepSearchDescription[];

extern const char kComposeboxDevToolsName[];
extern const char kComposeboxDevToolsDescription[];

extern const char
    kComposeboxFetchContextualSuggestionsForMultipleAttachmentsName[];
extern const char
    kComposeboxFetchContextualSuggestionsForMultipleAttachmentsDescription[];

extern const char kComposeboxForceTopName[];
extern const char kComposeboxForceTopDescription[];

extern const char kComposeboxIOSName[];
extern const char kComposeboxIOSDescription[];

extern const char kComposeboxImmersiveSRPName[];
extern const char kComposeboxImmersiveSRPDescription[];

extern const char kComposeboxIpadName[];
extern const char kComposeboxIpadDescription[];

extern const char kComposeboxMenuTitleName[];
extern const char kComposeboxMenuTitleDescription[];

extern const char kComposeboxServerSideStateName[];
extern const char kComposeboxServerSideStateDescription[];

extern const char kComposeboxTabPickerVariationName[];
extern const char kComposeboxTabPickerVariationDescription[];

extern const char kConfirmationButtonSwapOrderName[];
extern const char kConfirmationButtonSwapOrderDescription[];

extern const char kConsistentLogoDoodleHeightName[];
extern const char kConsistentLogoDoodleHeightDescription[];

extern const char kContentNotificationProvisionalIgnoreConditionsName[];
extern const char kContentNotificationProvisionalIgnoreConditionsDescription[];

extern const char kContentPushNotificationsName[];
extern const char kContentPushNotificationsDescription[];

extern const char kCredentialProviderExtensionPromoName[];
extern const char kCredentialProviderExtensionPromoDescription[];

extern const char kCredentialProviderPasskeyLargeBlobName[];
extern const char kCredentialProviderPasskeyLargeBlobDescription[];

extern const char kCredentialProviderPasskeyPRFName[];
extern const char kCredentialProviderPasskeyPRFDescription[];

extern const char kCredentialProviderPerformanceImprovementsName[];
extern const char kCredentialProviderPerformanceImprovementsDescription[];

extern const char kDataSharingDebugLogsName[];
extern const char kDataSharingDebugLogsDescription[];

extern const char kDataSharingJoinOnlyName[];
extern const char kDataSharingJoinOnlyDescription[];

extern const char kDataSharingName[];
extern const char kDataSharingDescription[];

extern const char kDataSharingSharedDataTypesEnabled[];
extern const char kDataSharingSharedDataTypesEnabledWithUi[];

extern const char kDataSharingVersioningStatesName[];
extern const char kDataSharingVersioningStatesDescription[];

extern const char kDefaultBrowserOffCyclePromoName[];
extern const char kDefaultBrowserOffCyclePromoDescription[];

extern const char kDefaultBrowserPictureInPictureName[];
extern const char kDefaultBrowserPictureInPictureDescription[];

extern const char kDefaultBrowserPromoIpadInstructionsName[];
extern const char kDefaultBrowserPromoIpadInstructionsDescription[];

extern const char kDefaultBrowserPromoPropensityModelName[];
extern const char kDefaultBrowserPromoPropensityModelDescription[];

extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

extern const char kDisableAutofillStrikeSystemName[];
extern const char kDisableAutofillStrikeSystemDescription[];

extern const char kDisableComposeboxFromAIMNTPName[];
extern const char kDisableComposeboxFromAIMNTPDescription[];

extern const char kDisableKeyboardAccessoryName[];
extern const char kDisableKeyboardAccessoryDescription[];

extern const char kDisableLensCameraName[];
extern const char kDisableLensCameraDescription[];

extern const char kDisableShareButtonName[];
extern const char kDisableShareButtonDescription[];

extern const char kDisableU18FeedbackIosName[];
extern const char kDisableU18FeedbackIosDescription[];

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

extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

extern const char kEnableCompromisedPasswordsMutingName[];
extern const char kEnableCompromisedPasswordsMutingDescription[];

extern const char kEnableFamilyLinkControlsName[];
extern const char kEnableFamilyLinkControlsDescription[];

extern const char kEnableFeedAblationName[];
extern const char kEnableFeedAblationDescription[];

extern const char kEnableFeedCardMenuSignInPromoName[];
extern const char kEnableFeedCardMenuSignInPromoDescription[];

extern const char kEnableFeedHeaderSettingsName[];
extern const char kEnableFeedHeaderSettingsDescription[];

extern const char kEnableFileDownloadConnectorIOSName[];
extern const char kEnableFileDownloadConnectorIOSDescription[];

extern const char kEnableFuseboxKeyboardAccessoryName[];
extern const char kEnableFuseboxKeyboardAccessoryDescription[];

extern const char kEnableLensInOmniboxCopiedImageName[];
extern const char kEnableLensInOmniboxCopiedImageDescription[];

extern const char kEnableNTPBackgroundImageCacheName[];
extern const char kEnableNTPBackgroundImageCacheDescription[];

extern const char kEnableNewStartupFlowName[];
extern const char kEnableNewStartupFlowDescription[];

extern const char kEnableReadingListAccountStorageName[];
extern const char kEnableReadingListAccountStorageDescription[];

extern const char kEnableReadingListSignInPromoName[];
extern const char kEnableReadingListSignInPromoDescription[];

extern const char kEnableTraitCollectionRegistrationName[];
extern const char kEnableTraitCollectionRegistrationDescription[];

extern const char kEnableVariationsGoogleGroupFilteringName[];
extern const char kEnableVariationsGoogleGroupFilteringDescription[];

extern const char kEnhancedCalendarName[];
extern const char kEnhancedCalendarDescription[];

extern const char kExplainGeminiEditMenuName[];
extern const char kExplainGeminiEditMenuDescription[];

extern const char kFRESignInHeaderTextUpdateName[];
extern const char kFRESignInHeaderTextUpdateDescription[];

extern const char kFeedBackgroundRefreshName[];
extern const char kFeedBackgroundRefreshDescription[];

extern const char kFeedSwipeInProductHelpName[];
extern const char kFeedSwipeInProductHelpDescription[];

extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

extern const char kFullscreenScrollThresholdName[];
extern const char kFullscreenScrollThresholdDescription[];

extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

extern const char kFullscreenTransitionSpeedName[];
extern const char kFullscreenTransitionSpeedDescription[];

extern const char kGeminiActorName[];
extern const char kGeminiActorDescription[];

extern const char kGeminiBackendMigrationName[];
extern const char kGeminiBackendMigrationDescription[];

extern const char kGeminiCopresenceName[];
extern const char kGeminiCopresenceDescription[];

extern const char kGeminiDynamicSettingsName[];
extern const char kGeminiDynamicSettingsDescription[];

extern const char kGeminiFloatyAllPagesName[];
extern const char kGeminiFloatyAllPagesDescription[];

extern const char kGeminiFullChatHistoryName[];
extern const char kGeminiFullChatHistoryDescription[];

extern const char kGeminiImageRemixToolName[];
extern const char kGeminiImageRemixToolDescription[];

extern const char kGeminiLatencyImprovementName[];
extern const char kGeminiLatencyImprovementDescription[];

extern const char kGeminiLiveName[];
extern const char kGeminiLiveDescription[];

extern const char kGeminiLoadingStateRedesignName[];
extern const char kGeminiLoadingStateRedesignDescription[];

extern const char kGeminiNavigationPromoName[];
extern const char kGeminiNavigationPromoDescription[];

extern const char kGeminiPersonalizationName[];
extern const char kGeminiPersonalizationDescription[];

extern const char kGeminiRefactoredFREName[];
extern const char kGeminiRefactoredFREDescription[];

extern const char kGeminiResponseViewDynamicResizingName[];
extern const char kGeminiResponseViewDynamicResizingDescription[];

extern const char kGeminiRichAPCExtractionName[];
extern const char kGeminiRichAPCExtractionDescription[];

extern const char kGeminiUpdatedEligibilityName[];
extern const char kGeminiUpdatedEligibilityDescription[];

extern const char kHandleMdmErrorsForDasherAccountsName[];
extern const char kHandleMdmErrorsForDasherAccountsDescription[];

extern const char kHideFuseboxVoiceLensActionsName[];
extern const char kHideFuseboxVoiceLensActionsDescription[];

extern const char kHideToolbarsInOverflowMenuName[];
extern const char kHideToolbarsInOverflowMenuDescription[];

extern const char kHttpsUpgradesName[];
extern const char kHttpsUpgradesDescription[];

extern const char kIOSAppBundlePromoEphemeralCardName[];
extern const char kIOSAppBundlePromoEphemeralCardDescription[];

extern const char kIOSBrowserEditMenuMetricsName[];
extern const char kIOSBrowserEditMenuMetricsDescription[];

extern const char kIOSBrowserReportIncludeAllProfilesName[];
extern const char kIOSBrowserReportIncludeAllProfilesDescription[];

extern const char kIOSChooseFromDriveName[];
extern const char kIOSChooseFromDriveDescription[];

extern const char kIOSChooseFromDriveSignedOutName[];
extern const char kIOSChooseFromDriveSignedOutDescription[];

extern const char kIOSCustomFileUploadMenuName[];
extern const char kIOSCustomFileUploadMenuDescription[];

extern const char kIOSDateToCalendarSignedOutName[];
extern const char kIOSDateToCalendarSignedOutDescription[];

extern const char kIOSDockingPromoName[];
extern const char kIOSDockingPromoDescription[];

extern const char kIOSDockingPromoV2Name[];
extern const char kIOSDockingPromoV2Description[];

extern const char kIOSEnableCloudProfileReportingName[];
extern const char kIOSEnableCloudProfileReportingDescription[];

extern const char kIOSEnableRealtimeEventReportingName[];
extern const char kIOSEnableRealtimeEventReportingDescription[];

extern const char kIOSExpandedSetupListName[];
extern const char kIOSExpandedSetupListDescription[];

extern const char kIOSExpandedTipsName[];
extern const char kIOSExpandedTipsDescription[];

extern const char kIOSKeyboardAccessoryDefaultViewName[];
extern const char kIOSKeyboardAccessoryDefaultViewDescription[];

extern const char kIOSKeyboardAccessoryTwoBubbleName[];
extern const char kIOSKeyboardAccessoryTwoBubbleDescription[];

extern const char kIOSOmniboxAimServerEligibilityEnName[];
extern const char kIOSOmniboxAimServerEligibilityEnDescription[];

extern const char kIOSOmniboxAimServerEligibilityName[];
extern const char kIOSOmniboxAimServerEligibilityDescription[];

extern const char kIOSOmniboxAimShortcutName[];
extern const char kIOSOmniboxAimShortcutDescription[];

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

extern const char kIOSSaveToDriveSignedOutName[];
extern const char kIOSSaveToDriveSignedOutDescription[];

extern const char kIOSSaveToPhotosSignedOutName[];
extern const char kIOSSaveToPhotosSignedOutDescription[];

extern const char kIOSSoftLockName[];
extern const char kIOSSoftLockDescription[];

extern const char kIOSSyncedSetUpName[];
extern const char kIOSSyncedSetUpDescription[];

extern const char kIOSTipsNotificationsStringAlternativesName[];
extern const char kIOSTipsNotificationsStringAlternativesDescription[];

extern const char kIOSTrustedVaultNotificationName[];
extern const char kIOSTrustedVaultNotificationDescription[];

extern const char kIOSWebContextMenuNewTitleName[];
extern const char kIOSWebContextMenuNewTitleDescription[];

extern const char kIdentityConfirmationSnackbarName[];
extern const char kIdentityConfirmationSnackbarDescription[];

extern const char kInFlowTrustedVaultKeyRetrievalIosName[];
extern const char kInFlowTrustedVaultKeyRetrievalIosDescription[];

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

extern const char kLensOverlayEnableLandscapeCompatibilityName[];
extern const char kLensOverlayEnableLandscapeCompatibilityDescription[];

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

extern const char kMigrateIOSKeychainAccessibilityName[];
extern const char kMigrateIOSKeychainAccessibilityDescription[];

extern const char kMobilePromoOnDesktopName[];
extern const char kMobilePromoOnDesktopDescription[];

extern const char kMobilePromoOnDesktopRecordActiveDaysName[];
extern const char kMobilePromoOnDesktopRecordActiveDaysDescription[];

extern const char kModelBasedPageClassificationName[];
extern const char kModelBasedPageClassificationDescription[];

extern const char kMostVisitedTilesCustomizationName[];
extern const char kMostVisitedTilesCustomizationDescription[];

extern const char kMostVisitedTilesHorizontalRenderGroupName[];
extern const char kMostVisitedTilesHorizontalRenderGroupDescription[];

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

extern const char kNewTabPageFieldTrialName[];
extern const char kNewTabPageFieldTrialDescription[];

extern const char kNonModalSignInPromoName[];
extern const char kNonModalSignInPromoDescription[];

extern const char kNotificationCollisionManagementName[];
extern const char kNotificationCollisionManagementDescription[];

extern const char kNtpAlphaBackgroundCollectionsName[];
extern const char kNtpAlphaBackgroundCollectionsDescription[];

extern const char kNtpComposeboxUsesChromeComposeClientName[];
extern const char kNtpComposeboxUsesChromeComposeClientDescription[];

extern const char kOmniboxCrashFixKillSwitchName[];
extern const char kOmniboxCrashFixKillSwitchDescription[];

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

extern const char kOmniboxZeroSuggestPrefetchingOnSRPName[];
extern const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[];

extern const char kOmniboxZeroSuggestPrefetchingOnWebName[];
extern const char kOmniboxZeroSuggestPrefetchingOnWebDescription[];

extern const char kOptimizationGuideDebugLogsName[];
extern const char kOptimizationGuideDebugLogsDescription[];

extern const char kPageActionMenuIconName[];
extern const char kPageActionMenuIconDescription[];

extern const char kPageActionMenuName[];
extern const char kPageActionMenuDescription[];

extern const char kPageContentAnnotationsName[];
extern const char kPageContentAnnotationsDescription[];

extern const char kPageContentAnnotationsRemotePageMetadataName[];
extern const char kPageContentAnnotationsRemotePageMetadataDescription[];

extern const char kPageVisibilityPageContentAnnotationsName[];
extern const char kPageVisibilityPageContentAnnotationsDescription[];

extern const char kPasswordFormClientsideClassifierName[];
extern const char kPasswordFormClientsideClassifierDescription[];

extern const char kPasswordRemovalFromDeleteBrowsingDataName[];
extern const char kPasswordRemovalFromDeleteBrowsingDataDescription[];

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

extern const char kProactiveSuggestionsFrameworkName[];
extern const char kProactiveSuggestionsFrameworkDescription[];

extern const char kProactiveSuggestionsFrameworkPopupBlockerName[];
extern const char kProactiveSuggestionsFrameworkPopupBlockerDescription[];

extern const char kProvisionalNotificationAlertName[];
extern const char kProvisionalNotificationAlertDescription[];

extern const char kRcapsDynamicProfileCountryName[];
extern const char kRcapsDynamicProfileCountryDescription[];

extern const char kReaderModeContentSettingsForLinksName[];
extern const char kReaderModeContentSettingsForLinksDescription[];

extern const char kReaderModeOmniboxEntrypointInUSName[];
extern const char kReaderModeOmniboxEntrypointInUSDescription[];

extern const char kReaderModeOptimizationGuideEligibilityName[];
extern const char kReaderModeOptimizationGuideEligibilityDescription[];

extern const char kReaderModeReadabilityDistillerName[];
extern const char kReaderModeReadabilityDistillerDescription[];

extern const char kReaderModeReadabilityHeuristicName[];
extern const char kReaderModeReadabilityHeuristicDescription[];

extern const char kReaderModeSupportNewFontsName[];
extern const char kReaderModeSupportNewFontsDescription[];

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

extern const char kSegmentationPlatformEphemeralCardRankerName[];
extern const char kSegmentationPlatformEphemeralCardRankerDescription[];

extern const char kSegmentationPlatformIosModuleRankerCachingName[];
extern const char kSegmentationPlatformIosModuleRankerCachingDescription[];

extern const char kSegmentationPlatformIosModuleRankerName[];
extern const char kSegmentationPlatformIosModuleRankerDescription[];

extern const char kSegmentationPlatformIosModuleRankerSplitBySurfaceName[];
extern const char
    kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription[];

extern const char kSendTabToSelfEnhancedHandoffName[];
extern const char kSendTabToSelfEnhancedHandoffDescription[];

extern const char kSendTabToSelfIOSPushNotificationsName[];
extern const char kSendTabToSelfIOSPushNotificationsDescription[];

extern const char kShareInOmniboxLongPressName[];
extern const char kShareInOmniboxLongPressDescription[];

extern const char kShareInOverflowMenuName[];
extern const char kShareInOverflowMenuDescription[];

extern const char kShareInVerbatimMatchName[];
extern const char kShareInVerbatimMatchDescription[];

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

extern const char kSkipDefaultBrowserPromoInFirstRunName[];
extern const char kSkipDefaultBrowserPromoInFirstRunDescription[];

extern const char kSmartTabGroupingName[];
extern const char kSmartTabGroupingDescription[];

extern const char kSmoothScrollingUseDelegateName[];
extern const char kSmoothScrollingUseDelegateDescription[];

extern const char kStrokesAPIEnabledName[];
extern const char kStrokesAPIEnabledDescription[];

extern const char kSupervisedUserBlockInterstitialV3Name[];
extern const char kSupervisedUserBlockInterstitialV3Description[];

extern const char kSupervisedUserEmitLogRecordSeparatelyName[];
extern const char kSupervisedUserEmitLogRecordSeparatelyDescription[];

extern const char
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsName[];
extern const char
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsDescription[];

extern const char kSupervisedUserUseUrlFilteringServiceName[];
extern const char kSupervisedUserUseUrlFilteringServiceDescription[];

extern const char kSyncAutofillWalletCredentialDataName[];
extern const char kSyncAutofillWalletCredentialDataDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncThemesIosName[];
extern const char kSyncThemesIosDescription[];

extern const char kSyncTrustedVaultInfobarMessageImprovementsName[];
extern const char kSyncTrustedVaultInfobarMessageImprovementsDescription[];

extern const char kTabGridDragAndDropName[];
extern const char kTabGridDragAndDropDescription[];

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

extern const char kUseSceneViewControllerName[];
extern const char kUseSceneViewControllerDescription[];

extern const char kVariationsExperimentalCorpusName[];
extern const char kVariationsExperimentalCorpusDescription[];

extern const char kVariationsRestrictDogfoodName[];
extern const char kVariationsRestrictDogfoodDescription[];

extern const char kViewCertificateInformationName[];
extern const char kViewCertificateInformationDescription[];

extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

extern const char kWebPageTextZoomIPadName[];
extern const char kWebPageTextZoomIPadDescription[];

extern const char kWelcomeBackName[];
extern const char kWelcomeBackDescription[];

extern const char kZeroStateSuggestionsName[];
extern const char kZeroStateSuggestionsDescription[];

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAIHubNewBadgeName[] = "AI Hub New Badge";
const char kAIHubNewBadgeDescription[] =
    "Enables showing a new badge on the AI Hub button in the toolbar.";

const char kAIMCobrowseDebugEntrypointName[] = "AIM Cobrowse debug entrypoint";
const char kAIMCobrowseDebugEntrypointDescription[] =
    "Enables the AIM Cobrowse debug entrypoint feature.";

const char kAIMEligibilityRefreshNTPModulesName[] =
    "AIMEligibilityRefreshNTPModules";
const char kAIMEligibilityRefreshNTPModulesDescription[] =
    "Enables the AIMEligibilityRefreshNTPModules feature.";

const char kAIMEligibilityServiceStartWithProfileName[] =
    "AIMEligibilityServiceStartWithProfile";
const char kAIMEligibilityServiceStartWithProfileDescription[] =
    "Start the AIM eligibility service with the profile.";

const char kAIMNTPEntrypointTabletName[] = "AIMNTPEntrypointTablet";
const char kAIMNTPEntrypointTabletDescription[] =
    "Enables the AIMNTPEntrypointTablet feature.";

const char kAimCobrowseHeaderName[] = "AimCobrowseHeader";
const char kAimCobrowseHeaderDescription[] =
    "Changes the design of the AIM cobrowse header.";

const char kAimCobrowseName[] = "AimCobrowse";
const char kAimCobrowseDescription[] = "Enables the AimCobrowse feature.";

const char kAimUrlNavigationFetchEnabledName[] = "AimUrlNavigationFetchEnabled";
const char kAimUrlNavigationFetchEnabledDescription[] =
    "Enables the AimUrlNavigationFetchEnabled feature.";

const char kAnimatedDefaultBrowserPromoInFREName[] =
    "Enable the animated Default Browser Promo in the FRE";
const char kAnimatedDefaultBrowserPromoInFREDescription[] =
    "When enabled, the Default Browser Promo in the FRE will be animated.";

const char kAppBackgroundRefreshName[] = "Enable app background refresh";
const char kAppBackgroundRefreshDescription[] =
    "Schedules app background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kAppleCalendarExperienceKitName[] = "Experience Kit Apple Calendar";
const char kAppleCalendarExperienceKitDescription[] =
    "When enabled, long pressing on dates will trigger Experience Kit Apple "
    "Calendar event handling.";

const char kApplyClientsideModelPredictionsForPasswordTypesName[] =
    "Apply clientside model predictions for password forms.";
const char kApplyClientsideModelPredictionsForPasswordTypesDescription[] =
    "Enable using clientside model predictions to fill password forms.";

const char kAskAboutThisPageName[] = "AskAboutThisPage";
const char kAskAboutThisPageDescription[] =
    "Enables the AskAboutThisPage feature.";

const char kAskGeminiChipName[] = "Ask Gemini Chip";
const char kAskGeminiChipDescription[] = "Enables the Ask Gemini Chip feature.";

const char kAssistantAimMinimizedStateName[] = "AssistantAimMinimizedState";
const char kAssistantAimMinimizedStateDescription[] =
    "When enabled, the Assistant AIM (Co-browse) interface initially appears "
    "in a minimized state instead of the default medium state.";

const char kAssistantContainerName[] = "Assistant Container";
const char kAssistantContainerDescription[] =
    "Enables the Assistant Container feature. The debug parameter enables "
    "debug elements and forces AIM eligibility.";

const char kAssistantSidePanelName[] = "AssistantSidePanel";
const char kAssistantSidePanelDescription[] =
    "Enables the AssistantSidePanel feature.";

const char kAutofillAcrossIframesName[] = "Enables Autofill across iframes";
const char kAutofillAcrossIframesDescription[] =
    "When enabled, Autofill will fill and save information on forms that "
    "spread across multiple iframes.";

const char kAutofillAiAvailableByDefaultName[] =
    "Autofill AI available by default";
const char kAutofillAiAvailableByDefaultDescription[] =
    "Makes Autofill AI available by default.";

const char kAutofillAiCreateEntityDataManagerName[] =
    "Autofill AI Create Entity Data Manager";
const char kAutofillAiCreateEntityDataManagerDescription[] =
    "Enables Autofill AI Create Entity Data Manager.";

const char kAutofillAiDedupeEntitiesName[] = "Autofill AI dedupe entities";
const char kAutofillAiDedupeEntitiesDescription[] =
    "Enables periodic deduplication of Autofill AI entities.";

const char kAutofillAiNoFillingIconsExperimentName[] =
    "Autofill AI no filling icons experiment";
const char kAutofillAiNoFillingIconsExperimentDescription[] =
    "If enabled, Autofill AI filling suggestions do not have an icon.";

const char kAutofillAiReauthRequiredName[] = "Autofill AI Reauth Required";
const char kAutofillAiReauthRequiredDescription[] =
    "Enables Autofill AI Reauth Required.";

const char kAutofillAiValuablesIPHName[] = "IPH Autofill AI Valuables";
const char kAutofillAiValuablesIPHDescription[] =
    "Enables the In-Product Help for Autofill AI valuables.";

const char kAutofillAiWalletFlightReservationName[] =
    "Autofill AI Google Wallet flight reservations";
const char kAutofillAiWalletFlightReservationDescription[] =
    "Enables Autofill AI support for flight reservation entities from Google "
    "Wallet.";

const char kAutofillAiWalletPrivatePassesDeepLinkName[] =
    "Autofill AI Google Wallet private passes deep link";
const char kAutofillAiWalletPrivatePassesDeepLinkDescription[] =
    "Enables Autofill AI support for deep linking to private passes from "
    "Google Wallet.";

const char kAutofillAiWalletPrivatePassesName[] =
    "Autofill AI Google Wallet private passes";
const char kAutofillAiWalletPrivatePassesDescription[] =
    "Enables Autofill AI support for private passes from Google Wallet.";

const char kAutofillAiWalletVehicleRegistrationName[] =
    "Autofill AI Google Wallet vehicle registration";
const char kAutofillAiWalletVehicleRegistrationDescription[] =
    "Enables Autofill AI support for vehicle registration entities from Google "
    "Wallet.";

const char kAutofillAiWithDataSchemaName[] = "Autofill AI With Data Schema";
const char kAutofillAiWithDataSchemaDescription[] =
    "Enables Autofill AI With Data Schema.";

const char kAutofillBottomSheetNewBlurName[] =
    "New Blur Method for Autofill Bottom Sheet";
const char kAutofillBottomSheetNewBlurDescription[] =
    "Enables a new method for blurring the autofill bottom sheet to prevent "
    "the keyboard from showing up. This uses `mousedown` instead of `focus`.";

const char kAutofillCreditCardScannerIosName[] =
    "Enable the credit card scanner for Autofill";
const char kAutofillCreditCardScannerIosDescription[] =
    "When enabled, users are offered the ability to use their phone camera to "
    "scan their credit card when adding it to Chrome Autofill";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDisableProfileUpdatesName[] =
    "Disables Autofill profile updates from form submissions";
const char kAutofillDisableProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply updates to address profiles based "
    "on data extracted from submitted forms. For testing purposes.";

const char kAutofillDisableSilentProfileUpdatesName[] =
    "Disables Autofill silent profile updates from form submissions";
const char kAutofillDisableSilentProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply silent updates to address profiles. "
    "For testing purposes.";

const char kAutofillEnableBottomSheetScanCardAndFillName[] =
    "Enable scan card BottomSheet, then save and fill of the credit card";
const char kAutofillEnableBottomSheetScanCardAndFillDescription[] =
    "When enabled, offers a card scanning BottomSheet and allows users to "
    "save and autofill credit cards in autofill forms.";

const char kAutofillEnableCardBenefitsForAmericanExpressName[] =
    "Enable showing American Express card benefits";
const char kAutofillEnableCardBenefitsForAmericanExpressDescription[] =
    "When enabled, card benefits offered by American Express will be shown in "
    "Autofill suggestions.";

const char kAutofillEnableCardBenefitsForBmoName[] =
    "Enable showing BMO card benefits";
const char kAutofillEnableCardBenefitsForBmoDescription[] =
    "When enabled, card benefits offered by BMO will be shown in Autofill "
    "suggestions.";

const char kAutofillEnableCardBenefitsSyncName[] =
    "Enable syncing card benefits from the server";
const char kAutofillEnableCardBenefitsSyncDescription[] =
    "When enabled, card benefits offered by issuers will be synced from "
    "the Payments server.";

const char kAutofillEnableCardInfoRuntimeRetrievalName[] =
    "Enable retrieval of card info(with CVC) from issuer for enrolled cards";
const char kAutofillEnableCardInfoRuntimeRetrievalDescription[] =
    "When enabled, runtime retrieval of CVC along with card number and expiry "
    "from issuer for enrolled cards will be enabled during form fill.";

const char kAutofillEnableFlatRateCardBenefitsFromCurinosName[] =
    "Enable showing flat rate card benefits sourced from Curinos";
const char kAutofillEnableFlatRateCardBenefitsFromCurinosDescription[] =
    "When enabled, flat rate card benefits sourced from Curinos will be shown "
    "in Autofill suggestions.";

const char kAutofillEnablePrefetchingRiskDataForRetrievalName[] =
    "Enable prefetching of risk data during payments autofill retrieval";
const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[] =
    "When enabled, risk data is prefetched during payments autofill flows "
    "to reduce user-perceived latency.";

const char kAutofillEnableSupportForHomeAndWorkName[] =
    "Enable support for home and work addresses";
const char kAutofillEnableSupportForHomeAndWorkDescription[] =
    "When enabled, chrome will support home and work addresses from account.";

const char kAutofillEnableSupportForNameAndEmailName[] =
    "Support for name and email addresses in Autofill";
const char kAutofillEnableSupportForNameAndEmailDescription[] =
    "When enabled, a name and email profile with data comming from the account "
    "will be created for autofilling.";

const char kAutofillEnableWalletBrandingName[] =
    "Update Google Pay branding to Wallet where applicable";
const char kAutofillEnableWalletBrandingDescription[] =
    "When enabled, certain strings and logos referencing Google Account, "
    "Google Payments, and Google Pay will instead reference Google Wallet.";

const char kAutofillEnableWalletBrandingV2Name[] =
    "Further update Google Pay and Google Wallet branding where applicable";
const char kAutofillEnableWalletBrandingV2Description[] =
    "When enabled, further brings certain strings and images referencing "
    "Google Pay and Google Wallet into consistency with branding requirements.";

const char kAutofillManualTestingDataName[] = "Autofill manual testing data";
const char kAutofillManualTestingDataDescription[] =
    "When set, imports the addresses and cards specified on startup. WARNING: "
    "If at least one address/card is specified, all other existing "
    "addresses/cards are overwritten.";

const char kAutofillPaymentsFieldSwappingName[] =
    "Swap credit card suggestions";
const char kAutofillPaymentsFieldSwappingDescription[] =
    "When enabled, swapping autofilled payment suggestions would result"
    "in overriding all of the payments fields with the swapped profile data";

const char kAutofillPaymentsSheetV2Name[] =
    "Enable the payments suggestion bottom sheet V2";
const char kAutofillPaymentsSheetV2Description[] =
    "When enabled, the V2 of the payments suggestion bottom sheet will be "
    "used.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSupportDateInputName[] = "Autofill support for date input";
const char kAutofillSupportDateInputDescription[] =
    "Enables form filling and saving capabilities for <input type=\"date\">.";

const char kAutofillThrottleDocumentFormScanName[] =
    "Throttle Autofill Document Form Scans";
const char kAutofillThrottleDocumentFormScanDescription[] =
    "Enables the throttling of the recurrent document form scans done by "
    "Autofill.";

const char kAutofillThrottleFilteredDocumentFormScanName[] =
    "Throttle Filtered Autofill Document Form Scans";
const char kAutofillThrottleFilteredDocumentFormScanDescription[] =
    "Enables the throttling of the on the spot filtered form scans done by "
    "Autofill (e.g. get the latest state of a form that had an activity).";

const char kAutofillUpstreamEnforceStrikeDelayName[] =
    "Require a week between offers to save credit cards";
const char kAutofillUpstreamEnforceStrikeDelayDescription[] =
    "When enabled, users should not see offers to save the same credit card "
    "twice in a week, as the strike database enforces a 7-day delay between "
    "strikes.";

const char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
const char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

const char kAutofillVcnEnrollStrikeExpiryTimeName[] =
    "Expiry duration for VCN enrollment strikes";
const char kAutofillVcnEnrollStrikeExpiryTimeDescription[] =
    "When enabled, changes the amount of time required for VCN enrollment "
    "prompt strikes to expire.";

const char kBWGPromoConsentName[] = "BWG Promo Consent";
const char kBWGPromoConsentDescription[] =
    "Whether the promo consent flow is composed of a single or a double screen "
    "view.";

const char kBackgroundRefreshRegressionTestName[] =
    "Background Refresh Regression Test";
const char kBackgroundRefreshRegressionTestDescription[] =
    "Enables the Background Refresh Regression Test with multiple arms "
    "to test various refresh and persistence parameters.";

const char kBestFeaturesScreenInFirstRunName[] =
    "Display Best Features screen in the FRE";
const char kBestFeaturesScreenInFirstRunDescription[] =
    "When enabled, displays the BestFeatures screen in the First Run sequence. "
    "Screen can be displayed either before or after the DB promo.";

const char kBestOfAppFREName[] = "Display Best of App view in the FRE";
const char kBestOfAppFREDescription[] =
    "When enabled, displays some views during the FRE highlighting the best "
    "features in the app.";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kBuildExternalPrivacyContextName[] =
    "Build external privacy context";
const char kBuildExternalPrivacyContextDescription[] =
    "When enabled, checks if the account can be signed in on the device "
    "according to the capabilities. This needs `can_sign_in_to_chrome` "
    "capability to be fetched (controlled by "
    "kEnforceCanSignInToChromeCapability flag).";

const char kCacheIdentityListInChromeName[] = "Cache identity list in chrome.";
const char kCacheIdentityListInChromeDescription[] =
    "Changes the implementation of the cache of the list of identities on "
    "device.";

const char kChromeNextIaName[] = "ChromeNextIa";
const char kChromeNextIaDescription[] = "Enables the chrome_next_ia feature.";

const char kCobrowseAimHistoryName[] = "CobrowseAimHistory";
const char kCobrowseAimHistoryDescription[] =
    "When enabled, the history button in cobrowse is shown and can display the "
    "list of all previous AIM conversations.";

const char kCollaborationMessagingName[] = "Collaboration Messaging";
const char kCollaborationMessagingDescription[] =
    "Enables the messaging framework within the collaboration feature, "
    "including features such as recent activity, dirty dots, and description "
    "action chips.";

const char kComposeboxAIMDisabledName[] = "ComposeboxAIMDisabled";
const char kComposeboxAIMDisabledDescription[] =
    "When enabled, AIM feature are disabled in the composebox.";

const char kComposeboxAIMNudgeName[] = "ComposeboxAIMNudge";
const char kComposeboxAIMNudgeDescription[] =
    "Enables the AIM nudge button in the composebox, tapping on the button "
    "enables AIM. This is conditionned by AIM availability.";

const char kComposeboxAdditionalAdvancedToolsName[] =
    "Enable additional advanced tools in composebox";
extern const char kComposeboxAdditionalAdvancedToolsDescription[] =
    "When enabled, the additional tools in the input plate are shown, such as "
    "canvas and the model picker";

const char kComposeboxAttachmentsTypedStateName[] =
    "Enable contextual suggestions for typed state";
const char kComposeboxAttachmentsTypedStateDescription[] =
    "Enables showing suggestions for multiple composebox attachments in a "
    "typed state.";

const char kComposeboxCloseButtonTopAlignName[] =
    "Align the close button in composebox to the top edge of the view";
const char kComposeboxCloseButtonTopAlignDescription[] =
    "If the user preference is set to top, enabling this feature aligns the "
    "compose box close button with the top edge of the input plate instead of "
    "centering.";

const char kComposeboxCompactModeName[] = "ComposeboxCompactMode";
const char kComposeboxCompactModeDescription[] =
    "Enables the compact composebox, adding attachment or enabling AIM will "
    "expand it to the regular size.";

const char kComposeboxConditionalPlusButtonName[] =
    "Composebox Conditional Plus Button";
const char kComposeboxConditionalPlusButtonDescription[] =
    "When enabled, hides the plus button when typing a URL in compact mode.";

const char kComposeboxDeepSearchName[] = "Enable Composebox Deep Search";
extern const char kComposeboxDeepSearchDescription[] =
    "Enables the deep search advanced tool in Composebox";

const char kComposeboxDevToolsName[] = "Enable Composebox Dev Tools";
const char kComposeboxDevToolsDescription[] =
    "Enables development tools for the composebox, allowing simulation of "
    "delays and failures.";

const char kComposeboxFetchContextualSuggestionsForMultipleAttachmentsName[] =
    "Enable Composebox Fetch Contextual Suggestions For multiple attachments";
const char
    kComposeboxFetchContextualSuggestionsForMultipleAttachmentsDescription[] =
        "Enables showing suggestions for multiple attachments";

const char kComposeboxForceTopName[] = "ComposeboxForceTop";
const char kComposeboxForceTopDescription[] =
    "Forces the composebox to be at the top.";

const char kComposeboxIOSName[] = "ComposeboxIOS";
const char kComposeboxIOSDescription[] =
    "Enables the composebox that replaces the regular omnibox in edit state.";

const char kComposeboxImmersiveSRPName[] =
    "Enable the immersive SRP within the composebox";
const char kComposeboxImmersiveSRPDescription[] =
    "When enabled, the composebox will open SRPs in an embedded web view.";

const char kComposeboxIpadName[] = "ComposeboxIpad";
const char kComposeboxIpadDescription[] = "Enables the composeboxIpad feature.";

const char kComposeboxPlusButtonBottomSheetName[] =
    "Enable the bottom sheet for plus button in Composebox";
const char kComposeboxPlusButtonBottomSheetDescription[] =
    "Uses the updated bottom sheet for the plus button multimodal menu.";

const char kComposeboxServerSideStateName[] =
    "Enable server side state in Composebox";
extern const char kComposeboxServerSideStateDescription[] =
    "When enabled, the server side state will be used in the composebox";

const char kConfirmationButtonSwapOrderName[] =
    "Swap Button Order in confirmation alerts";
const char kConfirmationButtonSwapOrderDescription[] =
    "Swaps the positions of the primary and secondary buttons in the "
    "confirmation alerts, so that the primary button is placed at the bottom.";

const char kConsistentLogoDoodleHeightName[] =
    "Consistent NTP Logo and Doodle Height";
const char kConsistentLogoDoodleHeightDescription[] =
    "Ensures the NTP Logo and Doodle have a consistent height to prevent "
    "content jumping.";

const char kContentNotificationProvisionalIgnoreConditionsName[] =
    "Content Notification Provisional Ignore Conditions";
const char kContentNotificationProvisionalIgnoreConditionsDescription[] =
    "Enable Content Notification Provisional without Conditions";

const char kContentPushNotificationsName[] = "Content Push Notifications";
const char kContentPushNotificationsDescription[] =
    "Enables the content push notifications.";

const char kCredentialProviderExtensionPromoName[] =
    "Enable the Credential Provider Extension promo.";
const char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, Credential Provider Extension promo will be "
    "presented to eligible users.";

const char kCredentialProviderPasskeyLargeBlobName[] =
    "Credential Provider Large Blob support";
const char kCredentialProviderPasskeyLargeBlobDescription[] =
    "Enables support for the Large Blob extension for Passkeys in the "
    "Credential Provider Extension.";

const char kCredentialProviderPasskeyPRFName[] =
    "Credential Provider PRF support";
const char kCredentialProviderPasskeyPRFDescription[] =
    "Enables support for the PRF extension for Passkeys in the Credential "
    "Provider Extension.";

const char kCredentialProviderPerformanceImprovementsName[] =
    "Credential Provider Performance Improvements";
const char kCredentialProviderPerformanceImprovementsDescription[] =
    "Enables a series of performance improvements for the Credential Provider "
    "Extension.";

const char kCrossDeviceSigninName[] = "Cross-Device Sign-in";
const char kCrossDeviceSigninDescription[] =
    "Guards the logic to start sign-in from a given QR Code.";

const char kDataSharingDebugLogsName[] = "Enable data sharing debug logs";
const char kDataSharingDebugLogsDescription[] =
    "Enables the data sharing infrastructure to log and save debug messages "
    "that can be shown in the internals page.";

const char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
const char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

const char kDataSharingName[] = "Data Sharing";
const char kDataSharingDescription[] =
    "Enabled Data Sharing related UI and features.";

const char kDataSharingSharedDataTypesEnabled[] = "Version out-of-date, no UI";
const char kDataSharingSharedDataTypesEnabledWithUi[] =
    "Version out-of-date, show UI ";

const char kDataSharingVersioningStatesName[] =
    "Data Sharing Versioning Test Scenarios";
const char kDataSharingVersioningStatesDescription[] =
    "Testing multiple scenarios for versioning.";

const char kDefaultBrowserOffCyclePromoName[] =
    "Default Browser off-cycle promo";
const char kDefaultBrowserOffCyclePromoDescription[] =
    "When enabled, an off-cycle default browser promo will be shown.";

const char kDefaultBrowserPictureInPictureName[] =
    "Default Browser Promo Picture in Picture";
const char kDefaultBrowserPictureInPictureDescription[] =
    "When enabled, default browser instructions will be displayed in "
    "picture-in-picture format over the iOS settings.";

const char kDefaultBrowserPromoIpadInstructionsName[] =
    "Default Browser Promo iPad Instructions";
const char kDefaultBrowserPromoIpadInstructionsDescription[] =
    "When enabled, displays default browser promo instructions specifically "
    "adapted for iPad.";

const char kDefaultBrowserPromoPropensityModelName[] =
    "Default Browser promo propensity model";
const char kDefaultBrowserPromoPropensityModelDescription[] =
    "When enabled, a propensity model will help make the determination of "
    "whether to show a default browser promo";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDisableAutofillStrikeSystemName[] =
    "Disable the Autofill strike system";
const char kDisableAutofillStrikeSystemDescription[] =
    "When enabled, the Autofill strike system will not block a feature from "
    "being offered.";

const char kDisableComposeboxFromAIMNTPName[] = "DisableComposeboxFromAIMNTP";
const char kDisableComposeboxFromAIMNTPDescription[] =
    "When enabled, the NTP entrypoint will always lead to the AIM webpage even "
    "when composebox is enabled.";

const char kDisableKeyboardAccessoryName[] =
    "Disable Omnibox Keyboard Accessory";
const char kDisableKeyboardAccessoryDescription[] =
    "Disables parts or all of omnibox keyboard accessory.";

const char kDisableLensCameraName[] = "Disable Lens camera experience";
const char kDisableLensCameraDescription[] =
    "When enabled, the option use Lens to search for images from your device "
    "camera menu when Google is the selected search engine, accessible from "
    "the home screen widget, new tab page, and keyboard, is disabled.";

const char kDisableShareButtonName[] = "Disable Share Button in Toolbar";
const char kDisableShareButtonDescription[] =
    "Hides the share button in toolbar.";

const char kDisableU18FeedbackIosName[] = "DisableU18FeedbackIos";
const char kDisableU18FeedbackIosDescription[] =
    "When enabled, the primary identity is set to the feedback UI when opened. "
    "The user is free add it to the feedback or not. Also the feedback cannot "
    "be sent if the primary user is under 18. When disabled, the feedback is "
    "anoymous";

const char kDownloadAutoDeletionClearFilesOnEveryStartupName[] =
    "Enable Download Auto-Deletion Testing Mode";
const char kDownloadAutoDeletionClearFilesOnEveryStartupDescription[] =
    "When enabled, the Auto-deletion feature wil clear all downloaded files "
    "scheduled for deletion on every application startup, regardless of when "
    "the file was downloaded. This feature is intended for testing-only.";

const char kDownloadAutoDeletionName[] = "Enable Download Auto Deletion";
const char kDownloadAutoDeletionDescription[] =
    "When enabled, files downloaded on the device can be scheduled to be "
    "deleted automatically after 30 days.";

const char kDownloadListName[] = "Enable Download List";
const char kDownloadListDescription[] =
    "Controls the UI type for the download list. When enabled, allows "
    "switching between default and custom UI implementations.";

const char kDownloadServiceForegroundSessionName[] =
    "Download service foreground download";
const char kDownloadServiceForegroundSessionDescription[] =
    "Enable download service to download in app foreground only";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableACPrefetchName[] = "Enable AC Prefetch";
const char kEnableACPrefetchDescription[] =
    "Ensures that account capabilities are prefetched and cached.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableClientCertificateProvisioningOnIOSName[] =
    "Enable client certificate provisioning on iOS";
const char kEnableClientCertificateProvisioningOnIOSDescription[] =
    "When enabled, client certificate provisioning from the cloud is allowed "
    "for enterprise users on iOS.";

const char kEnableCompromisedPasswordsMutingName[] =
    "Enable the muting of compromised passwords in the Password Manager";
const char kEnableCompromisedPasswordsMutingDescription[] =
    "Enable the compromised password alert mutings in Password Manager to be "
    "respected in the app.";

const char kEnableFamilyLinkControlsName[] = "Family Link parental controls";
const char kEnableFamilyLinkControlsDescription[] =
    "Enables parental controls from Family Link on supervised accounts "
    "signed-in to Chrome.";

const char kEnableFeedAblationName[] = "Enables Feed Ablation";
const char kEnableFeedAblationDescription[] =
    "If Enabled the Feed will be removed from the NTP";

const char kEnableFeedCardMenuSignInPromoName[] =
    "Enable Feed card menu sign-in promotion";
const char kEnableFeedCardMenuSignInPromoDescription[] =
    "Display a sign-in promotion UI when signed out users click on "
    "personalization options within the feed card menu.";

const char kEnableFeedHeaderSettingsName[] =
    "Enables the feed header settings.";
const char kEnableFeedHeaderSettingsDescription[] =
    "When enabled, some UI elements of the feed header can be modified.";

const char kEnableFileDownloadConnectorIOSName[] =
    "Enable file download connectors on iOS.";
const char kEnableFileDownloadConnectorIOSDescription[] =
    "When enabled, the enterprise DLP file download featured is available on "
    "iOS. ";

const char kEnableFuseboxKeyboardAccessoryName[] =
    "Enable Omnibox Keyboard Accessory in Fusebox";
extern const char kEnableFuseboxKeyboardAccessoryDescription[] =
    "Enables parts or all of omnibox keyboard accessory.";

const char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
const char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

const char kEnableNTPBackgroundImageCacheName[] =
    "Enable NTP Background Image Cache";
const char kEnableNTPBackgroundImageCacheDescription[] =
    "Enables the NTP background image cache service to improve performance.";

const char kEnableNewStartupFlowName[] = "EnableNewStartupFlow";
const char kEnableNewStartupFlowDescription[] =
    "Enables the EnableNewStartupFlow feature.";

const char kEnableReadingListAccountStorageName[] =
    "Enable Reading List Account Storage";
const char kEnableReadingListAccountStorageDescription[] =
    "Enable the reading list account storage.";

const char kEnableReadingListSignInPromoName[] =
    "Enable Reading List Sign-in promo";
const char kEnableReadingListSignInPromoDescription[] =
    "Enable the sign-in promo view in the reading list screen.";

const char kEnableScreenshotProtectionIOSName[] =
    "Enable Screenshot Protection on iOS";
const char kEnableScreenshotProtectionIOSDescription[] =
    "Prevents the content of the app from appearing in screenshots and screen "
    "recordings.";

const char kEnableTraitCollectionRegistrationName[] =
    "Enable Customizable Trait Registration";
const char kEnableTraitCollectionRegistrationDescription[] =
    "When enabled, UI elements will only observe and respond to the UITraits "
    "to which they have been registered.";

const char kEnforceCanSignInToChromeCapabilityName[] =
    "Fetch can_sign_in_to_chrome capability";
const char kEnforceCanSignInToChromeCapabilityDescription[] =
    "When enabled, can_sign_in_to_chrome is fetched.";

const char kEnhancedCalendarName[] = "Enable Enhanced Calendar integration";
const char kEnhancedCalendarDescription[] =
    "When enabled, the enhanced calendar flow will be available to eligible "
    "users when adding a calendar event.";

const char kExplainGeminiEditMenuName[] = "Enable Explain Gemini Edit Menu";
const char kExplainGeminiEditMenuDescription[] =
    "When enabled, the explain Gemini edit menu will be available to eligible "
    "users when highlighting any text on a web page.";

const char kFRESignInHeaderTextUpdateName[] =
    "Enable header text variations on the FRE sign-in page.";
const char kFRESignInHeaderTextUpdateDescription[] =
    "When enabled, the FRE sign-in page displays a different header text.";

const char kFeedBackgroundRefreshName[] = "Enable feed background refresh";
const char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kFeedSwipeInProductHelpName[] = "Enable Feed Swipe IPH";
const char kFeedSwipeInProductHelpDescription[] =
    "Presents an in-product help on the NTP to promote swiping on the Feed";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kFullscreenRefactoringName[] = "FullscreenRefactoring";
const char kFullscreenRefactoringDescription[] =
    "Enables the FullscreenRefactoring feature.";

const char kFullscreenScrollThresholdName[] = "Fullscreen Scroll Threshold";
const char kFullscreenScrollThresholdDescription[] =
    "When enabled, scrolling must exceed a small threshold before the web view "
    "begins to enter or exit fullscreen.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kFullscreenTransitionSpeedName[] =
    "Fullscreen Transition Speed Tweaks";
const char kFullscreenTransitionSpeedDescription[] =
    "When enabled, the speed of the fullscreen' transition is "
    "increased-decreased.";

const char kGeminiActorName[] = "Gemini Actor";
const char kGeminiActorDescription[] = "Enables the Gemini Actor.";

const char kGeminiBackendMigrationName[] = "Gemini Backend Migration";
const char kGeminiBackendMigrationDescription[] =
    "Enables the backend migration for Gemini.";

const char kGeminiBinaryMigrationName[] = "Gemini Binary Migration";
const char kGeminiBinaryMigrationDescription[] =
    "Enables the binary network migration for Gemini.";

const char kGeminiChatPersistenceName[] = "Gemini Chat Persistence";
const char kGeminiChatPersistenceDescription[] =
    "Enables improvements to Gemini Chat persistence.";

const char kGeminiClientMigrationName[] = "Gemini Client Migration";
const char kGeminiClientMigrationDescription[] =
    "Enables the client migration for Gemini, adding the infrastructure for "
    "several key features that render more than just text.";

const char kGeminiCopresenceName[] = "Gemini Copresence";
const char kGeminiCopresenceDescription[] =
    "Enables the Gemini Copresence feature, which provides a persistent Gemini "
    "overlay.";

const char kGeminiDynamicSettingsName[] = "Gemini Dynamic Settings";
const char kGeminiDynamicSettingsDescription[] =
    "Enables loading Gemini settings dynamically using the Gemini SDK.";

const char kGeminiFloatyAllPagesName[] = "Gemini Floaty All Pages";
const char kGeminiFloatyAllPagesDescription[] =
    "Enables the Gemini floaty on all pages.";

const char kGeminiImageRemixToolName[] = "Gemini Image Remix Tool";
const char kGeminiImageRemixToolDescription[] =
    "Enables the image remix tool in the Gemini floaty.";

const char kGeminiLiveName[] = "GeminiLive";
const char kGeminiLiveDescription[] = "Enables Gemini Live.";

const char kGeminiMapsRichUIName[] = "Gemini Maps Rich UI";
const char kGeminiMapsRichUIDescription[] =
    "Enables the rich Maps UI in Gemini.";

const char kGeminiMultiTabContextName[] = "Gemini Multi Tab Context";
const char kGeminiMultiTabContextDescription[] =
    "Enables attaching multiple tabs in Gemini.";

const char kGeminiNavigationPromoName[] = "GeminiNavigationPromo";
const char kGeminiNavigationPromoDescription[] =
    "Enables the automatic promo for Gemini on navigation.";

const char kGeminiPreciseLocationName[] = "BWG Precise Location";
const char kGeminiPreciseLocationDescription[] =
    "When enabled, the precise location row is shown in BWG settings.";

const char kGeminiRefactoredFREName[] = "Gemini Refactored FRE";
const char kGeminiRefactoredFREDescription[] =
    "Enables the refactored Gemini First Run Experience (FRE).";

const char kGeminiResponseViewDynamicResizingName[] =
    "Gemini Response View Dynamic Resizing";
const char kGeminiResponseViewDynamicResizingDescription[] =
    "Enables dynamic resizing for the Gemini response view.";

const char kGeminiRichAPCExtractionName[] = "Gemini Rich APC Extraction";
const char kGeminiRichAPCExtractionDescription[] =
    "Enables rich APC extraction for Gemini.";

const char kGeminiUnaryMigrationName[] = "Gemini Unary Migration";
const char kGeminiUnaryMigrationDescription[] =
    "Enables the unary network migration for Gemini.";

const char kGeminiUpdatedEligibilityName[] = "Gemini Updated Eligibility";
const char kGeminiUpdatedEligibilityDescription[] =
    "Enables the updated eligibility checks for Gemini users.";

const char kHandleMdmErrorsForDasherAccountsName[] =
    "Mdm error handling for dasher accounts";
const char kHandleMdmErrorsForDasherAccountsDescription[] =
    "Enables the mdm error handling feature for dasher accounts";

const char kHideFuseboxVoiceLensActionsName[] =
    "Hide Voice and Lens in Fusebox";
const char kHideFuseboxVoiceLensActionsDescription[] =
    "Hides voice and lens shortcuts in fusebox.";

const char kHideToolbarsInOverflowMenuName[] = "Hide Toolbars in Overflow menu";
const char kHideToolbarsInOverflowMenuDescription[] =
    "When enabled, adds a button in the overflow menu that force the "
    "fullscreen mode on iOS.";

const char kHttpsUpgradesName[] = "HTTPS Upgrades";
const char kHttpsUpgradesDescription[] =
    "When enabled, eligible navigations will automatically be upgraded to "
    "HTTPS.";

const char kIOSActorToolsName[] = "iOS Actor Tools";
const char kIOSActorToolsDescription[] = "Enables all actor tools on iOS.";

const char kIOSBrowserEditMenuMetricsName[] = "Browser edit menu metrics";
const char kIOSBrowserEditMenuMetricsDescription[] =
    "Collect metrics for edit menu usage.";

const char kIOSBrowserReportIncludeAllProfilesName[] =
    "Include all profiles in browser reports";
const char kIOSBrowserReportIncludeAllProfilesDescription[] =
    "When enabled, enterprise browser reports include all profiles (instead of "
    "only the current profile).";

const char kIOSChooseFromDriveName[] = "IOS Choose from Drive";
const char kIOSChooseFromDriveDescription[] =
    "Enables the Choose from Drive feature on iOS.";

const char kIOSChooseFromDriveSignedOutName[] = "Choose from Drive Signed Out";
const char kIOSChooseFromDriveSignedOutDescription[] =
    "Enables the Choose from Drive feature to signed out users.";

const char kIOSCobaltDeveloperModeName[] = "IOS Cobalt Developer Mode";
const char kIOSCobaltDeveloperModeDescription[] =
    "Enables the developer mode of the Cobalt feature on iOS.";

const char kIOSCobaltName[] = "IOS Cobalt";
const char kIOSCobaltDescription[] = "Enables the Cobalt feature on iOS.";

const char kIOSCustomFileUploadMenuName[] = "Custom file upload menu";
const char kIOSCustomFileUploadMenuDescription[] =
    "Enables the custom file upload menu implementation.";

const char kIOSDateToCalendarSignedOutName[] = "Date to Calendar Signed Out";
const char kIOSDateToCalendarSignedOutDescription[] =
    "When enabled, signed-out users can long-press detected dates to access "
    "the 'Add to Google Calendar' feature.";

const char kIOSDockingPromoV2Name[] = "Docking Promo V2";
const char kIOSDockingPromoV2Description[] =
    "When enabled, the user will be presented an animated, instructional "
    "promo V2 showing how to move Chrome to their native iOS dock.";

const char kIOSEnableCloudProfileReportingName[] =
    "Enable profile reporting on iOS";
const char kIOSEnableCloudProfileReportingDescription[] =
    "When enabled, profile reports will be reported to the user's "
    "organization.";

const char kIOSEnableRealtimeEventReportingName[] =
    "Enable realtime event reporting on iOS";
const char kIOSEnableRealtimeEventReportingDescription[] =
    "When enabled, realtime events will be reported to the user's "
    "organization.";

const char kIOSExpandedSetupListName[] = "Expanded Setup List";
const char kIOSExpandedSetupListDescription[] =
    "Enables a feature that adds new items in the Setup List.";

const char kIOSExpandedTipsName[] = "Expanded Tips Notifications";
const char kIOSExpandedTipsDescription[] =
    "Enables a feature that adds several new Tips Notifications that can be "
    "sent.";

const char kIOSKeyboardAccessoryDefaultViewName[] =
    "Default Input Accessory View";
const char kIOSKeyboardAccessoryDefaultViewDescription[] =
    "When enabled, a default Keyboard Accessory view with navigation buttons "
    "is provided for a <select> HTML element.";

const char kIOSKeyboardAccessoryTwoBubbleName[] =
    "Enable the two-bubble design for the Keyboard Accessory view";
const char kIOSKeyboardAccessoryTwoBubbleDescription[] =
    "When enabled, the two-bubble design is used for the Keyboard Accessory "
    "view.";

const char kIOSMiniMapUniversalLinkName[] =
    "Open Maps Universal links in native view.";
const char kIOSMiniMapUniversalLinkDescription[] =
    "When enabled, maps universal links on Google Page are opened in "
    "native views (under conditions).";

const char kIOSOmniboxAimServerEligibilityEnName[] =
    "AIM Server Eligibility EN locales";
const char kIOSOmniboxAimServerEligibilityEnDescription[] =
    "Enable AIM server eligibility checks for EN locales.";

const char kIOSOmniboxAimServerEligibilityName[] = "AIM Server Eligibility";
const char kIOSOmniboxAimServerEligibilityDescription[] =
    "Enable AIM server eligibility checks for all locales.";

const char kIOSOmniboxAimShortcutName[] = "Enable the omnibox aim shortcut";
const char kIOSOmniboxAimShortcutDescription[] =
    "When enabled, an aim shortcut entrypoint will be displayed when the "
    "omnibox is on edit mode.";

const char kIOSOneTapMiniMapRestrictionsName[] =
    "Revalidate detected addresses for one tap Mini Map.";
const char kIOSOneTapMiniMapRestrictionsDescription[] =
    "Different restrictions to block false positive for one tap Mini Map.";

const char kIOSOneTimeDefaultBrowserNotificationName[] =
    "One-time default browser notification";
const char kIOSOneTimeDefaultBrowserNotificationDescription[] =
    "Enables a one-time notification to prompt the user to set the app as the "
    "default browser.";

const char kIOSProactivePasswordGenerationBottomSheetName[] =
    "IOS Proactive Password Generation Bottom Sheet";
const char kIOSProactivePasswordGenerationBottomSheetDescription[] =
    "Enables the display of the proactive password generation bottom sheet on "
    "IOS.";

const char kIOSProvidesAppNotificationSettingsName[] =
    "IOS Provides App Notification Settings";
const char kIOSProvidesAppNotificationSettingsDescription[] =
    "Enabled integration with iOS's ProvidesAppNotificationSettings feature.";

const char kIOSSaveToDriveClientFolderName[] = "Save to Drive client folder";
const char kIOSSaveToDriveClientFolderDescription[] =
    "Enables a feature to use a client folder API for Save to Drive on iOS.";

const char kIOSSaveToDriveSignedOutName[] = "Save to Drive Signed Out";
const char kIOSSaveToDriveSignedOutDescription[] =
    "Enables the Save to Drive feature to signed out users.";

const char kIOSSaveToPhotosSignedOutName[] = "Save to Photos Signed Out";
const char kIOSSaveToPhotosSignedOutDescription[] =
    "Enables the Save to Photos feature to signed out users.";

const char kIOSSoftLockName[] = "Soft Lock on iOS";
const char kIOSSoftLockDescription[] = "Enables experimental Soft Lock on iOS.";

const char kIOSSyncedSetUpName[] = "Synced Set Up";
const char kIOSSyncedSetUpDescription[] =
    "Enables the Synced Set Up experience, allowing the user to locally apply "
    "settings from their synced devices.";

const char kIOSTabRemindersName[] = "Tab Reminders";
const char kIOSTabRemindersDescription[] =
    "Enables the Tab Reminder notifications feature on iOS.";

const char kIOSTipsNotificationsStringAlternativesName[] =
    "Tips notifications alternative string experiment";
const char kIOSTipsNotificationsStringAlternativesDescription[] =
    "Enables different alternative strings for tips notifications";

const char kIOSTrustedVaultNotificationName[] =
    "Enable the trusted vault notification on iOS";
const char kIOSTrustedVaultNotificationDescription[] =
    "When enabled and when the trusted vault key is missing, the provisional "
    "notification will be delivered.";

const char kIOSWebContextMenuNewTitleName[] =
    "Use the new title for the Web context menu";
const char kIOSWebContextMenuNewTitleDescription[] =
    "Enables actions in context menu title instead of customized action for "
    "web context menu.";

const char kIdentityConfirmationSnackbarName[] =
    "Identity Confirmation Snackbar";
const char kIdentityConfirmationSnackbarDescription[] =
    "When enabled, the identity confirmation snackbar will show on startup.";

const char kInFlowTrustedVaultKeyRetrievalIosName[] =
    "In-flow Trusted Vault key retrieval";
const char kInFlowTrustedVaultKeyRetrievalIosDescription[] =
    "Starts the key retrieval flow after offering to save a password.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kIndicateIdentityErrorInOverflowMenuName[] =
    "Indicate Identity Error in Overflow Menu";
const char kIndicateIdentityErrorInOverflowMenuDescription[] =
    "When enabled, the Overflow Menu indicates the identity error with an "
    "error badge on the Settings destination";

const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[] =
    "Invalidate search engine choice after device restore";
const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[] =
    "When enabled, search engine choices made before backup & restore will not "
    "be considered valid on the restored device, leading to the choice screen "
    "potentially retriggering.";

const char kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName[] =
    "Lens blocks fetch objects interaction RPCs on separate handshake";
const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription[] =
        "When enabled, RPCs are blocked on separate handshake.";

const char kLensCameraNoStillOutputRequiredName[] =
    "Lens camera avoids creating unused outputs";
const char kLensCameraNoStillOutputRequiredDescription[] =
    "When enabled, Lens camera doesn't create unused still output.";

const char kLensCameraUnbinnedCaptureFormatsPreferredName[] =
    "Lens camera prefers unbinned formats";
const char kLensCameraUnbinnedCaptureFormatsPreferredDescription[] =
    "When enabled, Lens camera prefers unbinned pixel formats.";

const char kLensContinuousZoomEnabledName[] =
    "Enable Lens camera continuous zoom";
const char kLensContinuousZoomEnabledDescription[] =
    "When enabled, Lens camera supports continuous zoom.";

const char kLensEnableSendRawFileMediaTypesName[] =
    "Lens enable send raw file media types";
const char kLensEnableSendRawFileMediaTypesDescription[] =
    "Enables sending raw file media types in the Lens overlay.";

const char kLensEnableSendUrlsInComposeboxesName[] =
    "Lens enable send urls in composeboxes";
const char kLensEnableSendUrlsInComposeboxesDescription[] =
    "Enables sending urls in AIM composeboxes.";

const char kLensExactMatchesEnabledName[] = "Lens exact matches enabled";
const char kLensExactMatchesEnabledDescription[] =
    "Enables exact matches in the Lens results.";

const char kLensFetchSrpApiEnabledName[] = "Lens fetch SRP API enabled";
const char kLensFetchSrpApiEnabledDescription[] = "Enables the fetch SRP API.";

const char kLensFilterToggleEnabledName[] = "Lens filter toggle enabled";
const char kLensFilterToggleEnabledDescription[] =
    "Enables the filter toggle in Lens camera.";

const char kLensFiltersAblationModeEnabledName[] =
    "Lens filters ablation mode enabled";
const char kLensFiltersAblationModeEnabledDescription[] =
    "Enables the filters ablation mode.";

const char kLensGestureTextSelectionDisabledName[] =
    "Disable Lens gesture text selection";
const char kLensGestureTextSelectionDisabledDescription[] =
    "When disabled, turns off gesture text selection.";

const char kLensInitialLvfZoomLevel90PercentName[] =
    "Initial Lens camera zoom 90 percent";
const char kLensInitialLvfZoomLevel90PercentDescription[] =
    "When enabled, sets the initial Lens camera zoom level to 90 percent.";

const char kLensLoadAIMInLensResultPageName[] =
    "Enable loading AIM in the Lens result page";
const char kLensLoadAIMInLensResultPageDescription[] =
    "Opens in Lens result page rather than a new tab.";

const char kLensOmnientShaderV2EnabledName[] = "Enable Lens Omnient Shader V2";
const char kLensOmnientShaderV2EnabledDescription[] =
    "When enabled, Lens Omnient will use the new Shader V2";

const char kLensOverlayCustomBottomSheetName[] =
    "Use a custom bottom sheet presentation for Lens Overlay";
const char kLensOverlayCustomBottomSheetDescription[] =
    "When enabled the system bottom sheet for the Lens result page is "
    "replaced by a custom bottom sheet presentation";

const char kLensOverlayEnableLandscapeCompatibilityName[] =
    "Allow Lens overlay to also run in landscape if the feature is enabled";
const char kLensOverlayEnableLandscapeCompatibilityDescription[] =
    "When enabled, it allows Lens Overlay to run in landscape orientation";

const char kLensOverlayNavigationHistoryName[] =
    "Enable Lens overlay navigation history";
const char kLensOverlayNavigationHistoryDescription[] =
    "When enabled, web navigation in the Lens overlay are recorded in browser "
    "history.";

const char kLensPrewarmHardStickinessInInputSelectionName[] =
    "Lens prewarm hard stickiness in input selection";
const char kLensPrewarmHardStickinessInInputSelectionDescription[] =
    "When enabled, input selection prewarms hard stickiness.";

const char kLensPrewarmHardStickinessInQueryFormulationName[] =
    "Lens prewarm hard stickiness in query formulation";
const char kLensPrewarmHardStickinessInQueryFormulationDescription[] =
    "When enabled, query formulation prewarms hard stickiness.";

const char kLensSearchHeadersCheckEnabledName[] = "Lens search headers check";
const char kLensSearchHeadersCheckEnabledDescription[] =
    "When enabled, ensures headers are attached to Lens search requests.";

const char kLensSingleTapTextSelectionDisabledName[] =
    "Disable Lens single tap text selection";
const char kLensSingleTapTextSelectionDisabledDescription[] =
    "When disabled, single taps do not trigger text selections.";

const char kLensStreamServiceWebChannelTransportEnabledName[] =
    "Lens stream service web channel transport";
const char kLensStreamServiceWebChannelTransportEnabledDescription[] =
    "When enabled, uses web channel transport for the stream service.";

const char kLensTranslateToggleModeEnabledName[] =
    "Lens translate toggle mode enabled";
const char kLensTranslateToggleModeEnabledDescription[] =
    "Enables the translate toggle mode.";

const char kLensTripleCameraEnabledName[] = "Enable Lens triple camera";
const char kLensTripleCameraEnabledDescription[] =
    "When enabled, Lens LVF uses virtual triple camera.";

const char kLensUnaryApiSalientTextEnabledName[] =
    "Lens unary API salient text enabled";
const char kLensUnaryApiSalientTextEnabledDescription[] =
    "Enables the unary salient text API.";

const char kLensUnaryApisWithHttpTransportEnabledName[] =
    "Lens unary APIs with HTTP transport enabled";
const char kLensUnaryApisWithHttpTransportEnabledDescription[] =
    "Enables the unary APIs with HTTP transport.";

const char kLensUnaryHttpTransportEnabledName[] =
    "Lens unary HTTP transport enabled";
const char kLensUnaryHttpTransportEnabledDescription[] =
    "Enables the HTTP transport for unary requests.";

const char kLocationBarBadgeMigrationName[] = "LocationBarBadgeMigration";
const char kLocationBarBadgeMigrationDescription[] =
    "Enables the LocationBarBadgeMigration feature.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kManualLogUploadsInFREName[] = "Manual log uploads in the FRE";
const char kManualLogUploadsInFREDescription[] =
    "Enables triggering an UMA log upload after each FRE screen.";

const char kMeasurementsName[] = "Measurements experience enable";
const char kMeasurementsDescription[] =
    "When enabled, one tapping or long pressing on a measurement will trigger "
    "the measurement conversion experience.";

const char kMetrickitNonCrashReportName[] = "Metrickit non-crash reports";
const char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

const char kMigrateIOSKeychainAccessibilityName[] =
    "Migrate iOS Keychain Accessibility";
const char kMigrateIOSKeychainAccessibilityDescription[] =
    "Migrate the accessibility attribute in the iOS keychain to 'after first "
    "unlock'.";

const char kMobilePromoOnDesktopName[] = "Mobile Promo On Desktop";
const char kMobilePromoOnDesktopDescription[] =
    "When enabled, shows a mobile promo on the desktop new tab page.";

const char kMobilePromoOnDesktopRecordActiveDaysName[] =
    "Mobile Promo On Desktop Record Active Days";
const char kMobilePromoOnDesktopRecordActiveDaysDescription[] =
    "When enabled, records the user's number of active days for the mobile "
    "promo on desktop.";

const char kMobilePromoOnDesktopWave1Name[] =
    "Mobile Promo On Desktop (Wave 1)";
const char kMobilePromoOnDesktopWave1Description[] =
    "When enabled, shows a mobile promo with a reminder flow on desktop for "
    "eligible users. This version highlights features not included in the "
    "existing mobile promos.";

const char kModelBasedPageClassificationName[] =
    "Model Based Page Classification";
const char kModelBasedPageClassificationDescription[] =
    "Enables the model based page classification.";

const char kMostVisitedTilesCustomizationName[] =
    "Most Visited Tiles Customization on iOS";
const char kMostVisitedTilesCustomizationDescription[] =
    "Enables customization of Most Visited tiles on the New Tab Page.";

const char kMostVisitedTilesHorizontalRenderGroupName[] =
    "MVTiles Horizontal Render Group";
const char kMostVisitedTilesHorizontalRenderGroupDescription[] =
    "When enabled, the MV tiles are represented as individual matches";

const char kNTPBackgroundColorSliderName[] =
    "Enable the background color slider in the background customization color "
    "picker";
const char kNTPBackgroundColorSliderDescription[] =
    "When enabled, the color slider is available in the background "
    "customization color picker.";

const char kNTPBackgroundCustomizationName[] =
    "Enable background customization menu on the NTP";
const char kNTPBackgroundCustomizationDescription[] =
    "When enabled, the background customization menu is available on the NTP.";

const char kNTPBackgroundDownsampleImageName[] =
    "NTP Background Downsample Image";
const char kNTPBackgroundDownsampleImageDescription[] =
    "Downsamples user-uploaded NTP background images to screen size, "
    "reducing memory usage.";

const char kNTPHeaderUseTransformsForAnimationsName[] =
    "NTP Header Transform Animations";
const char kNTPHeaderUseTransformsForAnimationsDescription[] =
    "Use high-performance transforms for NTP header animations instead of "
    "updating constraints on scroll.";

const char kNativeFindInPageName[] = "Native Find in Page";
const char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

const char kNewTabPageFieldTrialName[] =
    "New tab page features that target new users";
const char kNewTabPageFieldTrialDescription[] =
    "Enables new tab page features that are available on first run for new "
    "Chrome iOS users.";

const char kNoAccountWebSigninName[] =
    "Enable no account web sigin bottom sheet";
const char kNoAccountWebSigninDescription[] =
    "Surfaces the web sign in bottom sheet when the user attempts to sign in "
    "to the web.";

const char kNonModalSignInPromoName[] = "Non-modal sign-in promo";
const char kNonModalSignInPromoDescription[] =
    "Enables a non-modal sign-in promo that prompts users to sign in.";

const char kNotificationCollisionManagementName[] =
    "Notification collision management";
const char kNotificationCollisionManagementDescription[] =
    "Enables delays to notifications to space them out more";

const char kNtpAlphaBackgroundCollectionsName[] =
    "Enable alpha background collections";
const char kNtpAlphaBackgroundCollectionsDescription[] =
    "When enabled, the alpha background collections are available on the NTP.";

const char kNtpComposeboxUsesChromeComposeClientName[] =
    "Enable composebox to use the suggest chrome compose client";
const char kNtpComposeboxUsesChromeComposeClientDescription[] =
    "When enabled, the composebox will use the suggest chrome compose client "
    "when AIM is enabled";

const char kOmniboxCrashFixKillSwitchName[] = "OmniboxCrashFixKillSwitch";
const char kOmniboxCrashFixKillSwitchDescription[] =
    "Enables the OmniboxCrashFixKillSwitch feature.";

const char kOmniboxDRSPrototypeName[] = "Enable the Omnibox DRS prototype";
const char kOmniboxDRSPrototypeDescription[] =
    "Enables the omnibox dynamic response system prototype";

const char kOmniboxGroupingFrameworkForTypedSuggestionsName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxGroupingFrameworkForZPSName[] =
    "Omnibox Grouping Framework for ZPS";
const char kOmniboxGroupingFrameworkForZPSDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxHttpsUpgradesName[] = "Omnibox HTTPS upgrades";
const char kOmniboxHttpsUpgradesDescription[] =
    "Enables HTTPS upgrades for omnibox navigations typed without a scheme";

const char kOmniboxInspireMeSignedOutName[] =
    "Omnibox Trending Queries For Signed-Out users";
const char kOmniboxInspireMeSignedOutDescription[] =
    "When enabled, appends additional suggestions based on local trends and "
    "optionally extends the ZPS limit (for signed out users).";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "Limit the number of URL suggestions in the omnibox. The omnibox will "
    "still display more than MaxURLMatches if there are no non-URL suggestions "
    "to replace them.";

const char kOmniboxMiaZpsName[] = "Omnibox Mia ZPS on NTP";
const char kOmniboxMiaZpsDescription[] =
    "Enables Mia ZPS suggestions in NTP omnibox";

const char kOmniboxMlLogUrlScoringSignalsName[] =
    "Log Omnibox URL Scoring Signals";
const char kOmniboxMlLogUrlScoringSignalsDescription[] =
    "Enables Omnibox to log scoring signals of URL suggestions.";

const char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[] =
    "Omnibox ML Scoring with Piecewise Score Mapping";
const char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores using "
    "a piecewise ML score mapping function.";

const char kOmniboxMlUrlScoreCachingName[] = "Omnibox ML URL Score Caching";
const char kOmniboxMlUrlScoreCachingDescription[] =
    "Enables in-memory caching of ML URL scores.";

const char kOmniboxMlUrlScoringModelName[] = "Omnibox URL Scoring Model";
const char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL sugestions.";

const char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
const char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

const char kOmniboxMlUrlSearchBlendingName[] = "Omnibox ML URL Search Blending";
const char kOmniboxMlUrlSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores.";

const char kOmniboxOnClobberFocusTypeOnIOSName[] =
    "Omnibox On Clobber Focus Type On IOS";
const char kOmniboxOnClobberFocusTypeOnIOSDescription[] =
    "Send ON_CLOBBER focus type for zero-prefix requests with an empty input "
    "on Web/SRP on IOS platform.";

const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for incognito";

const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[] =
    "Omnibox on device head suggestions (non-incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for non-incognito";

const char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
const char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

const char kOmniboxSuggestionAnswerMigrationName[] =
    "Omnibox suggestion answer migration";
const char kOmniboxSuggestionAnswerMigrationDescription[] =
    "Enables omnibox Suggestion answer migration, when enabled the omnibox "
    "will use the migrated Answer_template instead of answer.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxZeroSuggestPrefetchingOnSRPName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on SRP";
const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Search Results page.";

const char kOmniboxZeroSuggestPrefetchingOnWebName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on the Web";
const char kOmniboxZeroSuggestPrefetchingOnWebDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Web (i.e. non-NTP and non-SRP URLs).";

const char kOpenEditGroupViewByTappingTitleName[] =
    "OpenEditGroupViewByTappingTitle";
const char kOpenEditGroupViewByTappingTitleDescription[] =
    "Enables the OpenEditGroupViewByTappingTitle feature.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kPageActionMenuAuthFlowName[] = "Page Action Menu Auth Flow";
const char kPageActionMenuAuthFlowDescription[] =
    "When enabled, the Page Action Menu entry point becomes stable and "
    "supports the Ask Gemini auth flow.";

const char kPageActionMenuIconName[] = "PageActionMenuIcon";
const char kPageActionMenuIconDescription[] =
    "When enabled, changes the icon for the page action menu entry point.";

const char kPageActionMenuName[] = "Page Action Menu";
const char kPageActionMenuDescription[] =
    "When enabled, the entry point for the Page Action Menu becomes available "
    "for actions relating to the web page.";

const char kPageContentAnnotationsName[] = "Page content annotations";
const char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

const char kPageContentAnnotationsRemotePageMetadataName[] =
    "Page content annotations - Remote page metadata";
const char kPageContentAnnotationsRemotePageMetadataDescription[] =
    "Enables fetching of page load metadata to be persisted on-device.";

const char kPageContextIPCOptimizationName[] = "PageContextIPCOptimization";
const char kPageContextIPCOptimizationDescription[] =
    "Enables the PageContextIPCOptimization feature.";

const char kPageToolsFeatureUnavailabilityName[] =
    "PageToolsFeatureUnavailability";
const char kPageToolsFeatureUnavailabilityDescription[] =
    "Enables the PageToolsFeatureUnavailability feature.";

const char kPasswordRemovalFromDeleteBrowsingDataName[] =
    "Removal of Passwords from Quick Delete Browsing Data";
const char kPasswordRemovalFromDeleteBrowsingDataDescription[] =
    "Disables the deletion of passwords via the quick delete bottom sheet. "
    "Enables a new navigational view towards the appropriate pages to delete "
    "passwords or manage other Google data (Search History and My Activities).";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kPasswordSharingName[] = "Enables password sharing";
const char kPasswordSharingDescription[] =
    "Enables password sharing between members of the same family.";

const char kPersistTabContextName[] = "Persist Tab APC and Inner Text";
const char kPersistTabContextDescription[] =
    "Enables persisting tab APC and inner text in storage for fast access to "
    "multi-tab context.";

const char kPersistTabContextRichExtractionName[] =
    "PersistTabContextRichExtraction";
const char kPersistTabContextRichExtractionDescription[] =
    "Enables the PersistTabContextRichExtraction feature.";

const char kPersistentDefaultBrowserPromoName[] =
    "Persist default browser promo through app backgrounding";
const char kPersistentDefaultBrowserPromoDescription[] =
    "When enabled, the default browser promo will persist through "
    "backgrounding the app so the instructions remain visible when coming "
    "back.";

const char kPhoneNumberName[] = "Phone number experience enable";
const char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

const char kPlusButtonInFakeboxName[] = "Enable plus button in fakebox NTP";
const char kPlusButtonInFakeboxDescription[] =
    "When enabled, the fakebox NTP can contain a plus button for multimodal "
    "actions";

const char kPriceTrackingPromoName[] =
    "Enables price tracking notification promo card";
const char kPriceTrackingPromoDescription[] =
    "Enables being able to show the card in the Magic Stack";

const char kProactiveSuggestionsFrameworkName[] =
    "Proactive Suggestions Framework";
const char kProactiveSuggestionsFrameworkDescription[] =
    "When enabled, consolidates omnibox proactive suggestions (Reader Mode, "
    "Translate, Price History, etc.) into a unified badge system with "
    "centralized settings access through the AI Hub Page Tools.";

const char kProactiveSuggestionsFrameworkPopupBlockerName[] = "Popup Blocker";
const char kProactiveSuggestionsFrameworkPopupBlockerDescription[] =
    "Enables the popup blocker feature row in the Page Action Menu.";

const char kProvisionalNotificationAlertName[] =
    "Provisional notifiation alert on iOS";
const char kProvisionalNotificationAlertDescription[] =
    "Shows an alert to the user when app notification settings are changed but "
    "only provisonal notifications are enabled";

const char kRcapsDynamicProfileCountryName[] = "Dynamic Profile Country";
const char kRcapsDynamicProfileCountryDescription[] =
    "When enabled, Chrome updates the country associated with "
    "the profile on open";

const char kReaderModeContentSettingsForLinksName[] =
    "Enables Content Settings options for Reading Mode";
const char kReaderModeContentSettingsForLinksDescription[] =
    "Enables Content Settings options for disabling/enabling links in Reading "
    "Mode.";

const char kReaderModeIgnoreBadgeThresholdName[] =
    "Reader Mode ignore badge threshold";
const char kReaderModeIgnoreBadgeThresholdDescription[] =
    "When enabled, the badge threshold is ignored for Reader Mode.";

const char kReaderModeOmniboxEntrypointInUSName[] =
    "Reader Mode Omnibox Entrypoint In US";
const char kReaderModeOmniboxEntrypointInUSDescription[] =
    "Enables the omnibox entrypoint for Reader Mode for users in the US.";

const char kReaderModeOptimizationGuideEligibilityName[] =
    "Enables Reader Mode Optimization Guide Eligibility";
const char kReaderModeOptimizationGuideEligibilityDescription[] =
    "Enables the optimization guide eligibility check for Reader Mode.";

const char kReaderModeReadabilityDistillerName[] =
    "Enables Readability distiller for Reader Mode";
const char kReaderModeReadabilityDistillerDescription[] =
    "Enables Readability distiller for Reader Mode UI.";

const char kReaderModeReadabilityHeuristicName[] =
    "Enables Readability heuristic for Reader Mode";
const char kReaderModeReadabilityHeuristicDescription[] =
    "Enables Readability heuristic for Reader Mode UI.";

const char kReaderModeSupportNewFontsName[] = "Reader Mode support new fonts";
const char kReaderModeSupportNewFontsDescription[] =
    "Enables new accessible font options in Reader Mode.";

const char kReaderModeTranslationWithInfobarName[] =
    "Enables Reader Mode Translation Settings";
const char kReaderModeTranslationWithInfobarDescription[] =
    "Enables translation of web pages in Reader Mode with Settings available "
    "via the infobar.";

const char kReaderModeUSEnabledName[] = "Enables Reader Mode in US";
const char kReaderModeUSEnabledDescription[] =
    "Enables Reader Mode for users in the US. Requires reader-mode-enabled.";

const char kRefactorToolbarsSizeName[] = "Refactor toolbars size";
const char kRefactorToolbarsSizeDescription[] =
    "When enabled, the toolbars size does not use broadcaster but observers.";

const char kRemoveExcessNTPsExperimentName[] = "Remove extra New Tab Pages";
const char kRemoveExcessNTPsExperimentDescription[] =
    "When enabled, extra tabs with the New Tab Page open and no navigation "
    "history will be removed.";

const char kSafeBrowsingAvailableName[] = "Make Safe Browsing available";
const char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

const char kSafeBrowsingLocalListsUseSBv5Name[] =
    "Safe Browsing Local Lists use v5 API";
const char kSafeBrowsingLocalListsUseSBv5Description[] =
    "Fetch and check local lists using the Safe Browsing v5 API instead of the "
    "v4 Update API.";

const char kSafeBrowsingRealTimeLookupName[] = "Enable real-time Safe Browsing";
const char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

const char kSafeBrowsingTrustedURLName[] =
    "Enable the Trusted URL for Safe Browsing";
const char kSafeBrowsingTrustedURLDescription[] =
    "When enabled, chrome://safe-browsing will be accessible.";

const char kSegmentationPlatformEphemeralCardRankerName[] =
    "Enable Segmentation Ranking for Ephemeral Cards";
const char kSegmentationPlatformEphemeralCardRankerDescription[] =
    "Enables the segmentation platform to rank ephemeral cards in the Magic "
    "Stack";

const char kSegmentationPlatformIosModuleRankerCachingName[] =
    "Enabled Magic Stack Segmentation Ranking Caching";
const char kSegmentationPlatformIosModuleRankerCachingDescription[] =
    "Enables the Segmentation platform to cache the Magic Stack module rank "
    "for Start";

const char kSegmentationPlatformIosModuleRankerName[] =
    "Enable Magic Stack Segmentation Ranking";
const char kSegmentationPlatformIosModuleRankerDescription[] =
    "Enables the Segmentation platform to rank Magic Stack modules";

const char kSegmentationPlatformIosModuleRankerSplitBySurfaceName[] =
    "Enable Magic Stack Segmentation Ranking split by surface";
const char kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription[] =
    "Enables the Magic Stack module ranking to be split by surface for "
    "engagement";

const char kSendTabToSelfEnhancedHandoffName[] =
    "Send Tab To Self enhanced handoff";
const char kSendTabToSelfEnhancedHandoffDescription[] =
    "Enables an enhanced version of Send Tab To Self that propagates more "
    "information, such as form fields.";

const char kShareInOmniboxLongPressName[] = "Share in Omnibox Long Press";
const char kShareInOmniboxLongPressDescription[] =
    "Displays an option to share current page in the omnibox long press menu";

const char kShareInOverflowMenuName[] = "Share in Overflow Menu";
const char kShareInOverflowMenuDescription[] =
    "Displays share menu item in overflow menu";

const char kShareInVerbatimMatchName[] = "Share in Verbatim Match";
const char kShareInVerbatimMatchDescription[] =
    "Displays share button in the omnibox verbatim match";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

const char kShopCardName[] = "Enables Tab Resumption ShopCard";
const char kShopCardDescription[] =
    "Enables being able to show Tab Resumption ShopCard in the Magic Stack";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowTabGroupInGridOnStartName[] = "Show tab group in grid on start";
const char kShowTabGroupInGridOnStartDescription[] =
    "Show tab group in grid on start if the last activation is within a "
    "specific time interval";

const char kSkipDefaultBrowserPromoInFirstRunName[] =
    "Skip the FRE Default Browser Promo in EEA";
const char kSkipDefaultBrowserPromoInFirstRunDescription[] =
    "When enabled, users in the EEA will not see a Default Browser Promo in "
    "the FRE.";

const char kSmartTabGroupingName[] = "Enable Smart Tab Grouping";
const char kSmartTabGroupingDescription[] =
    "When enabled, users will have access to use the smart tab grouping "
    "feature in the tab grid.";

const char kSmoothScrollingUseDelegateName[] =
    "Fullscreen Smooth Scrolling No Broadcaster";
const char kSmoothScrollingUseDelegateDescription[] =
    "When enabled, the SmoothScrollingDefault experiment uses the regular "
    "UIScrollViewDelegate instead of KVO and broadcasting.";

const char kSnapshotCompressedJPEGQualityName[] =
    "Snapshot Compressed JPEG Quality";
const char kSnapshotCompressedJPEGQualityDescription[] =
    "Reduces snapshot JPEG quality from 1.0 to 0.97 for visually lossless "
    "compression, reducing file size by ~3-5x.";

const char kSnapshotDownsampleImageName[] =
    "Snapshot Downsample Image";
const char kSnapshotDownsampleImageDescription[] =
    "Downsamples tab snapshots to half resolution before writing to disk, "
    "reducing storage and I/O while keeping full resolution in memory.";

const char kStrokesAPIEnabledName[] = "Enable Strokes API for Lens";
const char kStrokesAPIEnabledDescription[] =
    "When enabled, Lens will use the Strokes API.";

const char kSupervisedUserBlockInterstitialV3Name[] =
    "Enable URL filter interstitial V3";
const char kSupervisedUserBlockInterstitialV3Description[] =
    "Enables URL filter interstitial V3 for Family Link users.";

const char kSupervisedUserEmitLogRecordSeparatelyName[] =
    "Emit supervised user log record separately";
const char kSupervisedUserEmitLogRecordSeparatelyDescription[] =
    "Emit supervised user log record separately for Family Link and device "
    "parental controls users (no user-visible effect).";

const char kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsName[] =
    "Merge device parental controls and Family Link prefs";
const char
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsDescription[] =
        "Merges non-web filtering device parental controls settings with "
        "Family Link settings in the SupervisedUserPrefStore (no user-visible "
        "effect).";

const char kSupervisedUserUseUrlFilteringServiceName[] =
    "Use URL filtering service";
const char kSupervisedUserUseUrlFilteringServiceDescription[] =
    "Use the SupervisedUserUrlFilteringService to get URL filtering settings "
    "directly from supervision services instead of using PrefService (no "
    "user-visible effect).";

const char kSyncAIThreadsName[] = "Sync AI Threads";
const char kSyncAIThreadsDescription[] =
    "Enables syncing of AI threads across devices.";

const char kSyncAccountSettingsName[] = "Sync account settings";
const char kSyncAccountSettingsDescription[] =
    "Enables syncing account settings to the server.";

const char kSyncAutofillValuableMetadataName[] =
    "Sync autofill valuable metadata";
const char kSyncAutofillValuableMetadataDescription[] =
    "Enables syncing valuable metadata for autofill to the server.";

const char kSyncAutofillValuableName[] = "Sync autofill valuable";
const char kSyncAutofillValuableDescription[] =
    "Enables syncing valuable for autofill to the server.";

const char kSyncContextualTaskName[] = "Sync Contextual Task";
const char kSyncContextualTaskDescription[] =
    "Enables syncing of contextual tasks.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncThemesIosName[] = "Enable Sync Themes on iOS";
const char kSyncThemesIosDescription[] =
    "Enables syncing of themes across iOS devices.";

const char kSyncTrustedVaultInfobarMessageImprovementsName[] =
    "Trusted vault infobar message improvements";
const char kSyncTrustedVaultInfobarMessageImprovementsDescription[] =
    "Enables massage improvements for the UI of the trusted vault error "
    "infobar.";

const char kSyncWalletFlightReservationsName[] =
    "Sync wallet flight reservations";
const char kSyncWalletFlightReservationsDescription[] =
    "Enables syncing flight reservations in the wallet to the server.";

const char kSyncWalletVehicleRegistrationsName[] =
    "Sync wallet vehicle registrations";
const char kSyncWalletVehicleRegistrationsDescription[] =
    "Enables syncing vehicle registrations in the wallet to the server.";

const char kSyncedGroupColorName[] = "SyncedGroupColor";
const char kSyncedGroupColorDescription[] =
    "Enables the SyncedGroupColor feature.";

const char kTabGridNewTransitionsName[] = "Enable new TabGrid transitions";
const char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

const char kTabGroupColorOnSurfaceName[] = "Tab group color on surfaces";
const char kTabGroupColorOnSurfaceDescription[] =
    "Adds the tab group color to the tab group and tab grid surfaces.";

const char kTabGroupInOverflowMenuName[] =
    "Enable the Tab Group button in the overflow menu";
const char kTabGroupInOverflowMenuDescription[] =
    "When enabled, a Tab Group button will appear in the overflow menu.";

const char kTabGroupInTabIconContextMenuName[] =
    "Enable the Tab Group button in the tab grid icon context menu";
const char kTabGroupInTabIconContextMenuDescription[] =
    "When enabled, a Tab Group button will appear in the tab grid icon context "
    "menu.";

const char kTabGroupIndicatorName[] = "Tab Group Indicator";
const char kTabGroupIndicatorDescription[] =
    "When enabled, displays a tab group indicator next to the omnibox.";

const char kTabGroupSyncName[] = "Enable Tab Group Sync";
const char kTabGroupSyncDescription[] =
    "When enabled, tab groups are synced between syncing devices. Requires "
    "#tab-groups-on-ipad to also be enabled on iPad.";

const char kTabResumptionImagesName[] = "Enable Tab Resumption images";
const char kTabResumptionImagesDescription[] =
    "When enabled, a relevant image is displayed in Tab resumption items.";

const char kTabResumptionName[] = "Enable Tab Resumption";
const char kTabResumptionDescription[] =
    "When enabled, offer users with a quick shortcut to resume the last synced "
    "tab from another device.";

const char kTabSwitcherOverflowMenuName[] =
    "Enable the Tab Switcher overflow menu";
const char kTabSwitcherOverflowMenuDescription[] =
    "When enabled, the Tab Switcher edit button and edit menu will be replaced "
    "by a three dot button and overflow menu.";

const char kTaiyakiAllSurfacesName[] = "Taiyaki (all surfaces)";
const char kTaiyakiAllSurfacesDescription[] =
    "Enables Taiyaki for all surfaces (including post-FRE).";

const char kUpdatedFRESequenceName[] =
    "Update the sequence of the First Run screens";
const char kUpdatedFRESequenceDescription[] =
    "Updates the sequence of the FRE screens to show the DB promo first, "
    "remove the Sin-In & Sync screens, or both.";

const char kUseDefaultAppsDestinationForPromosName[] =
    "Use Default Apps page for promos";
const char kUseDefaultAppsDestinationForPromosDescription[] =
    "When enabled, all Default Browser promos redirecting to the iOS settings "
    "will use the new Default Apps page, if the current device supports it.";

const char kUseFeedEligibilityServiceName[] =
    "[iOS] Use the new feed eligibility service";
const char kUseFeedEligibilityServiceDescription[] =
    "Use the new eligibility service to handle whether the Discover "
    "feed is displayed on NTP";

const char kUseSceneViewControllerName[] = "Use Scene View Controller";
const char kUseSceneViewControllerDescription[] =
    "Enables the use of SceneViewController.";

const char kVariationsExperimentalCorpusName[] =
    "Variations experimental corpus";
const char kVariationsExperimentalCorpusDescription[] =
    "When enabled, request the experimental variations seed from the "
    "variations server.";

const char kVariationsRestrictDogfoodName[] = "Variations restrict dogfood";
const char kVariationsRestrictDogfoodDescription[] =
    "When enabled, request dogfood variations from the variations server.";

const char kViewCertificateInformationName[] = "View Certificate Information";
const char kViewCertificateInformationDescription[] =
    "Enables viewing detailed certificate information in Page Info.";

const char kWaitThresholdMillisecondsForCapabilitiesApiName[] =
    "Maximum wait time (in seconds) for a response from the Account "
    "Capabilities API";
const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[] =
    "Used for testing purposes to test waiting thresholds in dev.";

const char kWalletApiPrivatePassesEnabledName[] = "Wallet API Private Passes";
const char kWalletApiPrivatePassesEnabledDescription[] =
    "Enables the Wallet API for private passes.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWelcomeBackName[] = "Enable Welcome Back screen";
const char kWelcomeBackDescription[] =
    "When enabled, returning users will see the Welcome Back screen.";

const char kYourSavedInfoSettingsPageIosName[] =
    "Enable Autofill and passwords settings redesign on iOS";
const char kYourSavedInfoSettingsPageIosDescription[] =
    "Enables the Autofill and passwords settings page redesign on iOS.";

const char kZeroStateSuggestionsName[] = "Enable Zero-State Suggestions";
const char kZeroStateSuggestionsDescription[] =
    "Enables fetching zero-state suggestions for the 'Ask Gemini' feature,"
    "based on the current page context.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

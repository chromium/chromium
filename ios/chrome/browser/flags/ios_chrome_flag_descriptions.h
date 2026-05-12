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

inline constexpr char kAIHubNewBadgeName[] = "AI Hub New Badge";
inline constexpr char kAIHubNewBadgeDescription[] =
    "Enables showing a new badge on the AI Hub button in the toolbar.";

inline constexpr char kAIMCobrowseDebugEntrypointName[] =
    "AIM Cobrowse debug entrypoint";
inline constexpr char kAIMCobrowseDebugEntrypointDescription[] =
    "Enables the AIM Cobrowse debug entrypoint feature.";

inline constexpr char kAIMEligibilityRefreshNTPModulesName[] =
    "AIMEligibilityRefreshNTPModules";
inline constexpr char kAIMEligibilityRefreshNTPModulesDescription[] =
    "Enables the AIMEligibilityRefreshNTPModules feature.";

inline constexpr char kAIMEligibilityServiceStartWithProfileName[] =
    "AIMEligibilityServiceStartWithProfile";
inline constexpr char kAIMEligibilityServiceStartWithProfileDescription[] =
    "Start the AIM eligibility service with the profile.";

inline constexpr char kAIMNTPEntrypointTabletName[] = "AIMNTPEntrypointTablet";
inline constexpr char kAIMNTPEntrypointTabletDescription[] =
    "Enables the AIMNTPEntrypointTablet feature.";

inline constexpr char kAIOmniboxAskPlaceholderName[] =
    "AI Omnibox Ask Placeholder";
inline constexpr char kAIOmniboxAskPlaceholderDescription[] =
    "Enables the placeholder text to be 'Ask...' instead of 'Search...' when "
    "AI Omnibox is available.";

inline constexpr char kAimCobrowseHeaderName[] = "AimCobrowseHeader";
inline constexpr char kAimCobrowseHeaderDescription[] =
    "Changes the design of the AIM cobrowse header.";

inline constexpr char kAimCobrowseName[] = "AimCobrowse";
inline constexpr char kAimCobrowseDescription[] =
    "Enables the AimCobrowse feature.";

inline constexpr char kAimUrlNavigationFetchEnabledName[] =
    "AimUrlNavigationFetchEnabled";
inline constexpr char kAimUrlNavigationFetchEnabledDescription[] =
    "Enables the AimUrlNavigationFetchEnabled feature.";

inline constexpr char kAnimatedDefaultBrowserPromoInFREName[] =
    "Enable the animated Default Browser Promo in the FRE";
inline constexpr char kAnimatedDefaultBrowserPromoInFREDescription[] =
    "When enabled, the Default Browser Promo in the FRE will be animated.";

inline constexpr char kAppBackgroundRefreshName[] =
    "Enable app background refresh";
inline constexpr char kAppBackgroundRefreshDescription[] =
    "Schedules app background refresh after some minimum period of time has "
    "passed after the last refresh.";

inline constexpr char kAppStoreInAppEventsName[] = "App Store In-App Events";
inline constexpr char kAppStoreInAppEventsDescription[] =
    "Enables a user to tap the promo within the iOS App Store and invoke the "
    "Gemini FRE after navigating to a Gemini related web page through an "
    "external action.";

inline constexpr char kAppleCalendarExperienceKitName[] =
    "Experience Kit Apple Calendar";
inline constexpr char kAppleCalendarExperienceKitDescription[] =
    "When enabled, long pressing on dates will trigger Experience Kit Apple "
    "Calendar event handling.";

inline constexpr char kApplyClientsideModelPredictionsForPasswordTypesName[] =
    "Apply clientside model predictions for password forms.";
inline constexpr char
    kApplyClientsideModelPredictionsForPasswordTypesDescription[] =
        "Enable using clientside model predictions to fill password forms.";

inline constexpr char kAskAboutThisPageName[] = "AskAboutThisPage";
inline constexpr char kAskAboutThisPageDescription[] =
    "Enables the AskAboutThisPage feature.";

inline constexpr char kAskGeminiChipName[] = "Ask Gemini Chip";
inline constexpr char kAskGeminiChipDescription[] =
    "Enables the Ask Gemini Chip feature.";

inline constexpr char kAssistantAimMinimizedStateName[] =
    "AssistantAimMinimizedState";
inline constexpr char kAssistantAimMinimizedStateDescription[] =
    "When enabled, the Assistant AIM (Co-browse) interface initially appears "
    "in a minimized state instead of the default medium state.";

inline constexpr char kAssistantContainerName[] = "Assistant Container";
inline constexpr char kAssistantContainerDescription[] =
    "Enables the Assistant Container feature. The debug parameter enables "
    "debug elements and forces AIM eligibility.";

inline constexpr char kAssistantSidePanelName[] = "AssistantSidePanel";
inline constexpr char kAssistantSidePanelDescription[] =
    "Enables the AssistantSidePanel feature.";

inline constexpr char kAutofillAcrossIframesName[] =
    "Enables Autofill across iframes";
inline constexpr char kAutofillAcrossIframesDescription[] =
    "When enabled, Autofill will fill and save information on forms that "
    "spread across multiple iframes.";

inline constexpr char kAutofillAiAvailableByDefaultName[] =
    "Autofill AI available by default";
inline constexpr char kAutofillAiAvailableByDefaultDescription[] =
    "Makes Autofill AI available by default.";

inline constexpr char kAutofillAiCreateEntityDataManagerName[] =
    "Autofill AI Create Entity Data Manager";
inline constexpr char kAutofillAiCreateEntityDataManagerDescription[] =
    "Enables Autofill AI Create Entity Data Manager.";

inline constexpr char kAutofillAiDedupeEntitiesName[] =
    "Autofill AI dedupe entities";
inline constexpr char kAutofillAiDedupeEntitiesDescription[] =
    "Enables periodic deduplication of Autofill AI entities.";

inline constexpr char kAutofillAiNoFillingIconsExperimentName[] =
    "Autofill AI no filling icons experiment";
inline constexpr char kAutofillAiNoFillingIconsExperimentDescription[] =
    "If enabled, Autofill AI filling suggestions do not have an icon.";

inline constexpr char kAutofillAiReauthRequiredName[] =
    "Autofill AI Reauth Required";
inline constexpr char kAutofillAiReauthRequiredDescription[] =
    "Enables Autofill AI Reauth Required.";

inline constexpr char kAutofillAiValuablesIPHName[] =
    "IPH Autofill AI Valuables";
inline constexpr char kAutofillAiValuablesIPHDescription[] =
    "Enables the In-Product Help for Autofill AI valuables.";

inline constexpr char kAutofillAiWalletFlightReservationName[] =
    "Autofill AI Google Wallet flight reservations";
inline constexpr char kAutofillAiWalletFlightReservationDescription[] =
    "Enables Autofill AI support for flight reservation entities from Google "
    "Wallet.";

inline constexpr char kAutofillAiWalletPrivatePassesDeepLinkName[] =
    "Autofill AI Google Wallet private passes deep link";
inline constexpr char kAutofillAiWalletPrivatePassesDeepLinkDescription[] =
    "Enables Autofill AI support for deep linking to private passes from "
    "Google Wallet.";

inline constexpr char kAutofillAiWalletPrivatePassesName[] =
    "Autofill AI Google Wallet private passes";
inline constexpr char kAutofillAiWalletPrivatePassesDescription[] =
    "Enables Autofill AI support for private passes from Google Wallet.";

inline constexpr char kAutofillAiWalletVehicleRegistrationName[] =
    "Autofill AI Google Wallet vehicle registration";
inline constexpr char kAutofillAiWalletVehicleRegistrationDescription[] =
    "Enables Autofill AI support for vehicle registration entities from Google "
    "Wallet.";

inline constexpr char kAutofillAiWithDataSchemaName[] =
    "Autofill AI With Data Schema";
inline constexpr char kAutofillAiWithDataSchemaDescription[] =
    "Enables Autofill AI With Data Schema.";

inline constexpr char kAutofillBottomSheetNewBlurName[] =
    "New Blur Method for Autofill Bottom Sheet";
inline constexpr char kAutofillBottomSheetNewBlurDescription[] =
    "Enables a new method for blurring the autofill bottom sheet to prevent "
    "the keyboard from showing up. This uses `mousedown` instead of `focus`.";

inline constexpr char kAutofillCreditCardScannerIosName[] =
    "Enable the credit card scanner for Autofill";
inline constexpr char kAutofillCreditCardScannerIosDescription[] =
    "When enabled, users are offered the ability to use their phone camera to "
    "scan their credit card when adding it to Chrome Autofill";

inline constexpr char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
inline constexpr char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

inline constexpr char kAutofillDisableProfileUpdatesName[] =
    "Disables Autofill profile updates from form submissions";
inline constexpr char kAutofillDisableProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply updates to address profiles based "
    "on data extracted from submitted forms. For testing purposes.";

inline constexpr char kAutofillDisableSilentProfileUpdatesName[] =
    "Disables Autofill silent profile updates from form submissions";
inline constexpr char kAutofillDisableSilentProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply silent updates to address profiles. "
    "For testing purposes.";

inline constexpr char kAutofillEnableBottomSheetScanCardAndFillName[] =
    "Enable scan card BottomSheet, then save and fill of the credit card";
inline constexpr char kAutofillEnableBottomSheetScanCardAndFillDescription[] =
    "When enabled, offers a card scanning BottomSheet and allows users to "
    "save and autofill credit cards in autofill forms.";

inline constexpr char kAutofillEnableCardInfoRuntimeRetrievalName[] =
    "Enable retrieval of card info(with CVC) from issuer for enrolled cards";
inline constexpr char kAutofillEnableCardInfoRuntimeRetrievalDescription[] =
    "When enabled, runtime retrieval of CVC along with card number and expiry "
    "from issuer for enrolled cards will be enabled during form fill.";

inline constexpr char kAutofillEnablePrefetchingRiskDataForRetrievalName[] =
    "Enable prefetching of risk data during payments autofill retrieval";
inline constexpr char
    kAutofillEnablePrefetchingRiskDataForRetrievalDescription[] =
        "When enabled, risk data is prefetched during payments autofill flows "
        "to reduce user-perceived latency.";

inline constexpr char kAutofillEnableSupportForHomeAndWorkName[] =
    "Enable support for home and work addresses";
inline constexpr char kAutofillEnableSupportForHomeAndWorkDescription[] =
    "When enabled, chrome will support home and work addresses from account.";

inline constexpr char kAutofillEnableSupportForNameAndEmailName[] =
    "Support for name and email addresses in Autofill";
inline constexpr char kAutofillEnableSupportForNameAndEmailDescription[] =
    "When enabled, a name and email profile with data comming from the account "
    "will be created for autofilling.";

inline constexpr char kAutofillEnableWalletBrandingName[] =
    "Update Google Pay branding to Wallet where applicable";
inline constexpr char kAutofillEnableWalletBrandingDescription[] =
    "When enabled, certain strings and logos referencing Google Account, "
    "Google Payments, and Google Pay will instead reference Google Wallet.";

inline constexpr char kAutofillEnableWalletBrandingV2Name[] =
    "Further update Google Pay and Google Wallet branding where applicable";
inline constexpr char kAutofillEnableWalletBrandingV2Description[] =
    "When enabled, further brings certain strings and images referencing "
    "Google Pay and Google Wallet into consistency with branding requirements.";

inline constexpr char kAutofillManualTestingDataName[] =
    "Autofill manual testing data";
inline constexpr char kAutofillManualTestingDataDescription[] =
    "When set, imports the addresses and cards specified on startup. WARNING: "
    "If at least one address/card is specified, all other existing "
    "addresses/cards are overwritten.";

inline constexpr char kAutofillPaymentsFieldSwappingName[] =
    "Swap credit card suggestions";
inline constexpr char kAutofillPaymentsFieldSwappingDescription[] =
    "When enabled, swapping autofilled payment suggestions would result"
    "in overriding all of the payments fields with the swapped profile data";

inline constexpr char kAutofillPaymentsSheetV2Name[] =
    "Enable the payments suggestion bottom sheet V2";
inline constexpr char kAutofillPaymentsSheetV2Description[] =
    "When enabled, the V2 of the payments suggestion bottom sheet will be "
    "used.";

inline constexpr char kAutofillPruneSuggestionsName[] =
    "Autofill Prune Suggestions";
inline constexpr char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

inline constexpr char kAutofillSupportDateInputName[] =
    "Autofill support for date input";
inline constexpr char kAutofillSupportDateInputDescription[] =
    "Enables form filling and saving capabilities for <input type=\"date\">.";

inline constexpr char kAutofillThrottleDocumentFormScanName[] =
    "Throttle Autofill Document Form Scans";
inline constexpr char kAutofillThrottleDocumentFormScanDescription[] =
    "Enables the throttling of the recurrent document form scans done by "
    "Autofill.";

inline constexpr char kAutofillThrottleFilteredDocumentFormScanName[] =
    "Throttle Filtered Autofill Document Form Scans";
inline constexpr char kAutofillThrottleFilteredDocumentFormScanDescription[] =
    "Enables the throttling of the on the spot filtered form scans done by "
    "Autofill (e.g. get the latest state of a form that had an activity).";

inline constexpr char kAutofillUpstreamEnforceStrikeDelayName[] =
    "Require a week between offers to save credit cards";
inline constexpr char kAutofillUpstreamEnforceStrikeDelayDescription[] =
    "When enabled, users should not see offers to save the same credit card "
    "twice in a week, as the strike database enforces a 7-day delay between "
    "strikes.";

inline constexpr char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
inline constexpr char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

inline constexpr char kAutofillVcnEnrollStrikeExpiryTimeName[] =
    "Expiry duration for VCN enrollment strikes";
inline constexpr char kAutofillVcnEnrollStrikeExpiryTimeDescription[] =
    "When enabled, changes the amount of time required for VCN enrollment "
    "prompt strikes to expire.";

inline constexpr char kBWGPromoConsentName[] = "BWG Promo Consent";
inline constexpr char kBWGPromoConsentDescription[] =
    "Whether the promo consent flow is composed of a single or a double screen "
    "view.";

inline constexpr char kBackgroundRefreshRegressionTestName[] =
    "Background Refresh Regression Test";
inline constexpr char kBackgroundRefreshRegressionTestDescription[] =
    "Enables the Background Refresh Regression Test with multiple arms "
    "to test various refresh and persistence parameters.";

inline constexpr char kBestFeaturesScreenInFirstRunName[] =
    "Display Best Features screen in the FRE";
inline constexpr char kBestFeaturesScreenInFirstRunDescription[] =
    "When enabled, displays the BestFeatures screen in the First Run sequence. "
    "Screen can be displayed either before or after the DB promo.";

inline constexpr char kBestOfAppFREName[] =
    "Display Best of App view in the FRE";
inline constexpr char kBestOfAppFREDescription[] =
    "When enabled, displays some views during the FRE highlighting the best "
    "features in the app.";

inline constexpr char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
inline constexpr char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

inline constexpr char kBuildExternalPrivacyContextName[] =
    "Build external privacy context";
inline constexpr char kBuildExternalPrivacyContextDescription[] =
    "When enabled, checks if the account can be signed in on the device "
    "according to the capabilities. This needs `can_sign_in_to_chrome` "
    "capability to be fetched (controlled by "
    "kEnforceCanSignInToChromeCapability flag).";

inline constexpr char kCacheIdentityListInChromeName[] =
    "Cache identity list in chrome.";
inline constexpr char kCacheIdentityListInChromeDescription[] =
    "Changes the implementation of the cache of the list of identities on "
    "device.";

inline constexpr char kChromeNextIaName[] = "ChromeNextIa";
inline constexpr char kChromeNextIaDescription[] =
    "Enables the chrome_next_ia feature.";

inline constexpr char kCobrowseAimHistoryName[] = "CobrowseAimHistory";
inline constexpr char kCobrowseAimHistoryDescription[] =
    "When enabled, the history button in cobrowse is shown and can display the "
    "list of all previous AIM conversations.";

inline constexpr char kCollaborationMessagingName[] = "Collaboration Messaging";
inline constexpr char kCollaborationMessagingDescription[] =
    "Enables the messaging framework within the collaboration feature, "
    "including features such as recent activity, dirty dots, and description "
    "action chips.";

inline constexpr char kComposeboxAIMDisabledName[] = "ComposeboxAIMDisabled";
inline constexpr char kComposeboxAIMDisabledDescription[] =
    "When enabled, AIM feature are disabled in the composebox.";

inline constexpr char kComposeboxAIMNudgeName[] = "ComposeboxAIMNudge";
inline constexpr char kComposeboxAIMNudgeDescription[] =
    "Enables the AIM nudge button in the composebox, tapping on the button "
    "enables AIM. This is conditionned by AIM availability.";

inline constexpr char kComposeboxAdditionalAdvancedToolsName[] =
    "Enable additional advanced tools in composebox";
inline constexpr char kComposeboxAdditionalAdvancedToolsDescription[] =
    "When enabled, the additional tools in the input plate are shown, such as "
    "canvas and the model picker";

inline constexpr char kComposeboxAttachmentsTypedStateName[] =
    "Enable contextual suggestions for typed state";
inline constexpr char kComposeboxAttachmentsTypedStateDescription[] =
    "Enables showing suggestions for multiple composebox attachments in a "
    "typed state.";

inline constexpr char kComposeboxCloseButtonTopAlignName[] =
    "Align the close button in composebox to the top edge of the view";
inline constexpr char kComposeboxCloseButtonTopAlignDescription[] =
    "If the user preference is set to top, enabling this feature aligns the "
    "compose box close button with the top edge of the input plate instead of "
    "centering.";

inline constexpr char kComposeboxCompactModeName[] = "ComposeboxCompactMode";
inline constexpr char kComposeboxCompactModeDescription[] =
    "Enables the compact composebox, adding attachment or enabling AIM will "
    "expand it to the regular size.";

inline constexpr char kComposeboxConditionalPlusButtonName[] =
    "Composebox Conditional Plus Button";
inline constexpr char kComposeboxConditionalPlusButtonDescription[] =
    "When enabled, hides the plus button when typing a URL in compact mode.";

inline constexpr char kComposeboxDeepSearchName[] =
    "Enable Composebox Deep Search";
inline constexpr char kComposeboxDeepSearchDescription[] =
    "Enables the deep search advanced tool in Composebox";

inline constexpr char kComposeboxDevToolsName[] = "Enable Composebox Dev Tools";
inline constexpr char kComposeboxDevToolsDescription[] =
    "Enables development tools for the composebox, allowing simulation of "
    "delays and failures.";

inline constexpr char
    kComposeboxFetchContextualSuggestionsForMultipleAttachmentsName[] =
        "Enable Composebox Fetch Contextual Suggestions For multiple "
        "attachments";
inline constexpr char
    kComposeboxFetchContextualSuggestionsForMultipleAttachmentsDescription[] =
        "Enables showing suggestions for multiple attachments";

inline constexpr char kComposeboxForceTopName[] = "ComposeboxForceTop";
inline constexpr char kComposeboxForceTopDescription[] =
    "Forces the composebox to be at the top.";

inline constexpr char kComposeboxIOSName[] = "ComposeboxIOS";
inline constexpr char kComposeboxIOSDescription[] =
    "Enables the composebox that replaces the regular omnibox in edit state.";

inline constexpr char kComposeboxIpadName[] = "ComposeboxIpad";
inline constexpr char kComposeboxIpadDescription[] =
    "Enables the composeboxIpad feature.";

inline constexpr char kComposeboxPlusButtonBottomSheetName[] =
    "Enable the bottom sheet for plus button in Composebox";
inline constexpr char kComposeboxPlusButtonBottomSheetDescription[] =
    "Uses the updated bottom sheet for the plus button multimodal menu.";

inline constexpr char kComposeboxServerSideStateName[] =
    "Enable server side state in Composebox";
inline constexpr char kComposeboxServerSideStateDescription[] =
    "When enabled, the server side state will be used in the composebox";

inline constexpr char kConfirmationButtonSwapOrderName[] =
    "Swap Button Order in confirmation alerts";
inline constexpr char kConfirmationButtonSwapOrderDescription[] =
    "Swaps the positions of the primary and secondary buttons in the "
    "confirmation alerts, so that the primary button is placed at the bottom.";

inline constexpr char kConsistentLogoDoodleHeightName[] =
    "Consistent NTP Logo and Doodle Height";
inline constexpr char kConsistentLogoDoodleHeightDescription[] =
    "Ensures the NTP Logo and Doodle have a consistent height to prevent "
    "content jumping.";

inline constexpr char kContentNotificationProvisionalIgnoreConditionsName[] =
    "Content Notification Provisional Ignore Conditions";
inline constexpr char
    kContentNotificationProvisionalIgnoreConditionsDescription[] =
        "Enable Content Notification Provisional without Conditions";

inline constexpr char kContentPushNotificationsName[] =
    "Content Push Notifications";
inline constexpr char kContentPushNotificationsDescription[] =
    "Enables the content push notifications.";

inline constexpr char kCredentialProviderExtensionPromoName[] =
    "Enable the Credential Provider Extension promo.";
inline constexpr char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, Credential Provider Extension promo will be "
    "presented to eligible users.";

inline constexpr char kCredentialProviderPasskeyLargeBlobName[] =
    "Credential Provider Large Blob support";
inline constexpr char kCredentialProviderPasskeyLargeBlobDescription[] =
    "Enables support for the Large Blob extension for Passkeys in the "
    "Credential Provider Extension.";

inline constexpr char kCredentialProviderPerformanceImprovementsName[] =
    "Credential Provider Performance Improvements";
inline constexpr char kCredentialProviderPerformanceImprovementsDescription[] =
    "Enables a series of performance improvements for the Credential Provider "
    "Extension.";

inline constexpr char kCrossDeviceSigninName[] = "Cross-Device Sign-in";
inline constexpr char kCrossDeviceSigninDescription[] =
    "Guards the logic to start sign-in from a given QR Code.";

inline constexpr char kDataControlsSearchWithName[] =
    "Data Controls enforcement for search context menu item";
inline constexpr char kDataControlsSearchWithDescription[] =
    "Enables the Enterprise Data Controls for restricting data exfiltration "
    "with the \"Search with...\" context menu item.";

inline constexpr char kDataSharingDebugLogsName[] =
    "Enable data sharing debug logs";
inline constexpr char kDataSharingDebugLogsDescription[] =
    "Enables the data sharing infrastructure to log and save debug messages "
    "that can be shown in the internals page.";

inline constexpr char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
inline constexpr char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

inline constexpr char kDataSharingName[] = "Data Sharing";
inline constexpr char kDataSharingDescription[] =
    "Enabled Data Sharing related UI and features.";

inline constexpr char kDataSharingSharedDataTypesEnabled[] =
    "Version out-of-date, no UI";
inline constexpr char kDataSharingSharedDataTypesEnabledWithUi[] =
    "Version out-of-date, show UI ";

inline constexpr char kDataSharingVersioningStatesName[] =
    "Data Sharing Versioning Test Scenarios";
inline constexpr char kDataSharingVersioningStatesDescription[] =
    "Testing multiple scenarios for versioning.";

inline constexpr char kDefaultBrowserOffCyclePromoName[] =
    "Default Browser off-cycle promo";
inline constexpr char kDefaultBrowserOffCyclePromoDescription[] =
    "When enabled, an off-cycle default browser promo will be shown.";

inline constexpr char kDefaultBrowserPictureInPictureName[] =
    "Default Browser Promo Picture in Picture";
inline constexpr char kDefaultBrowserPictureInPictureDescription[] =
    "When enabled, default browser instructions will be displayed in "
    "picture-in-picture format over the iOS settings.";

inline constexpr char kDefaultBrowserPromoIpadInstructionsName[] =
    "Default Browser Promo iPad Instructions";
inline constexpr char kDefaultBrowserPromoIpadInstructionsDescription[] =
    "When enabled, displays default browser promo instructions specifically "
    "adapted for iPad.";

inline constexpr char kDefaultBrowserPromoPropensityModelName[] =
    "Default Browser promo propensity model";
inline constexpr char kDefaultBrowserPromoPropensityModelDescription[] =
    "When enabled, a propensity model will help make the determination of "
    "whether to show a default browser promo";

inline constexpr char kDetectMainThreadFreezeName[] =
    "Detect freeze in the main thread.";
inline constexpr char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

inline constexpr char kDisableAutofillStrikeSystemName[] =
    "Disable the Autofill strike system";
inline constexpr char kDisableAutofillStrikeSystemDescription[] =
    "When enabled, the Autofill strike system will not block a feature from "
    "being offered.";

inline constexpr char kDisableComposeboxFromAIMNTPName[] =
    "DisableComposeboxFromAIMNTP";
inline constexpr char kDisableComposeboxFromAIMNTPDescription[] =
    "When enabled, the NTP entrypoint will always lead to the AIM webpage even "
    "when composebox is enabled.";

inline constexpr char kDisableKeyboardAccessoryName[] =
    "Disable Omnibox Keyboard Accessory";
inline constexpr char kDisableKeyboardAccessoryDescription[] =
    "Disables parts or all of omnibox keyboard accessory.";

inline constexpr char kDisableLensCameraName[] =
    "Disable Lens camera experience";
inline constexpr char kDisableLensCameraDescription[] =
    "When enabled, the option use Lens to search for images from your device "
    "camera menu when Google is the selected search engine, accessible from "
    "the home screen widget, new tab page, and keyboard, is disabled.";

inline constexpr char kDisableShareButtonName[] =
    "Disable Share Button in Toolbar";
inline constexpr char kDisableShareButtonDescription[] =
    "Hides the share button in toolbar.";

inline constexpr char kDisableU18FeedbackIosName[] = "DisableU18FeedbackIos";
inline constexpr char kDisableU18FeedbackIosDescription[] =
    "When enabled, the primary identity is set to the feedback UI when opened. "
    "The user is free add it to the feedback or not. Also the feedback cannot "
    "be sent if the primary user is under 18. When disabled, the feedback is "
    "anoymous";

inline constexpr char kDownloadAutoDeletionClearFilesOnEveryStartupName[] =
    "Enable Download Auto-Deletion Testing Mode";
inline constexpr char
    kDownloadAutoDeletionClearFilesOnEveryStartupDescription[] =
        "When enabled, the Auto-deletion feature wil clear all downloaded "
        "files "
        "scheduled for deletion on every application startup, regardless of "
        "when "
        "the file was downloaded. This feature is intended for testing-only.";

inline constexpr char kDownloadAutoDeletionName[] =
    "Enable Download Auto Deletion";
inline constexpr char kDownloadAutoDeletionDescription[] =
    "When enabled, files downloaded on the device can be scheduled to be "
    "deleted automatically after 30 days.";

inline constexpr char kDownloadListName[] = "Enable Download List";
inline constexpr char kDownloadListDescription[] =
    "Controls the UI type for the download list. When enabled, allows "
    "switching between default and custom UI implementations.";

inline constexpr char kDownloadServiceForegroundSessionName[] =
    "Download service foreground download";
inline constexpr char kDownloadServiceForegroundSessionDescription[] =
    "Enable download service to download in app foreground only";

inline constexpr char kEditPasswordsInSettingsName[] =
    "Edit passwords in settings";
inline constexpr char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

inline constexpr char kEnableACPrefetchName[] = "Enable AC Prefetch";
inline constexpr char kEnableACPrefetchDescription[] =
    "Ensures that account capabilities are prefetched and cached.";

inline constexpr char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
inline constexpr char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

inline constexpr char kEnableClientCertificateProvisioningOnIOSName[] =
    "Enable client certificate provisioning on iOS";
inline constexpr char kEnableClientCertificateProvisioningOnIOSDescription[] =
    "When enabled, client certificate provisioning from the cloud is allowed "
    "for enterprise users on iOS.";

inline constexpr char kEnableCompromisedPasswordsMutingName[] =
    "Enable the muting of compromised passwords in the Password Manager";
inline constexpr char kEnableCompromisedPasswordsMutingDescription[] =
    "Enable the compromised password alert mutings in Password Manager to be "
    "respected in the app.";

inline constexpr char kEnableFamilyLinkControlsName[] =
    "Family Link parental controls";
inline constexpr char kEnableFamilyLinkControlsDescription[] =
    "Enables parental controls from Family Link on supervised accounts "
    "signed-in to Chrome.";

inline constexpr char kEnableFeedAblationName[] = "Enables Feed Ablation";
inline constexpr char kEnableFeedAblationDescription[] =
    "If Enabled the Feed will be removed from the NTP";

inline constexpr char kEnableFeedCardMenuSignInPromoName[] =
    "Enable Feed card menu sign-in promotion";
inline constexpr char kEnableFeedCardMenuSignInPromoDescription[] =
    "Display a sign-in promotion UI when signed out users click on "
    "personalization options within the feed card menu.";

inline constexpr char kEnableFeedHeaderSettingsName[] =
    "Enables the feed header settings.";
inline constexpr char kEnableFeedHeaderSettingsDescription[] =
    "When enabled, some UI elements of the feed header can be modified.";

inline constexpr char kEnableFileDownloadConnectorIOSName[] =
    "Enable file download connectors on iOS.";
inline constexpr char kEnableFileDownloadConnectorIOSDescription[] =
    "When enabled, the enterprise DLP file download featured is available on "
    "iOS. ";

inline constexpr char kEnableFuseboxKeyboardAccessoryName[] =
    "Enable Omnibox Keyboard Accessory in Fusebox";
inline constexpr char kEnableFuseboxKeyboardAccessoryDescription[] =
    "Enables parts or all of omnibox keyboard accessory.";

inline constexpr char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
inline constexpr char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

inline constexpr char kEnableNTPBackgroundImageCacheName[] =
    "Enable NTP Background Image Cache";
inline constexpr char kEnableNTPBackgroundImageCacheDescription[] =
    "Enables the NTP background image cache service to improve performance.";

inline constexpr char kEnableNewStartupFlowName[] = "EnableNewStartupFlow";
inline constexpr char kEnableNewStartupFlowDescription[] =
    "Enables the EnableNewStartupFlow feature.";

inline constexpr char kEnableReadingListAccountStorageName[] =
    "Enable Reading List Account Storage";
inline constexpr char kEnableReadingListAccountStorageDescription[] =
    "Enable the reading list account storage.";

inline constexpr char kEnableReadingListSignInPromoName[] =
    "Enable Reading List Sign-in promo";
inline constexpr char kEnableReadingListSignInPromoDescription[] =
    "Enable the sign-in promo view in the reading list screen.";

inline constexpr char kEnableScreenshotProtectionIOSName[] =
    "Enable Screenshot Protection on iOS";
inline constexpr char kEnableScreenshotProtectionIOSDescription[] =
    "Prevents the content of the app from appearing in screenshots and screen "
    "recordings.";

inline constexpr char kEnableTraitCollectionRegistrationName[] =
    "Enable Customizable Trait Registration";
inline constexpr char kEnableTraitCollectionRegistrationDescription[] =
    "When enabled, UI elements will only observe and respond to the UITraits "
    "to which they have been registered.";

inline constexpr char kEnforceCanSignInToChromeCapabilityName[] =
    "Fetch can_sign_in_to_chrome capability";
inline constexpr char kEnforceCanSignInToChromeCapabilityDescription[] =
    "When enabled, can_sign_in_to_chrome is fetched.";

inline constexpr char kEnhancedCalendarName[] =
    "Enable Enhanced Calendar integration";
inline constexpr char kEnhancedCalendarDescription[] =
    "When enabled, the enhanced calendar flow will be available to eligible "
    "users when adding a calendar event.";

inline constexpr char kExplainGeminiEditMenuName[] =
    "Enable Explain Gemini Edit Menu";
inline constexpr char kExplainGeminiEditMenuDescription[] =
    "When enabled, the explain Gemini edit menu will be available to eligible "
    "users when highlighting any text on a web page.";

inline constexpr char kFRESignInHeaderTextUpdateName[] =
    "Enable header text variations on the FRE sign-in page.";
inline constexpr char kFRESignInHeaderTextUpdateDescription[] =
    "When enabled, the FRE sign-in page displays a different header text.";

inline constexpr char kFeedBackgroundRefreshName[] =
    "Enable feed background refresh";
inline constexpr char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

inline constexpr char kFeedSwipeInProductHelpName[] = "Enable Feed Swipe IPH";
inline constexpr char kFeedSwipeInProductHelpDescription[] =
    "Presents an in-product help on the NTP to promote swiping on the Feed";

inline constexpr char kForceStartupSigninPromoName[] =
    "Display the startup sign-in promo";
inline constexpr char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

inline constexpr char kFullscreenRefactoringName[] = "FullscreenRefactoring";
inline constexpr char kFullscreenRefactoringDescription[] =
    "Enables the FullscreenRefactoring feature.";

inline constexpr char kFullscreenScrollThresholdName[] =
    "Fullscreen Scroll Threshold";
inline constexpr char kFullscreenScrollThresholdDescription[] =
    "When enabled, scrolling must exceed a small threshold before the web view "
    "begins to enter or exit fullscreen.";

inline constexpr char kFullscreenSmoothScrollingName[] =
    "Fullscreen Smooth Scrolling";
inline constexpr char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

inline constexpr char kFullscreenTransitionSpeedName[] =
    "Fullscreen Transition Speed Tweaks";
inline constexpr char kFullscreenTransitionSpeedDescription[] =
    "When enabled, the speed of the fullscreen' transition is "
    "increased-decreased.";

inline constexpr char kGeminiActorName[] = "Gemini Actor";
inline constexpr char kGeminiActorDescription[] = "Enables the Gemini Actor.";

inline constexpr char kGeminiBackendMigrationName[] =
    "Gemini Backend Migration";
inline constexpr char kGeminiBackendMigrationDescription[] =
    "Enables the backend migration for Gemini.";

inline constexpr char kGeminiBinaryMigrationName[] = "Gemini Binary Migration";
inline constexpr char kGeminiBinaryMigrationDescription[] =
    "Enables the binary network migration for Gemini.";

inline constexpr char kGeminiChatPersistenceName[] = "Gemini Chat Persistence";
inline constexpr char kGeminiChatPersistenceDescription[] =
    "Enables improvements to Gemini Chat persistence.";

inline constexpr char kGeminiClientMigrationName[] = "Gemini Client Migration";
inline constexpr char kGeminiClientMigrationDescription[] =
    "Enables the client migration for Gemini, adding the infrastructure for "
    "several key features that render more than just text.";

inline constexpr char kGeminiCopresenceName[] = "Gemini Copresence";
inline constexpr char kGeminiCopresenceDescription[] =
    "Enables the Gemini Copresence feature, which provides a persistent Gemini "
    "overlay.";

inline constexpr char kGeminiDynamicSettingsName[] = "Gemini Dynamic Settings";
inline constexpr char kGeminiDynamicSettingsDescription[] =
    "Enables loading Gemini settings dynamically using the Gemini SDK.";

inline constexpr char kGeminiFloatyAllPagesName[] = "Gemini Floaty All Pages";
inline constexpr char kGeminiFloatyAllPagesDescription[] =
    "Enables the Gemini floaty on all pages.";

inline constexpr char kGeminiImageRemixToolName[] = "Gemini Image Remix Tool";
inline constexpr char kGeminiImageRemixToolDescription[] =
    "Enables the image remix tool in the Gemini floaty.";

inline constexpr char kGeminiLiveName[] = "GeminiLive";
inline constexpr char kGeminiLiveDescription[] = "Enables Gemini Live.";

inline constexpr char kGeminiMapsRichUIName[] = "Gemini Maps Rich UI";
inline constexpr char kGeminiMapsRichUIDescription[] =
    "Enables the rich Maps UI in Gemini.";

inline constexpr char kGeminiMultiTabContextName[] = "Gemini Multi Tab Context";
inline constexpr char kGeminiMultiTabContextDescription[] =
    "Enables attaching multiple tabs in Gemini.";

inline constexpr char kGeminiNavigationPromoName[] = "GeminiNavigationPromo";
inline constexpr char kGeminiNavigationPromoDescription[] =
    "Enables the automatic promo for Gemini on navigation.";

inline constexpr char kGeminiPreciseLocationName[] = "BWG Precise Location";
inline constexpr char kGeminiPreciseLocationDescription[] =
    "When enabled, the precise location row is shown in BWG settings.";

inline constexpr char kGeminiResponseViewDynamicResizingName[] =
    "Gemini Response View Dynamic Resizing";
inline constexpr char kGeminiResponseViewDynamicResizingDescription[] =
    "Enables dynamic resizing for the Gemini response view.";

inline constexpr char kGeminiRichAPCExtractionName[] =
    "Gemini Rich APC Extraction";
inline constexpr char kGeminiRichAPCExtractionDescription[] =
    "Enables rich APC extraction for Gemini.";

inline constexpr char kGeminiScreenContextMigrationName[] =
    "Gemini Screen Context Migration";
inline constexpr char kGeminiScreenContextMigrationDescription[] =
    "Enables migration from Gemini Page Context to Screen Context.";

inline constexpr char kGeminiUnaryMigrationName[] = "Gemini Unary Migration";
inline constexpr char kGeminiUnaryMigrationDescription[] =
    "Enables the unary network migration for Gemini.";

inline constexpr char kGeminiUpdatedEligibilityName[] =
    "Gemini Updated Eligibility";
inline constexpr char kGeminiUpdatedEligibilityDescription[] =
    "Enables the updated eligibility checks for Gemini users.";

inline constexpr char kGeneralizedGeminiEntryFlowName[] =
    "Generalized Gemini Entry Flow";
inline constexpr char kGeneralizedGeminiEntryFlowDescription[] =
    "Generalizes the Gemini entry flow to handle auth and eligibility outside "
    "of the Page Action Menu.";

inline constexpr char kHandleMdmErrorsForDasherAccountsName[] =
    "Mdm error handling for dasher accounts";
inline constexpr char kHandleMdmErrorsForDasherAccountsDescription[] =
    "Enables the mdm error handling feature for dasher accounts";

inline constexpr char kHideFuseboxVoiceLensActionsName[] =
    "Hide Voice and Lens in Fusebox";
inline constexpr char kHideFuseboxVoiceLensActionsDescription[] =
    "Hides voice and lens shortcuts in fusebox.";

inline constexpr char kHideToolbarsInOverflowMenuName[] =
    "Hide Toolbars in Overflow menu";
inline constexpr char kHideToolbarsInOverflowMenuDescription[] =
    "When enabled, adds a button in the overflow menu that force the "
    "fullscreen mode on iOS.";

inline constexpr char kHttpsUpgradesName[] = "HTTPS Upgrades";
inline constexpr char kHttpsUpgradesDescription[] =
    "When enabled, eligible navigations will automatically be upgraded to "
    "HTTPS.";

inline constexpr char kIOSActorToolsName[] = "iOS Actor Tools";
inline constexpr char kIOSActorToolsDescription[] =
    "Enables all actor tools on iOS.";

inline constexpr char kIOSBackendPromoServiceIntegrationName[] =
    "IOS Backend Promo Service Integration";
inline constexpr char kIOSBackendPromoServiceIntegrationDescription[] =
    "Enables Backend Promo Service integration.";

inline constexpr char kIOSBrowserEditMenuMetricsName[] =
    "Browser edit menu metrics";
inline constexpr char kIOSBrowserEditMenuMetricsDescription[] =
    "Collect metrics for edit menu usage.";

inline constexpr char kIOSBrowserReportIncludeAllProfilesName[] =
    "Include all profiles in browser reports";
inline constexpr char kIOSBrowserReportIncludeAllProfilesDescription[] =
    "When enabled, enterprise browser reports include all profiles (instead of "
    "only the current profile).";

inline constexpr char kIOSChooseFromDriveName[] = "IOS Choose from Drive";
inline constexpr char kIOSChooseFromDriveDescription[] =
    "Enables the Choose from Drive feature on iOS.";

inline constexpr char kIOSChooseFromDriveSignedOutName[] =
    "Choose from Drive Signed Out";
inline constexpr char kIOSChooseFromDriveSignedOutDescription[] =
    "Enables the Choose from Drive feature to signed out users.";

inline constexpr char kIOSCobaltDeveloperModeName[] =
    "IOS Cobalt Developer Mode";
inline constexpr char kIOSCobaltDeveloperModeDescription[] =
    "Enables the developer mode of the Cobalt feature on iOS.";

inline constexpr char kIOSCobaltName[] = "IOS Cobalt";
inline constexpr char kIOSCobaltDescription[] =
    "Enables the Cobalt feature on iOS.";

inline constexpr char kIOSCustomFileUploadMenuName[] =
    "Custom file upload menu";
inline constexpr char kIOSCustomFileUploadMenuDescription[] =
    "Enables the custom file upload menu implementation.";

inline constexpr char kIOSDateToCalendarSignedOutName[] =
    "Date to Calendar Signed Out";
inline constexpr char kIOSDateToCalendarSignedOutDescription[] =
    "When enabled, signed-out users can long-press detected dates to access "
    "the 'Add to Google Calendar' feature.";

inline constexpr char kIOSDockingPromoV2Name[] = "Docking Promo V2";
inline constexpr char kIOSDockingPromoV2Description[] =
    "When enabled, the user will be presented an animated, instructional "
    "promo V2 showing how to move Chrome to their native iOS dock.";

inline constexpr char kIOSEnableCloudProfileReportingName[] =
    "Enable profile reporting on iOS";
inline constexpr char kIOSEnableCloudProfileReportingDescription[] =
    "When enabled, profile reports will be reported to the user's "
    "organization.";

inline constexpr char kIOSEnableRealtimeEventReportingName[] =
    "Enable realtime event reporting on iOS";
inline constexpr char kIOSEnableRealtimeEventReportingDescription[] =
    "When enabled, realtime events will be reported to the user's "
    "organization.";

inline constexpr char kIOSExpandedSetupListName[] = "Expanded Setup List";
inline constexpr char kIOSExpandedSetupListDescription[] =
    "Enables a feature that adds new items in the Setup List.";

inline constexpr char kIOSExpandedTipsName[] = "Expanded Tips Notifications";
inline constexpr char kIOSExpandedTipsDescription[] =
    "Enables a feature that adds several new Tips Notifications that can be "
    "sent.";

inline constexpr char kIOSKeyboardAccessoryDefaultViewName[] =
    "Default Input Accessory View";
inline constexpr char kIOSKeyboardAccessoryDefaultViewDescription[] =
    "When enabled, a default Keyboard Accessory view with navigation buttons "
    "is provided for a <select> HTML element.";

inline constexpr char kIOSKeyboardAccessoryTwoBubbleName[] =
    "Enable the two-bubble design for the Keyboard Accessory view";
inline constexpr char kIOSKeyboardAccessoryTwoBubbleDescription[] =
    "When enabled, the two-bubble design is used for the Keyboard Accessory "
    "view.";

inline constexpr char kIOSLevelUpName[] = "Level Up";
inline constexpr char kIOSLevelUpDescription[] =
    "Enables the 'Level Up' feature on iOS.";

inline constexpr char kIOSMiniMapUniversalLinkCounterfactualName[] =
    "Counterfactual for opening Maps Universal links in native view";
inline constexpr char kIOSMiniMapUniversalLinkCounterfactualDescription[] =
    "Enables counterfactual logging for the maps universal link native preview "
    "experiment. It adds a `utm_campaign` parameter before opening the "
    "universal link in Maps Lite so that subsequent iGMM installs would be "
    "logged.";

inline constexpr char kIOSMiniMapUniversalLinkName[] =
    "Open Maps Universal links in native view.";
inline constexpr char kIOSMiniMapUniversalLinkDescription[] =
    "When enabled, maps universal links on Google Page are opened in "
    "native views (under conditions).";

inline constexpr char kIOSOmniboxAimServerEligibilityEnName[] =
    "AIM Server Eligibility EN locales";
inline constexpr char kIOSOmniboxAimServerEligibilityEnDescription[] =
    "Enable AIM server eligibility checks for EN locales.";

inline constexpr char kIOSOmniboxAimServerEligibilityName[] =
    "AIM Server Eligibility";
inline constexpr char kIOSOmniboxAimServerEligibilityDescription[] =
    "Enable AIM server eligibility checks for all locales.";

inline constexpr char kIOSOmniboxAimShortcutName[] =
    "Enable the omnibox aim shortcut";
inline constexpr char kIOSOmniboxAimShortcutDescription[] =
    "When enabled, an aim shortcut entrypoint will be displayed when the "
    "omnibox is on edit mode.";

inline constexpr char kIOSOneTapMiniMapRestrictionsName[] =
    "Revalidate detected addresses for one tap Mini Map.";
inline constexpr char kIOSOneTapMiniMapRestrictionsDescription[] =
    "Different restrictions to block false positive for one tap Mini Map.";

inline constexpr char kIOSOneTimeDefaultBrowserNotificationName[] =
    "One-time default browser notification";
inline constexpr char kIOSOneTimeDefaultBrowserNotificationDescription[] =
    "Enables a one-time notification to prompt the user to set the app as the "
    "default browser.";

inline constexpr char kIOSPasswordAutoSubmissionName[] =
    "Auto Submission for Password Autofill";
inline constexpr char kIOSPasswordAutoSubmissionDescription[] =
    "Enables automatic submission of password forms when filling credentials.";

inline constexpr char kIOSProactivePasswordGenerationBottomSheetName[] =
    "IOS Proactive Password Generation Bottom Sheet";
inline constexpr char kIOSProactivePasswordGenerationBottomSheetDescription[] =
    "Enables the display of the proactive password generation bottom sheet on "
    "IOS.";

inline constexpr char kIOSProvidesAppNotificationSettingsName[] =
    "IOS Provides App Notification Settings";
inline constexpr char kIOSProvidesAppNotificationSettingsDescription[] =
    "Enabled integration with iOS's ProvidesAppNotificationSettings feature.";

inline constexpr char kIOSSaveToDriveSignedOutName[] =
    "Save to Drive Signed Out";
inline constexpr char kIOSSaveToDriveSignedOutDescription[] =
    "Enables the Save to Drive feature to signed out users.";

inline constexpr char kIOSSaveToPhotosSignedOutName[] =
    "Save to Photos Signed Out";
inline constexpr char kIOSSaveToPhotosSignedOutDescription[] =
    "Enables the Save to Photos feature to signed out users.";

inline constexpr char kIOSSoftLockName[] = "Soft Lock on iOS";
inline constexpr char kIOSSoftLockDescription[] =
    "Enables experimental Soft Lock on iOS.";

inline constexpr char kIOSSyncedSetUpName[] = "Synced Set Up";
inline constexpr char kIOSSyncedSetUpDescription[] =
    "Enables the Synced Set Up experience, allowing the user to locally apply "
    "settings from their synced devices.";

inline constexpr char kIOSTabRemindersName[] = "Tab Reminders";
inline constexpr char kIOSTabRemindersDescription[] =
    "Enables the Tab Reminder notifications feature on iOS.";

inline constexpr char kIOSTipsNotificationsStringAlternativesName[] =
    "Tips notifications alternative string experiment";
inline constexpr char kIOSTipsNotificationsStringAlternativesDescription[] =
    "Enables different alternative strings for tips notifications";

inline constexpr char kIOSTrustedVaultNotificationName[] =
    "Enable the trusted vault notification on iOS";
inline constexpr char kIOSTrustedVaultNotificationDescription[] =
    "When enabled and when the trusted vault key is missing, the provisional "
    "notification will be delivered.";

inline constexpr char kIOSWebContextMenuNewTitleName[] =
    "Use the new title for the Web context menu";
inline constexpr char kIOSWebContextMenuNewTitleDescription[] =
    "Enables actions in context menu title instead of customized action for "
    "web context menu.";

inline constexpr char kIdentityConfirmationSnackbarName[] =
    "Identity Confirmation Snackbar";
inline constexpr char kIdentityConfirmationSnackbarDescription[] =
    "When enabled, the identity confirmation snackbar will show on startup.";

inline constexpr char kInFlowTrustedVaultKeyRetrievalIosName[] =
    "In-flow Trusted Vault key retrieval";
inline constexpr char kInFlowTrustedVaultKeyRetrievalIosDescription[] =
    "Starts the key retrieval flow after offering to save a password.";

inline constexpr char kInProductHelpDemoModeName[] =
    "In-Product Help Demo Mode";
inline constexpr char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

inline constexpr char kIndicateIdentityErrorInOverflowMenuName[] =
    "Indicate Identity Error in Overflow Menu";
inline constexpr char kIndicateIdentityErrorInOverflowMenuDescription[] =
    "When enabled, the Overflow Menu indicates the identity error with an "
    "error badge on the Settings destination";

inline constexpr char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[] =
        "Invalidate search engine choice after device restore";
inline constexpr char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[] =
        "When enabled, search engine choices made before backup & restore will "
        "not "
        "be considered valid on the restored device, leading to the choice "
        "screen "
        "potentially retriggering.";

inline constexpr char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName[] =
        "Lens blocks fetch objects interaction RPCs on separate handshake";
inline constexpr char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription[] =
        "When enabled, RPCs are blocked on separate handshake.";

inline constexpr char kLensCameraNoStillOutputRequiredName[] =
    "Lens camera avoids creating unused outputs";
inline constexpr char kLensCameraNoStillOutputRequiredDescription[] =
    "When enabled, Lens camera doesn't create unused still output.";

inline constexpr char kLensCameraUnbinnedCaptureFormatsPreferredName[] =
    "Lens camera prefers unbinned formats";
inline constexpr char kLensCameraUnbinnedCaptureFormatsPreferredDescription[] =
    "When enabled, Lens camera prefers unbinned pixel formats.";

inline constexpr char kLensContinuousZoomEnabledName[] =
    "Enable Lens camera continuous zoom";
inline constexpr char kLensContinuousZoomEnabledDescription[] =
    "When enabled, Lens camera supports continuous zoom.";

inline constexpr char kLensEnableSendRawFileMediaTypesName[] =
    "Lens enable send raw file media types";
inline constexpr char kLensEnableSendRawFileMediaTypesDescription[] =
    "Enables sending raw file media types in the Lens overlay.";

inline constexpr char kLensEnableSendUrlsInComposeboxesName[] =
    "Lens enable send urls in composeboxes";
inline constexpr char kLensEnableSendUrlsInComposeboxesDescription[] =
    "Enables sending urls in AIM composeboxes.";

inline constexpr char kLensExactMatchesEnabledName[] =
    "Lens exact matches enabled";
inline constexpr char kLensExactMatchesEnabledDescription[] =
    "Enables exact matches in the Lens results.";

inline constexpr char kLensFetchSrpApiEnabledName[] =
    "Lens fetch SRP API enabled";
inline constexpr char kLensFetchSrpApiEnabledDescription[] =
    "Enables the fetch SRP API.";

inline constexpr char kLensFilterToggleEnabledName[] =
    "Lens filter toggle enabled";
inline constexpr char kLensFilterToggleEnabledDescription[] =
    "Enables the filter toggle in Lens camera.";

inline constexpr char kLensFiltersAblationModeEnabledName[] =
    "Lens filters ablation mode enabled";
inline constexpr char kLensFiltersAblationModeEnabledDescription[] =
    "Enables the filters ablation mode.";

inline constexpr char kLensGestureTextSelectionDisabledName[] =
    "Disable Lens gesture text selection";
inline constexpr char kLensGestureTextSelectionDisabledDescription[] =
    "When disabled, turns off gesture text selection.";

inline constexpr char kLensInitialLvfZoomLevel90PercentName[] =
    "Initial Lens camera zoom 90 percent";
inline constexpr char kLensInitialLvfZoomLevel90PercentDescription[] =
    "When enabled, sets the initial Lens camera zoom level to 90 percent.";

inline constexpr char kLensLoadAIMInLensResultPageName[] =
    "Enable loading AIM in the Lens result page";
inline constexpr char kLensLoadAIMInLensResultPageDescription[] =
    "Opens in Lens result page rather than a new tab.";

inline constexpr char kLensOmnientShaderV2EnabledName[] =
    "Enable Lens Omnient Shader V2";
inline constexpr char kLensOmnientShaderV2EnabledDescription[] =
    "When enabled, Lens Omnient will use the new Shader V2";

inline constexpr char kLensOverlayCustomBottomSheetName[] =
    "Use a custom bottom sheet presentation for Lens Overlay";
inline constexpr char kLensOverlayCustomBottomSheetDescription[] =
    "When enabled the system bottom sheet for the Lens result page is "
    "replaced by a custom bottom sheet presentation";

inline constexpr char kLensOverlayEnableLandscapeCompatibilityName[] =
    "Allow Lens overlay to also run in landscape if the feature is enabled";
inline constexpr char kLensOverlayEnableLandscapeCompatibilityDescription[] =
    "When enabled, it allows Lens Overlay to run in landscape orientation";

inline constexpr char kLensOverlayNavigationHistoryName[] =
    "Enable Lens overlay navigation history";
inline constexpr char kLensOverlayNavigationHistoryDescription[] =
    "When enabled, web navigation in the Lens overlay are recorded in browser "
    "history.";

inline constexpr char kLensPrewarmHardStickinessInInputSelectionName[] =
    "Lens prewarm hard stickiness in input selection";
inline constexpr char kLensPrewarmHardStickinessInInputSelectionDescription[] =
    "When enabled, input selection prewarms hard stickiness.";

inline constexpr char kLensPrewarmHardStickinessInQueryFormulationName[] =
    "Lens prewarm hard stickiness in query formulation";
inline constexpr char
    kLensPrewarmHardStickinessInQueryFormulationDescription[] =
        "When enabled, query formulation prewarms hard stickiness.";

inline constexpr char kLensSearchHeadersCheckEnabledName[] =
    "Lens search headers check";
inline constexpr char kLensSearchHeadersCheckEnabledDescription[] =
    "When enabled, ensures headers are attached to Lens search requests.";

inline constexpr char kLensSingleTapTextSelectionDisabledName[] =
    "Disable Lens single tap text selection";
inline constexpr char kLensSingleTapTextSelectionDisabledDescription[] =
    "When disabled, single taps do not trigger text selections.";

inline constexpr char kLensStreamServiceWebChannelTransportEnabledName[] =
    "Lens stream service web channel transport";
inline constexpr char
    kLensStreamServiceWebChannelTransportEnabledDescription[] =
        "When enabled, uses web channel transport for the stream service.";

inline constexpr char kLensTranslateToggleModeEnabledName[] =
    "Lens translate toggle mode enabled";
inline constexpr char kLensTranslateToggleModeEnabledDescription[] =
    "Enables the translate toggle mode.";

inline constexpr char kLensTripleCameraEnabledName[] =
    "Enable Lens triple camera";
inline constexpr char kLensTripleCameraEnabledDescription[] =
    "When enabled, Lens LVF uses virtual triple camera.";

inline constexpr char kLensUnaryApiSalientTextEnabledName[] =
    "Lens unary API salient text enabled";
inline constexpr char kLensUnaryApiSalientTextEnabledDescription[] =
    "Enables the unary salient text API.";

inline constexpr char kLensUnaryApisWithHttpTransportEnabledName[] =
    "Lens unary APIs with HTTP transport enabled";
inline constexpr char kLensUnaryApisWithHttpTransportEnabledDescription[] =
    "Enables the unary APIs with HTTP transport.";

inline constexpr char kLensUnaryHttpTransportEnabledName[] =
    "Lens unary HTTP transport enabled";
inline constexpr char kLensUnaryHttpTransportEnabledDescription[] =
    "Enables the HTTP transport for unary requests.";

inline constexpr char kLocationBarBadgeMigrationName[] =
    "LocationBarBadgeMigration";
inline constexpr char kLocationBarBadgeMigrationDescription[] =
    "Enables the LocationBarBadgeMigration feature.";

inline constexpr char kLockBottomToolbarName[] = "Lock bottom toolbar";
inline constexpr char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

inline constexpr char kManualLogUploadsInFREName[] =
    "Manual log uploads in the FRE";
inline constexpr char kManualLogUploadsInFREDescription[] =
    "Enables triggering an UMA log upload after each FRE screen.";

inline constexpr char kMeasurementsName[] = "Measurements experience enable";
inline constexpr char kMeasurementsDescription[] =
    "When enabled, one tapping or long pressing on a measurement will trigger "
    "the measurement conversion experience.";

inline constexpr char kMetrickitNonCrashReportName[] =
    "Metrickit non-crash reports";
inline constexpr char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

inline constexpr char kMigrateIOSKeychainAccessibilityName[] =
    "Migrate iOS Keychain Accessibility";
inline constexpr char kMigrateIOSKeychainAccessibilityDescription[] =
    "Migrate the accessibility attribute in the iOS keychain to 'after first "
    "unlock'.";

inline constexpr char kMobilePromoOnDesktopName[] = "Mobile Promo On Desktop";
inline constexpr char kMobilePromoOnDesktopDescription[] =
    "When enabled, shows a mobile promo on the desktop new tab page.";

inline constexpr char kMobilePromoOnDesktopRecordActiveDaysName[] =
    "Mobile Promo On Desktop Record Active Days";
inline constexpr char kMobilePromoOnDesktopRecordActiveDaysDescription[] =
    "When enabled, records the user's number of active days for the mobile "
    "promo on desktop.";

inline constexpr char kMobilePromoOnDesktopWave1Name[] =
    "Mobile Promo On Desktop (Wave 1)";
inline constexpr char kMobilePromoOnDesktopWave1Description[] =
    "When enabled, shows a mobile promo with a reminder flow on desktop for "
    "eligible users. This version highlights features not included in the "
    "existing mobile promos.";

inline constexpr char kModelBasedPageClassificationName[] =
    "Model Based Page Classification";
inline constexpr char kModelBasedPageClassificationDescription[] =
    "Enables the model based page classification.";

inline constexpr char kMostVisitedTilesCustomizationName[] =
    "Most Visited Tiles Customization on iOS";
inline constexpr char kMostVisitedTilesCustomizationDescription[] =
    "Enables customization of Most Visited tiles on the New Tab Page.";

inline constexpr char kMostVisitedTilesHorizontalRenderGroupName[] =
    "MVTiles Horizontal Render Group";
inline constexpr char kMostVisitedTilesHorizontalRenderGroupDescription[] =
    "When enabled, the MV tiles are represented as individual matches";

inline constexpr char kNTPBackgroundColorSliderName[] =
    "Enable the background color slider in the background customization color "
    "picker";
inline constexpr char kNTPBackgroundColorSliderDescription[] =
    "When enabled, the color slider is available in the background "
    "customization color picker.";

inline constexpr char kNTPBackgroundDownsampleImageName[] =
    "NTP Background Downsample Image";
inline constexpr char kNTPBackgroundDownsampleImageDescription[] =
    "Downsamples user-uploaded NTP background images to screen size, "
    "reducing memory usage.";

inline constexpr char kNTPHeaderUseTransformsForAnimationsName[] =
    "NTP Header Transform Animations";
inline constexpr char kNTPHeaderUseTransformsForAnimationsDescription[] =
    "Use high-performance transforms for NTP header animations instead of "
    "updating constraints on scroll.";

inline constexpr char kNativeFindInPageName[] = "Native Find in Page";
inline constexpr char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

inline constexpr char kNewTabPageFieldTrialName[] =
    "New tab page features that target new users";
inline constexpr char kNewTabPageFieldTrialDescription[] =
    "Enables new tab page features that are available on first run for new "
    "Chrome iOS users.";

inline constexpr char kNoAccountWebSigninName[] =
    "Enable no account web sigin bottom sheet";
inline constexpr char kNoAccountWebSigninDescription[] =
    "Surfaces the web sign in bottom sheet when the user attempts to sign in "
    "to the web.";

inline constexpr char kNonModalSignInPromoName[] = "Non-modal sign-in promo";
inline constexpr char kNonModalSignInPromoDescription[] =
    "Enables a non-modal sign-in promo that prompts users to sign in.";

inline constexpr char kNotificationCollisionManagementName[] =
    "Notification collision management";
inline constexpr char kNotificationCollisionManagementDescription[] =
    "Enables delays to notifications to space them out more";

inline constexpr char kNtpAlphaBackgroundCollectionsName[] =
    "Enable alpha background collections";
inline constexpr char kNtpAlphaBackgroundCollectionsDescription[] =
    "When enabled, the alpha background collections are available on the NTP.";

inline constexpr char kNtpComposeboxUsesChromeComposeClientName[] =
    "Enable composebox to use the suggest chrome compose client";
inline constexpr char kNtpComposeboxUsesChromeComposeClientDescription[] =
    "When enabled, the composebox will use the suggest chrome compose client "
    "when AIM is enabled";

inline constexpr char kOmniboxCrashFixKillSwitchName[] =
    "OmniboxCrashFixKillSwitch";
inline constexpr char kOmniboxCrashFixKillSwitchDescription[] =
    "Enables the OmniboxCrashFixKillSwitch feature.";

inline constexpr char kOmniboxDRSPrototypeName[] =
    "Enable the Omnibox DRS prototype";
inline constexpr char kOmniboxDRSPrototypeDescription[] =
    "Enables the omnibox dynamic response system prototype";

inline constexpr char kOmniboxGroupingFrameworkForTypedSuggestionsName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
inline constexpr char
    kOmniboxGroupingFrameworkForTypedSuggestionsDescription[] =
        "Enables an alternative grouping implementation for omnibox "
        "autocompletion.";

inline constexpr char kOmniboxGroupingFrameworkForZPSName[] =
    "Omnibox Grouping Framework for ZPS";
inline constexpr char kOmniboxGroupingFrameworkForZPSDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

inline constexpr char kOmniboxHttpsUpgradesName[] = "Omnibox HTTPS upgrades";
inline constexpr char kOmniboxHttpsUpgradesDescription[] =
    "Enables HTTPS upgrades for omnibox navigations typed without a scheme";

inline constexpr char kOmniboxInspireMeSignedOutName[] =
    "Omnibox Trending Queries For Signed-Out users";
inline constexpr char kOmniboxInspireMeSignedOutDescription[] =
    "When enabled, appends additional suggestions based on local trends and "
    "optionally extends the ZPS limit (for signed out users).";

inline constexpr char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
inline constexpr char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

inline constexpr char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL matches";
inline constexpr char kOmniboxMaxURLMatchesDescription[] =
    "Limit the number of URL suggestions in the omnibox. The omnibox will "
    "still display more than MaxURLMatches if there are no non-URL suggestions "
    "to replace them.";

inline constexpr char kOmniboxMiaZpsName[] = "Omnibox Mia ZPS on NTP";
inline constexpr char kOmniboxMiaZpsDescription[] =
    "Enables Mia ZPS suggestions in NTP omnibox";

inline constexpr char kOmniboxMlLogUrlScoringSignalsName[] =
    "Log Omnibox URL Scoring Signals";
inline constexpr char kOmniboxMlLogUrlScoringSignalsDescription[] =
    "Enables Omnibox to log scoring signals of URL suggestions.";

inline constexpr char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[] =
    "Omnibox ML Scoring with Piecewise Score Mapping";
inline constexpr char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores using "
    "a piecewise ML score mapping function.";

inline constexpr char kOmniboxMlUrlScoreCachingName[] =
    "Omnibox ML URL Score Caching";
inline constexpr char kOmniboxMlUrlScoreCachingDescription[] =
    "Enables in-memory caching of ML URL scores.";

inline constexpr char kOmniboxMlUrlScoringModelName[] =
    "Omnibox URL Scoring Model";
inline constexpr char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL sugestions.";

inline constexpr char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
inline constexpr char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

inline constexpr char kOmniboxMlUrlSearchBlendingName[] =
    "Omnibox ML URL Search Blending";
inline constexpr char kOmniboxMlUrlSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores.";

inline constexpr char kOmniboxOnClobberFocusTypeOnIOSName[] =
    "Omnibox On Clobber Focus Type On IOS";
inline constexpr char kOmniboxOnClobberFocusTypeOnIOSDescription[] =
    "Send ON_CLOBBER focus type for zero-prefix requests with an empty input "
    "on Web/SRP on IOS platform.";

inline constexpr char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
inline constexpr char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for incognito";

inline constexpr char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[] =
    "Omnibox on device head suggestions (non-incognito only)";
inline constexpr char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for non-incognito";

inline constexpr char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
inline constexpr char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

inline constexpr char kOmniboxSuggestionAnswerMigrationName[] =
    "Omnibox suggestion answer migration";
inline constexpr char kOmniboxSuggestionAnswerMigrationDescription[] =
    "Enables omnibox Suggestion answer migration, when enabled the omnibox "
    "will use the migrated Answer_template instead of answer.";

inline constexpr char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
inline constexpr char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

inline constexpr char kOmniboxZeroSuggestPrefetchingOnSRPName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on SRP";
inline constexpr char kOmniboxZeroSuggestPrefetchingOnSRPDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Search Results page.";

inline constexpr char kOmniboxZeroSuggestPrefetchingOnWebName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on the Web";
inline constexpr char kOmniboxZeroSuggestPrefetchingOnWebDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Web (i.e. non-NTP and non-SRP URLs).";

inline constexpr char kOpenEditGroupViewByTappingTitleName[] =
    "OpenEditGroupViewByTappingTitle";
inline constexpr char kOpenEditGroupViewByTappingTitleDescription[] =
    "Enables the OpenEditGroupViewByTappingTitle feature.";

inline constexpr char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
inline constexpr char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

inline constexpr char kPageActionMenuAuthFlowName[] =
    "Page Action Menu Auth Flow";
inline constexpr char kPageActionMenuAuthFlowDescription[] =
    "When enabled, the Page Action Menu entry point becomes stable and "
    "supports the Ask Gemini auth flow.";

inline constexpr char kPageActionMenuIconName[] = "PageActionMenuIcon";
inline constexpr char kPageActionMenuIconDescription[] =
    "When enabled, changes the icon for the page action menu entry point.";

inline constexpr char kPageActionMenuName[] = "Page Action Menu";
inline constexpr char kPageActionMenuDescription[] =
    "When enabled, the entry point for the Page Action Menu becomes available "
    "for actions relating to the web page.";

inline constexpr char kPageContentAnnotationsName[] =
    "Page content annotations";
inline constexpr char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

inline constexpr char kPageContentAnnotationsRemotePageMetadataName[] =
    "Page content annotations - Remote page metadata";
inline constexpr char kPageContentAnnotationsRemotePageMetadataDescription[] =
    "Enables fetching of page load metadata to be persisted on-device.";

inline constexpr char kPageContextIPCOptimizationName[] =
    "PageContextIPCOptimization";
inline constexpr char kPageContextIPCOptimizationDescription[] =
    "Enables the PageContextIPCOptimization feature.";

inline constexpr char kPageToolsFeatureUnavailabilityName[] =
    "PageToolsFeatureUnavailability";
inline constexpr char kPageToolsFeatureUnavailabilityDescription[] =
    "Enables the PageToolsFeatureUnavailability feature.";

inline constexpr char kPasswordRemovalFromDeleteBrowsingDataName[] =
    "Removal of Passwords from Quick Delete Browsing Data";
inline constexpr char kPasswordRemovalFromDeleteBrowsingDataDescription[] =
    "Disables the deletion of passwords via the quick delete bottom sheet. "
    "Enables a new navigational view towards the appropriate pages to delete "
    "passwords or manage other Google data (Search History and My Activities).";

inline constexpr char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
inline constexpr char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

inline constexpr char kPasswordSharingName[] = "Enables password sharing";
inline constexpr char kPasswordSharingDescription[] =
    "Enables password sharing between members of the same family.";

inline constexpr char kPersistTabContextName[] =
    "Persist Tab APC and Inner Text";
inline constexpr char kPersistTabContextDescription[] =
    "Enables persisting tab APC and inner text in storage for fast access to "
    "multi-tab context.";

inline constexpr char kPersistTabContextRichExtractionName[] =
    "PersistTabContextRichExtraction";
inline constexpr char kPersistTabContextRichExtractionDescription[] =
    "Enables the PersistTabContextRichExtraction feature.";

inline constexpr char kPersistentDefaultBrowserPromoName[] =
    "Persist default browser promo through app backgrounding";
inline constexpr char kPersistentDefaultBrowserPromoDescription[] =
    "When enabled, the default browser promo will persist through "
    "backgrounding the app so the instructions remain visible when coming "
    "back.";

inline constexpr char kPhoneNumberName[] = "Phone number experience enable";
inline constexpr char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

inline constexpr char kPlusButtonInFakeboxName[] =
    "Enable plus button in fakebox NTP";
inline constexpr char kPlusButtonInFakeboxDescription[] =
    "When enabled, the fakebox NTP can contain a plus button for multimodal "
    "actions";

inline constexpr char kPriceTrackingPromoName[] =
    "Enables price tracking notification promo card";
inline constexpr char kPriceTrackingPromoDescription[] =
    "Enables being able to show the card in the Magic Stack";

inline constexpr char kProactiveSuggestionsFrameworkName[] =
    "Proactive Suggestions Framework";
inline constexpr char kProactiveSuggestionsFrameworkDescription[] =
    "When enabled, consolidates omnibox proactive suggestions (Reader Mode, "
    "Translate, Price History, etc.) into a unified badge system with "
    "centralized settings access through the AI Hub Page Tools.";

inline constexpr char kProactiveSuggestionsFrameworkPopupBlockerName[] =
    "Popup Blocker";
inline constexpr char kProactiveSuggestionsFrameworkPopupBlockerDescription[] =
    "Enables the popup blocker feature row in the Page Action Menu.";

inline constexpr char kProvisionalNotificationAlertName[] =
    "Provisional notifiation alert on iOS";
inline constexpr char kProvisionalNotificationAlertDescription[] =
    "Shows an alert to the user when app notification settings are changed but "
    "only provisonal notifications are enabled";

inline constexpr char kRcapsDynamicProfileCountryName[] =
    "Dynamic Profile Country";
inline constexpr char kRcapsDynamicProfileCountryDescription[] =
    "When enabled, Chrome updates the country associated with "
    "the profile on open";

inline constexpr char kReaderModeContentSettingsForLinksName[] =
    "Enables Content Settings options for Reading Mode";
inline constexpr char kReaderModeContentSettingsForLinksDescription[] =
    "Enables Content Settings options for disabling/enabling links in Reading "
    "Mode.";

inline constexpr char kReaderModeIgnoreBadgeThresholdName[] =
    "Reader Mode ignore badge threshold";
inline constexpr char kReaderModeIgnoreBadgeThresholdDescription[] =
    "When enabled, the badge threshold is ignored for Reader Mode.";

inline constexpr char kReaderModeOmniboxEntrypointInUSName[] =
    "Reader Mode Omnibox Entrypoint In US";
inline constexpr char kReaderModeOmniboxEntrypointInUSDescription[] =
    "Enables the omnibox entrypoint for Reader Mode for users in the US.";

inline constexpr char kReaderModeOptimizationGuideEligibilityName[] =
    "Enables Reader Mode Optimization Guide Eligibility";
inline constexpr char kReaderModeOptimizationGuideEligibilityDescription[] =
    "Enables the optimization guide eligibility check for Reader Mode.";

inline constexpr char kReaderModeReadabilityDistillerName[] =
    "Enables Readability distiller for Reader Mode";
inline constexpr char kReaderModeReadabilityDistillerDescription[] =
    "Enables Readability distiller for Reader Mode UI.";

inline constexpr char kReaderModeReadabilityHeuristicName[] =
    "Enables Readability heuristic for Reader Mode";
inline constexpr char kReaderModeReadabilityHeuristicDescription[] =
    "Enables Readability heuristic for Reader Mode UI.";

inline constexpr char kReaderModeSupportNewFontsName[] =
    "Reader Mode support new fonts";
inline constexpr char kReaderModeSupportNewFontsDescription[] =
    "Enables new accessible font options in Reader Mode.";

inline constexpr char kReaderModeTranslationWithInfobarName[] =
    "Enables Reader Mode Translation Settings";
inline constexpr char kReaderModeTranslationWithInfobarDescription[] =
    "Enables translation of web pages in Reader Mode with Settings available "
    "via the infobar.";

inline constexpr char kReaderModeUSEnabledName[] = "Enables Reader Mode in US";
inline constexpr char kReaderModeUSEnabledDescription[] =
    "Enables Reader Mode for users in the US. Requires reader-mode-enabled.";

inline constexpr char kRefactorToolbarsSizeName[] = "Refactor toolbars size";
inline constexpr char kRefactorToolbarsSizeDescription[] =
    "When enabled, the toolbars size does not use broadcaster but observers.";

inline constexpr char kRemoveExcessNTPsExperimentName[] =
    "Remove extra New Tab Pages";
inline constexpr char kRemoveExcessNTPsExperimentDescription[] =
    "When enabled, extra tabs with the New Tab Page open and no navigation "
    "history will be removed.";

inline constexpr char kSafeBrowsingAvailableName[] =
    "Make Safe Browsing available";
inline constexpr char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

inline constexpr char kSafeBrowsingRealTimeLookupName[] =
    "Enable real-time Safe Browsing";
inline constexpr char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

inline constexpr char kSafeBrowsingTrustedURLName[] =
    "Enable the Trusted URL for Safe Browsing";
inline constexpr char kSafeBrowsingTrustedURLDescription[] =
    "When enabled, chrome://safe-browsing will be accessible.";

inline constexpr char kSegmentationPlatformEphemeralCardRankerName[] =
    "Enable Segmentation Ranking for Ephemeral Cards";
inline constexpr char kSegmentationPlatformEphemeralCardRankerDescription[] =
    "Enables the segmentation platform to rank ephemeral cards in the Magic "
    "Stack";

inline constexpr char kSegmentationPlatformIosModuleRankerCachingName[] =
    "Enabled Magic Stack Segmentation Ranking Caching";
inline constexpr char kSegmentationPlatformIosModuleRankerCachingDescription[] =
    "Enables the Segmentation platform to cache the Magic Stack module rank "
    "for Start";

inline constexpr char kSegmentationPlatformIosModuleRankerName[] =
    "Enable Magic Stack Segmentation Ranking";
inline constexpr char kSegmentationPlatformIosModuleRankerDescription[] =
    "Enables the Segmentation platform to rank Magic Stack modules";

inline constexpr char kSegmentationPlatformIosModuleRankerSplitBySurfaceName[] =
    "Enable Magic Stack Segmentation Ranking split by surface";
inline constexpr char
    kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription[] =
        "Enables the Magic Stack module ranking to be split by surface for "
        "engagement";

inline constexpr char kSendTabToSelfEnhancedHandoffName[] =
    "Send Tab To Self enhanced handoff";
inline constexpr char kSendTabToSelfEnhancedHandoffDescription[] =
    "Enables an enhanced version of Send Tab To Self that propagates more "
    "information, such as form fields.";

inline constexpr char kShareInOmniboxLongPressName[] =
    "Share in Omnibox Long Press";
inline constexpr char kShareInOmniboxLongPressDescription[] =
    "Displays an option to share current page in the omnibox long press menu";

inline constexpr char kShareInOverflowMenuName[] = "Share in Overflow Menu";
inline constexpr char kShareInOverflowMenuDescription[] =
    "Displays share menu item in overflow menu";

inline constexpr char kShareInVerbatimMatchName[] = "Share in Verbatim Match";
inline constexpr char kShareInVerbatimMatchDescription[] =
    "Displays share button in the omnibox verbatim match";

inline constexpr char kSharedHighlightingIOSName[] =
    "Enable Shared Highlighting features";
inline constexpr char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

inline constexpr char kShopCardName[] = "Enables Tab Resumption ShopCard";
inline constexpr char kShopCardDescription[] =
    "Enables being able to show Tab Resumption ShopCard in the Magic Stack";

inline constexpr char kShowAutofillTypePredictionsName[] =
    "Show Autofill predictions";
inline constexpr char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

inline constexpr char kShowTabGroupInGridOnStartName[] =
    "Show tab group in grid on start";
inline constexpr char kShowTabGroupInGridOnStartDescription[] =
    "Show tab group in grid on start if the last activation is within a "
    "specific time interval";

inline constexpr char kSkipDefaultBrowserPromoInFirstRunName[] =
    "Skip the FRE Default Browser Promo in EEA";
inline constexpr char kSkipDefaultBrowserPromoInFirstRunDescription[] =
    "When enabled, users in the EEA will not see a Default Browser Promo in "
    "the FRE.";

inline constexpr char kSmartTabGroupingName[] = "Enable Smart Tab Grouping";
inline constexpr char kSmartTabGroupingDescription[] =
    "When enabled, users will have access to use the smart tab grouping "
    "feature in the tab grid.";

inline constexpr char kSmoothScrollingUseDelegateName[] =
    "Fullscreen Smooth Scrolling No Broadcaster";
inline constexpr char kSmoothScrollingUseDelegateDescription[] =
    "When enabled, the SmoothScrollingDefault experiment uses the regular "
    "UIScrollViewDelegate instead of KVO and broadcasting.";

inline constexpr char kSnapshotCompressedJPEGQualityName[] =
    "Snapshot Compressed JPEG Quality";
inline constexpr char kSnapshotCompressedJPEGQualityDescription[] =
    "Reduces snapshot JPEG quality from 1.0 to 0.97 for visually lossless "
    "compression, reducing file size by ~3-5x.";

inline constexpr char kSnapshotDownsampleImageName[] =
    "Snapshot Downsample Image";
inline constexpr char kSnapshotDownsampleImageDescription[] =
    "Downsamples tab snapshots to half resolution before writing to disk, "
    "reducing storage and I/O while keeping full resolution in memory.";

inline constexpr char kStrokesAPIEnabledName[] = "Enable Strokes API for Lens";
inline constexpr char kStrokesAPIEnabledDescription[] =
    "When enabled, Lens will use the Strokes API.";

inline constexpr char kSupervisedUserBlockInterstitialV3Name[] =
    "Enable URL filter interstitial V3";
inline constexpr char kSupervisedUserBlockInterstitialV3Description[] =
    "Enables URL filter interstitial V3 for Family Link users.";

inline constexpr char kSupervisedUserEmitLogRecordSeparatelyName[] =
    "Emit supervised user log record separately";
inline constexpr char kSupervisedUserEmitLogRecordSeparatelyDescription[] =
    "Emit supervised user log record separately for Family Link and device "
    "parental controls users (no user-visible effect).";

inline constexpr char
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsName[] =
        "Merge device parental controls and Family Link prefs";
inline constexpr char
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsDescription[] =
        "Merges non-web filtering device parental controls settings with "
        "Family Link settings in the SupervisedUserPrefStore (no user-visible "
        "effect).";

inline constexpr char kSupervisedUserUseUrlFilteringServiceName[] =
    "Use URL filtering service";
inline constexpr char kSupervisedUserUseUrlFilteringServiceDescription[] =
    "Use the SupervisedUserUrlFilteringService to get URL filtering settings "
    "directly from supervision services instead of using PrefService (no "
    "user-visible effect).";

inline constexpr char kSyncAIThreadsName[] = "Sync AI Threads";
inline constexpr char kSyncAIThreadsDescription[] =
    "Enables syncing of AI threads across devices.";

inline constexpr char kSyncAccountSettingsName[] = "Sync account settings";
inline constexpr char kSyncAccountSettingsDescription[] =
    "Enables syncing account settings to the server.";

inline constexpr char kSyncAutofillValuableMetadataName[] =
    "Sync autofill valuable metadata";
inline constexpr char kSyncAutofillValuableMetadataDescription[] =
    "Enables syncing valuable metadata for autofill to the server.";

inline constexpr char kSyncAutofillValuableName[] = "Sync autofill valuable";
inline constexpr char kSyncAutofillValuableDescription[] =
    "Enables syncing valuable for autofill to the server.";

inline constexpr char kSyncContextualTaskName[] = "Sync Contextual Task";
inline constexpr char kSyncContextualTaskDescription[] =
    "Enables syncing of contextual tasks.";

inline constexpr char kSyncSandboxName[] = "Use Chrome Sync sandbox";
inline constexpr char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

inline constexpr char kSyncThemesIosName[] = "Enable Sync Themes on iOS";
inline constexpr char kSyncThemesIosDescription[] =
    "Enables syncing of themes across iOS devices.";

inline constexpr char kSyncTrustedVaultInfobarMessageImprovementsName[] =
    "Trusted vault infobar message improvements";
inline constexpr char kSyncTrustedVaultInfobarMessageImprovementsDescription[] =
    "Enables massage improvements for the UI of the trusted vault error "
    "infobar.";

inline constexpr char kSyncWalletFlightReservationsName[] =
    "Sync wallet flight reservations";
inline constexpr char kSyncWalletFlightReservationsDescription[] =
    "Enables syncing flight reservations in the wallet to the server.";

inline constexpr char kSyncWalletVehicleRegistrationsName[] =
    "Sync wallet vehicle registrations";
inline constexpr char kSyncWalletVehicleRegistrationsDescription[] =
    "Enables syncing vehicle registrations in the wallet to the server.";

inline constexpr char kTabGridNewTransitionsName[] =
    "Enable new TabGrid transitions";
inline constexpr char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

inline constexpr char kTabGroupColorOnSurfaceName[] =
    "Tab group color on surfaces";
inline constexpr char kTabGroupColorOnSurfaceDescription[] =
    "Adds the tab group color to the tab group and tab grid surfaces.";

inline constexpr char kTabGroupInOverflowMenuName[] =
    "Enable the Tab Group button in the overflow menu";
inline constexpr char kTabGroupInOverflowMenuDescription[] =
    "When enabled, a Tab Group button will appear in the overflow menu.";

inline constexpr char kTabGroupIndicatorName[] = "Tab Group Indicator";
inline constexpr char kTabGroupIndicatorDescription[] =
    "When enabled, displays a tab group indicator next to the omnibox.";

inline constexpr char kTabGroupSyncName[] = "Enable Tab Group Sync";
inline constexpr char kTabGroupSyncDescription[] =
    "When enabled, tab groups are synced between syncing devices. Requires "
    "#tab-groups-on-ipad to also be enabled on iPad.";

inline constexpr char kTabResumptionImagesName[] =
    "Enable Tab Resumption images";
inline constexpr char kTabResumptionImagesDescription[] =
    "When enabled, a relevant image is displayed in Tab resumption items.";

inline constexpr char kTabResumptionName[] = "Enable Tab Resumption";
inline constexpr char kTabResumptionDescription[] =
    "When enabled, offer users with a quick shortcut to resume the last synced "
    "tab from another device.";

inline constexpr char kTabSwitcherOverflowMenuName[] =
    "Enable the Tab Switcher overflow menu";
inline constexpr char kTabSwitcherOverflowMenuDescription[] =
    "When enabled, the Tab Switcher edit button and edit menu will be replaced "
    "by a three dot button and overflow menu.";

inline constexpr char kTaiyakiAllSurfacesName[] = "Taiyaki (all surfaces)";
inline constexpr char kTaiyakiAllSurfacesDescription[] =
    "Enables Taiyaki for all surfaces (including post-FRE).";

inline constexpr char kUpdateTabGroupColorsName[] = "UpdateTabGroupColors";
inline constexpr char kUpdateTabGroupColorsDescription[] =
    "Enables the UpdateTabGroupColors feature.";

inline constexpr char kUpdatedFRESequenceName[] =
    "Update the sequence of the First Run screens";
inline constexpr char kUpdatedFRESequenceDescription[] =
    "Updates the sequence of the FRE screens to show the DB promo first, "
    "remove the Sin-In & Sync screens, or both.";

inline constexpr char kUseDefaultAppsDestinationForPromosName[] =
    "Use Default Apps page for promos";
inline constexpr char kUseDefaultAppsDestinationForPromosDescription[] =
    "When enabled, all Default Browser promos redirecting to the iOS settings "
    "will use the new Default Apps page, if the current device supports it.";

inline constexpr char kUseFeedEligibilityServiceName[] =
    "[iOS] Use the new feed eligibility service";
inline constexpr char kUseFeedEligibilityServiceDescription[] =
    "Use the new eligibility service to handle whether the Discover "
    "feed is displayed on NTP";

inline constexpr char kUseSceneViewControllerName[] =
    "Use Scene View Controller";
inline constexpr char kUseSceneViewControllerDescription[] =
    "Enables the use of SceneViewController.";

inline constexpr char kUseUIGraphicsImageRendererForFallbackIconsName[] =
    "Use UIGraphicsImageRenderer for Fallback Icons";
inline constexpr char kUseUIGraphicsImageRendererForFallbackIconsDescription[] =
    "When enabled, uses UIGraphicsImageRenderer to generate fallback icons "
    "instead of deprecated UIGraphicsGetImageFromCurrentImageContext.";

inline constexpr char kVariationsExperimentalCorpusName[] =
    "Variations experimental corpus";
inline constexpr char kVariationsExperimentalCorpusDescription[] =
    "When enabled, request the experimental variations seed from the "
    "variations server.";

inline constexpr char kVariationsRestrictDogfoodName[] =
    "Variations restrict dogfood";
inline constexpr char kVariationsRestrictDogfoodDescription[] =
    "When enabled, request dogfood variations from the variations server.";

inline constexpr char kViewCertificateInformationName[] =
    "View Certificate Information";
inline constexpr char kViewCertificateInformationDescription[] =
    "Enables viewing detailed certificate information in Page Info.";

inline constexpr char kWaitThresholdMillisecondsForCapabilitiesApiName[] =
    "Maximum wait time (in seconds) for a response from the Account "
    "Capabilities API";
inline constexpr char
    kWaitThresholdMillisecondsForCapabilitiesApiDescription[] =
        "Used for testing purposes to test waiting thresholds in dev.";

inline constexpr char kWalletApiPrivatePassesEnabledName[] =
    "Wallet API Private Passes";
inline constexpr char kWalletApiPrivatePassesEnabledDescription[] =
    "Enables the Wallet API for private passes.";

inline constexpr char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox";
inline constexpr char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

inline constexpr char kWelcomeBackName[] = "Enable Welcome Back screen";
inline constexpr char kWelcomeBackDescription[] =
    "When enabled, returning users will see the Welcome Back screen.";

inline constexpr char kYourSavedInfoSettingsPageIosName[] =
    "Enable Autofill and passwords settings redesign on iOS";
inline constexpr char kYourSavedInfoSettingsPageIosDescription[] =
    "Enables the Autofill and passwords settings page redesign on iOS.";

inline constexpr char kZeroStateSuggestionsName[] =
    "Enable Zero-State Suggestions";
inline constexpr char kZeroStateSuggestionsDescription[] =
    "Enables fetching zero-state suggestions for the 'Ask Gemini' feature,"
    "based on the current page context.";

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAddAddressManuallyName[] =
    "Enable adding an address manually from Address Settings";
const char kAddAddressManuallyDescription[] =
    "When enabled, allows users to manually enter and save an address.";

const char kAppBackgroundRefreshName[] = "Enable app background refresh";
const char kAppBackgroundRefreshDescription[] =
    "Schedules app background refresh after some minimum period of time has "
    "passed after the last refresh.";

// Title and description for the flag that enables autofill across iframes.
extern const char kAutofillAcrossIframesName[] =
    "Enables Autofill across iframes";
extern const char kAutofillAcrossIframesDescription[] =
    "When enabled, Autofill will fill and save information on forms that "
    "spread across multiple iframes.";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDisableDefaultSaveCardFixFlowDetectionName[] =
    "Disables save card fix flow values as detected by default";
const char kAutofillDisableDefaultSaveCardFixFlowDetectionDescription[] =
    "When enabled, save card fix flow values for missing cardholder "
    "name and expiry date won't be defaulted as detected.";

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

const char kAutofillEnableAllowlistForBmoCardCategoryBenefitsName[] =
    "Enable allowlist for showing category benefits for BMO cards";
const char kAutofillEnableAllowlistForBmoCardCategoryBenefitsDescription[] =
    "When enabled, card category benefits offered by BMO will be shown in "
    "Autofill suggestions on the allowlisted merchant websites.";

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

const char kAutofillEnableDynamicallyLoadingFieldsForAddressInputName[] =
    "Enable dynamically loading fields for address input";
const char kAutofillEnableDynamicallyLoadingFieldsForAddressInputDescription[] =
    "When enabled, the address fields for input would be dynamically loaded "
    "based on the country value";

const char kAutofillEnableFlatRateCardBenefitsFromCurinosName[] =
    "Enable showing flat rate card benefits sourced from Curinos";
const char kAutofillEnableFlatRateCardBenefitsFromCurinosDescription[] =
    "When enabled, flat rate card benefits sourced from Curinos will be shown "
    "in Autofill suggestions.";

const char kAutofillEnableLogFormEventsToAllParsedFormTypesName[] =
    "Enable logging form events to all parsed form on a web page.";
const char kAutofillEnableLogFormEventsToAllParsedFormTypesDescription[] =
    "When enabled, a form event will log to all of the parsed forms of the "
    "same type on a webpage. This means credit card form events will log to "
    "all credit card form types and address form events will log to all "
    "address form types.";

const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName[] =
        "Enable multiple server request support for virtual card downstream "
        "enrollment";
const char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription
        [] = "When enabled, Chrome will be able to send preflight call for "
             "enrollment earlier in the flow with the multiple server request "
             "support.";

const char kAutofillEnableFpanRiskBasedAuthenticationName[] =
    "Enable risk-based authentication for FPAN retrieval";
const char kAutofillEnableFpanRiskBasedAuthenticationDescription[] =
    "When enabled, server card retrieval will begin with a risk-based check "
    "instead of jumping straight to CVC or biometric auth.";

const char kAutofillEnablePrefetchingRiskDataForRetrievalName[] =
    "Enable prefetching of risk data during payments autofill retrieval";
const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[] =
    "When enabled, risk data is prefetched during payments autofill flows "
    "to reduce user-perceived latency.";

const char kAutofillEnableRankingFormulaAddressProfilesName[] =
    "Enable new Autofill suggestion ranking formula for address profiles";
const char kAutofillEnableRankingFormulaAddressProfilesDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "address profile suggestions.";

const char kAutofillEnableRankingFormulaCreditCardsName[] =
    "Enable new Autofill suggestion ranking formula for credit cards";
const char kAutofillEnableRankingFormulaCreditCardsDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "data model credit card suggestions.";

const char kAutofillEnableSupportForHomeAndWorkName[] =
    "Enable support for home and work addresses";
const char kAutofillEnableSupportForHomeAndWorkDescription[] =
    "When enabled, chrome will support home and work addresses from account.";

const char kAutofillIsolatedWorldForJavascriptIOSName[] =
    "Isolated content world for Autofill";
const char kAutofillIsolatedWorldForJavascriptIOSDescription[] =
    "Use the isolated content world instead of the page content world "
    "for the Autofill JS feature scripts.";

const char kAutofillPaymentsSheetV2Name[] =
    "Enable the payments suggestion bottom sheet V2";
const char kAutofillPaymentsSheetV2Description[] =
    "When enabled, the V2 of the payments suggestion bottom sheet will be "
    "used.";

const char kPasswordSuggestionBottomSheetV2Name[] =
    "Enable the password suggestion bottom sheet V2";
const char kPasswordSuggestionBottomSheetV2Description[] =
    "When enabled, the V2 of the password suggestion bottom sheet will be "
    "used.";

const char kAutofillLocalSaveCardBottomSheetName[] =
    "Enable save card bottomsheet for local save";
const char kAutofillLocalSaveCardBottomSheetDescription[] =
    "When enabled, save card bottomsheet will be shown to save the card "
    "locally when the user has not previously rejected the offer to save the "
    "card.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSaveCardBottomSheetName[] =
    "Enable save card bottomsheet for upload save";
const char kAutofillSaveCardBottomSheetDescription[] =
    "When enabled, save card bottomsheet will be shown to save the card to the "
    "server when the user has not previously rejected the offer to save the "
    "card, and both a valid expiry date and cardholder name are present.";

const char kAutofillShowManualFillForVirtualCardsName[] =
    "Show Manual Fill for Virtual Cards";
const char kAutofillShowManualFillForVirtualCardsDescription[] =
    "When enabled, Autfoill will show the manual fill view directly on form "
    "focusing events for virtual cards.";

const char kAutofillStickyInfobarName[] = "Sticky Autofill Infobar";
const char kAutofillStickyInfobarDescription[] =
    "Makes the Address Infobar sticky to only dismiss on navigation from user "
    "gesture.";

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

const char kAutofillUnmaskCardRequestTimeoutName[] =
    "Timeout for the credit card unmask request";
const char kAutofillUnmaskCardRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "unmask request. Upon timeout, the client will terminate the current "
    "unmask server call, which may or may not terminate the ongoing unmask UI.";

const char kAutofillUploadCardRequestTimeoutName[] =
    "Timeout for the credit card upload request";
const char kAutofillUploadCardRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "upload request. Upon timeout, the client will terminate the upload UI, "
    "but the request may still succeed server-side.";

const char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
const char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

const char kAutofillVcnEnrollRequestTimeoutName[] =
    "Timeout for the credit card VCN enrollment request";
const char kAutofillVcnEnrollRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "VCN enrollment request. Upon timeout, the client will terminate the VCN "
    "enrollment UI, but the request may still succeed server-side.";

const char kAutofillVcnEnrollStrikeExpiryTimeName[] =
    "Expiry duration for VCN enrollment strikes";
const char kAutofillVcnEnrollStrikeExpiryTimeDescription[] =
    "When enabled, changes the amount of time required for VCN enrollment "
    "prompt strikes to expire.";

const char kBestFeaturesScreenInFirstRunName[] =
    "Display Best Features screen in the FRE";
const char kBestFeaturesScreenInFirstRunDescription[] =
    "When enabled, displays the BestFeatures screen in the First Run sequence. "
    "Screen can be displayed either before or after the DB promo.";

const char kBestOfAppFREName[] = "Display Best of App view in the FRE";
const char kBestOfAppFREDescription[] =
    "When enabled, displays some views during the FRE highlighting the best "
    "features in the app.";

const char kBlueDotOnToolsMenuButtonName[] =
    "Show blue dot promo on tools menu button";
const char kBlueDotOnToolsMenuButtonDescription[] =
    "When enabled, blue dot promo on tools menu button will be displayed to "
    "user";

const char kBottomOmniboxDefaultSettingName[] =
    "Bottom Omnibox Default Setting";
const char kBottomOmniboxDefaultSettingDescription[] =
    "Changes the default setting of the omnibox position. If the user "
    "hasn't already changed the setting, changes the omnibox position to top "
    "or bottom of the screen on iPhone. The default is top omnibox.";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

extern const char kAppleCalendarExperienceKitName[] =
    "Experience Kit Apple Calendar";
extern const char kAppleCalendarExperienceKitDescription[] =
    "When enabled, long pressing on dates will trigger Experience Kit Apple "
    "Calendar event handling.";

const char kCollaborationMessagingName[] = "Collaboration Messaging";
const char kCollaborationMessagingDescription[] =
    "Enables the messaging framework within the collaboration feature, "
    "including features such as recent activity, dirty dots, and description "
    "action chips.";

const char kColorfulTabGroupName[] = "Colorful tab groups";
const char kColorfulTabGroupDescription[] =
    "Display the tab group colors in additional surfaces.";

const char kContainedTabGroupName[] = "Contained tab group";
const char kContainedTabGroupDescription[] =
    "When enabled the tab group in the tab grid is not presented fullscreen.";

const char kContentNotificationExperimentName[] =
    "Content Notification Experiment";
const char kContentNotificationExperimentDescription[] =
    "Enable Content Notification Experiment.";

const char kContentNotificationProvisionalIgnoreConditionsName[] =
    "Content Notification Provisional Ignore Conditions";
const char kContentNotificationProvisionalIgnoreConditionsDescription[] =
    "Enable Content Notification Provisional without Conditions";

const char kContentPushNotificationsName[] = "Content Push Notifications";
const char kContentPushNotificationsDescription[] =
    "Enables the content push notifications.";

const char kContextualPanelName[] = "Contextual Panel";
const char kContextualPanelDescription[] =
    "Enables the contextual panel feature.";

const char kCredentialProviderAutomaticPasskeyUpgradeName[] =
    "Credential Provider Automatic Passkey Upgrade";
const char kCredentialProviderAutomaticPasskeyUpgradeDescription[] =
    "Enables automatic passkey upgrade in the Credential Provider Extension.";

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

const char kChromeStartupParametersAsyncName[] =
    "Enable the async chrome startup";
const char kChromeStartupParametersAsyncDescription[] =
    "When enabled the async version of the chrome startup method is used. This "
    "method is used to parse the startup parameters.";

extern const char kPhoneNumberName[] = "Phone number experience enable";
extern const char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

const char kMeasurementsName[] = "Measurements experience enable";
const char kMeasurementsDescription[] =
    "When enabled, one tapping or long pressing on a measurement will trigger "
    "the measurement conversion experience.";

const char kEnableExpKitTextClassifierDateName[] =
    "Date with Text Classifier in Experience Kit";
const char kEnableExpKitTextClassifierDateDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "date detection on long presses.";

const char kEnableExpKitTextClassifierAddressName[] =
    "Address with Text Classifier in Experience Kit";
const char kEnableExpKitTextClassifierAddressDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "address detection on long presses.";

const char kEnableExpKitTextClassifierPhoneNumberName[] =
    "Phone Number with Text Classifier in Experience Kit";
const char kEnableExpKitTextClassifierPhoneNumberDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "phone number detection on long presses.";

const char kEnableExpKitTextClassifierEmailName[] =
    "Email with Text Classifier in Experience Kit";
const char kEnableExpKitTextClassifierEmailDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "email detection on long presses.";

const char kEnableFamilyLinkControlsName[] = "Family Link parental controls";
const char kEnableFamilyLinkControlsDescription[] =
    "Enables parental controls from Family Link on supervised accounts "
    "signed-in to Chrome.";

extern const char kOneTapForMapsName[] = "Enable one Tap Experience for Maps";
extern const char kOneTapForMapsDescription[] =
    "Enables the one tap experience for maps experience kit.";

extern const char kMagicStackName[] = "Enable Magic Stack";
extern const char kMagicStackDescription[] =
    "When enabled, a Magic Stack will be shown in the Home surface displaying "
    "a variety of modules.";

const char kCredentialProviderExtensionPromoName[] =
    "Enable the Credential Provider Extension promo.";
const char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, Credential Provider Extension promo will be "
    "presented to eligible users.";

const char kDataSharingName[] = "Data Sharing";
const char kDataSharingDescription[] =
    "Enabled Data Sharing related UI and features.";

const char kDataSharingDebugLogsName[] = "Enable data sharing debug logs";
const char kDataSharingDebugLogsDescription[] =
    "Enables the data sharing infrastructure to log and save debug messages "
    "that can be shown in the internals page.";

const char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
const char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

const char kDefaultBrowserBannerPromoName[] = "Default Browser banner promo";
const char kDefaultBrowserBannerPromoDescription[] =
    "When enabled, the default browser banner promo will show when conditions "
    "are met.";

const char kDefaultBrowserTriggerCriteriaExperimentName[] =
    "Show default browser promo trigger criteria experiment";
const char kDefaultBrowserTriggerCriteriaExperimentDescription[] =
    "When enabled, default browser promo will be displayed to user without "
    "matching all the trigger criteria.";

const char kDeprecateFeedHeaderExperimentName[] =
    "Deprecate feed header toggle experiment";
const char kDeprecateFeedHeaderExperimentDescription[] =
    "When enabled, the feed header toggle would be removed, and users will use "
    "other ways to access the following feed. New tab page elements will also "
    "be repositioned according to the variation chosen.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

extern const char kDownloadAutoDeletionName[] = "Enable Download Auto Deletion";
extern const char kDownloadAutoDeletionDescription[] =
    "When enabled, files downloaded on the device can be scheduled to be "
    "deleted automatically after 30 days.";

const char kDownloadedPDFOpeningName[] = "Enables downloaded PDF opening";
const char kDownloadedPDFOpeningDescription[] =
    "Enables the direct opening of downloaded PDF files in Chrome";

const char kEnableFeedHeaderSettingsName[] =
    "Enables the feed header settings.";
const char kEnableFeedHeaderSettingsDescription[] =
    "When enabled, some UI elements of the feed header can be modified.";

const char kDisableLensCameraName[] = "Disable Lens camera experience";
const char kDisableLensCameraDescription[] =
    "When enabled, the option use Lens to search for images from your device "
    "camera menu when Google is the selected search engine, accessible from "
    "the home screen widget, new tab page, and keyboard, is disabled.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableASWebAuthenticationSessionName[] =
    "Enable ASWebAuthenticationSession";
const char kEnableASWebAuthenticationSessionDescription[] =
    "Enables using ASWebAuthenticationSession to add Google accounts to device";

const char kEnableCompromisedPasswordsMutingName[] =
    "Enable the muting of compromised passwords in the Password Manager";
const char kEnableCompromisedPasswordsMutingDescription[] =
    "Enable the compromised password alert mutings in Password Manager to be "
    "respected in the app.";

const char kEnableDiscoverFeedDiscoFeedEndpointName[] =
    "Enable discover feed discofeed";
const char kEnableDiscoverFeedDiscoFeedEndpointDescription[] =
    "Enable using the discofeed endpoint for the discover feed.";

const char kEnableFeedAblationName[] = "Enables Feed Ablation";
const char kEnableFeedAblationDescription[] =
    "If Enabled the Feed will be removed from the NTP";

const char kEnableFeedCardMenuSignInPromoName[] =
    "Enable Feed card menu sign-in promotion";
const char kEnableFeedCardMenuSignInPromoDescription[] =
    "Display a sign-in promotion UI when signed out users click on "
    "personalization options within the feed card menu.";

const char kEnableTraitCollectionRegistrationName[] =
    "Enable Customizable Trait Registration";
const char kEnableTraitCollectionRegistrationDescription[] =
    "When enabled, UI elements will only observe and respond to the UITraits "
    "to which they have been registered.";

const char kEnableiPadFeedGhostCardsName[] = "Enable ghost cards on iPad feeds";
const char kEnableiPadFeedGhostCardsDescription[] =
    "Enables ghost cards placeholder when feed is loading on iPads.";

const char kEnableReadingListAccountStorageName[] =
    "Enable Reading List Account Storage";
const char kEnableReadingListAccountStorageDescription[] =
    "Enable the reading list account storage.";

const char kEnableReadingListSignInPromoName[] =
    "Enable Reading List Sign-in promo";
const char kEnableReadingListSignInPromoDescription[] =
    "Enable the sign-in promo view in the reading list screen.";

const char kEnableSignedOutViewDemotionName[] =
    "Enable signed out user view demotion";
const char kEnableSignedOutViewDemotionDescription[] =
    "Enable signed out user view demotion to avoid repeated content for signed "
    "out users.";

const char kEnableIdentityInAuthErrorName[] = "Enable Identities in Auth Error";
const char kEnableIdentityInAuthErrorDescription[] =
    "Enable identities in auth error state.";

const char kEnhancedCalendarName[] = "Enable Enhanced Calendar integration";
const char kEnhancedCalendarDescription[] =
    "When enabled, the enhanced calendar flow will be available to eligible "
    "users when adding a calendar event.";

const char kEnhancedSafeBrowsingPromoName[] =
    "Enable Enhanced Safe Browsing Promos";
const char kEnhancedSafeBrowsingPromoDescription[] =
    "When enabled, the Enhanced Safe Browsing inline and infobar promos are "
    "displayed given certain preconditions are met.";

const char kEnterpriseRealtimeEventReportingOnIOSName[] =
    "Enable realtime event reporting for Enterprise on iOS";
const char kEnterpriseRealtimeEventReportingOnIOSDescription[] =
    "When enabled, realtime events will be reported to the user's organization";

const char kFeedBackgroundRefreshName[] = "Enable feed background refresh";
const char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kFeedSwipeInProductHelpName[] = "Enable Feed Swipe IPH";
const char kFeedSwipeInProductHelpDescription[] =
    "Presents an in-product help on the NTP to promote swiping on the Feed";

const char kFeedbackIncludeVariationsName[] = "Feedback include variations";
const char kFeedbackIncludeVariationsDescription[] =
    "In Chrome feedback report, include commandline variations.";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kAnimatedDefaultBrowserPromoInFREName[] =
    "Enable the animated Default Browser Promo in the FRE";
const char kAnimatedDefaultBrowserPromoInFREDescription[] =
    "When enabled, the Default Browser Promo in the FRE will be animated.";

const char kFeedbackIncludeGWSVariationsName[] =
    "Include GWS variations in feedback";
const char kFeedbackIncludeGWSVariationsDescription[] =
    "Includes GWS variations in Chrome feedback reports.";

const char kFullscreenImprovementName[] = "Improve fullscreen";
const char kFullscreenImprovementDescription[] =
    "When enabled, fullscreen should have a better stability.";

const char kFullscreenPromosManagerSkipInternalLimitsName[] =
    "Fullscreen Promos Manager (Skip internal Impression Limits)";
const char kFullscreenPromosManagerSkipInternalLimitsDescription[] =
    "When enabled, the internal Impression Limits of the Promos Manager will "
    "be ignored; this is useful for local development.";

const char kFullscreenTransitionName[] = "Fullscreen Transition Tweaks";
const char kFullscreenTransitionDescription[] =
    "When enabled, the transition of fullscreen is either delayed or the speed "
    "of the transition is increased-decreased.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kHomeMemoryImprovementsName[] = "Home Memory Improvements";
const char kHomeMemoryImprovementsDescription[] =
    "When enabled, uses code that aims to improve the memory footprint in "
    "Home.";

const char kHttpsUpgradesName[] = "HTTPS Upgrades";
const char kHttpsUpgradesDescription[] =
    "When enabled, eligible navigations will automatically be upgraded to "
    "HTTPS.";

const char kIdentityDiscAccountMenuName[] = "Identity Disc Account Menu";
const char kIdentityDiscAccountMenuDescription[] =
    "When enabled, tapping the identity disc on the New Tab page shows the "
    "account menu UI.";

const char kIdentityConfirmationSnackbarName[] =
    "Identity Confirmation Snackbar";
const char kIdentityConfirmationSnackbarDescription[] =
    "When enabled, the identity confirmation snackbar will show on startup.";

const char kImportPasswordsFromSafariName[] = "Import Passwords From Safari";
const char kImportPasswordsFromSafariDescription[] =
    "When enabled, allows users to import passwords from Safari.";

const char kIndicateIdentityErrorInOverflowMenuName[] =
    "Indicate Identity Error in Overflow Menu";
const char kIndicateIdentityErrorInOverflowMenuDescription[] =
    "When enabled, the Overflow Menu indicates the identity error with an "
    "error badge on the Settings destination";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kIOSBrowserEditMenuMetricsName[] = "Browser edit menu metrics";
const char kIOSBrowserEditMenuMetricsDescription[] =
    "Collect metrics for edit menu usage.";

const char kIOSDockingPromoName[] = "Docking Promo";
const char kIOSDockingPromoDescription[] =
    "When enabled, the user will be presented an animated, instructional "
    "promo showing how to move Chrome to their native iOS dock.";

extern const char kIOSEnableDeleteAllSavedCredentialsName[] =
    "Enable delete all saved credentials in PWM";
extern const char kIOSEnableDeleteAllSavedCredentialsDescription[] =
    "When enabled, the delete all data button in PWM will be presented.";

const char kIOSEnablePasswordManagerTrustedVaultWidgetName[] =
    "Enable password settings encryption error widget";
const char kIOSEnablePasswordManagerTrustedVaultWidgetDescription[] =
    "Display a widget in the password management settings page in case of a "
    "password encryption error.";

extern const char kIOSEnableRealtimeEventReportingName[] =
    "Enable realtime event reporting on iOS";
extern const char kIOSEnableRealtimeEventReportingDescription[] =
    "When enabled, realtime events will be reported to the user's "
    "organization.";

const char kIOSKeyboardAccessoryUpgradeName[] =
    "Enable the keyboard accessory upgrade on iOS";
const char kIOSKeyboardAccessoryUpgradeDescription[] =
    "When enabled, the upgraded keyboard accessory UI will be presented.";

const char kIOSKeyboardAccessoryUpgradeForIPadName[] =
    "Enable the keyboard accessory upgrade on iOS for iPads";
const char kIOSKeyboardAccessoryUpgradeForIPadDescription[] =
    "When enabled, the upgraded keyboard accessory UI will be presented on "
    "iPads.";

const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuName[] =
    "Enable the keyboard accessory upgrade on iOS with a shorter manual fill "
    "menu";
const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuDescription[] =
    "When enabled, the upgraded keyboard accessory UI will be presented with a "
    "shorter manual fill menu.";

const char kIOSOneTapMiniMapRemoveSectionBreaksName[] =
    "Remove section break for address detection.";
const char kIOSOneTapMiniMapRemoveSectionBreaksDescription[] =
    "Replace section break by spaces when detecting addresses.";

const char kIOSOneTapMiniMapRestrictionsName[] =
    "Revalidate detected addresses for one tap Mini Map.";
const char kIOSOneTapMiniMapRestrictionsDescription[] =
    "Different restrictions to block false positive for one tap Mini Map.";

const char kIOSPasskeysM2Name[] = "Enable the passkey syncing follow-ups";
const char kIOSPasskeysM2Description[] =
    "When enabled, the passkey syncing-related features will be available in "
    "the app.";

const char kIOSChooseFromDriveName[] = "IOS Choose from Drive";
const char kIOSChooseFromDriveDescription[] =
    "Enables the Choose from Drive feature on iOS.";

const char kIOSChooseFromDriveSimulatedClickName[] =
    "IOS Choose from Drive (simulated clicks)";
const char kIOSChooseFromDriveSimulatedClickDescription[] =
    "Enables support for simulated clicks in the Choose from Drive feature on "
    "iOS.";

const char kIOSManageAccountStorageName[] = "Allow managing Account storage.";
const char kIOSManageAccountStorageDescription[] =
    "Add entry points to manage Google One account storage.";

const char kIOSSaveToPhotosImprovementsName[] =
    "IOS Save to Photos Improvements";
const char kIOSSaveToPhotosImprovementsDescription[] =
    "Enables the Save to Photos Improvements on iOS.";

const char kIOSPasswordBottomSheetAutofocusName[] =
    "IOS Password Manager Bottom Sheet Autofocus";
const char kIOSPasswordBottomSheetAutofocusDescription[] =
    "Enables triggering the password bottom sheet on autofocus on IOS.";

const char kIOSProactivePasswordGenerationBottomSheetName[] =
    "IOS Proactive Password Generation Bottom Sheet";
const char kIOSProactivePasswordGenerationBottomSheetDescription[] =
    "Enables the display of the proactive password generation bottom sheet on "
    "IOS.";

const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[] =
    "Invalidate search engine choice after device restore";
const char kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[] =
    "When enabled, search engine choices made before backup & restore will not "
    "be considered valid on the restored device, leading to the choice screen "
    "potentially retriggering.";

const char kIOSQuickDeleteName[] = "Quick Delete for iOS";
const char kIOSQuickDeleteDescription[] =
    "Enables a new way for users to more easily delete their browsing data in "
    "iOS.";

const char kIOSEnterpriseRealtimeUrlFilteringName[] =
    "Enable Enterprise Url Filtering for iOS";
const char kIOSEnterpriseRealtimeUrlFilteringDescription[] =
    "When enabled, Enterprise admins can block navigations to urls matching "
    "rules defined by their organization.";

const char kNewShareExtensionName[] = "New Share Extension for iOS";
const char kNewShareExtensionDescription[] =
    "Update the share extension UI and add new share entries";

const char kNewTabPageFieldTrialName[] =
    "New tab page features that target new users";
const char kNewTabPageFieldTrialDescription[] =
    "Enables new tab page features that are available on first run for new "
    "Chrome iOS users.";

const char kNonModalSignInPromoName[] = "Non-modal sign-in promo";
const char kNonModalSignInPromoDescription[] =
    "Enables a non-modal sign-in promo that prompts users to sign in.";

const char kNotificationCollisionManagementName[] =
    "Notification collision management";
const char kNotificationCollisionManagementDescription[] =
    "Enables delays to notifications to space them out more";

const char kIOSSharedHighlightingColorChangeName[] =
    "IOS Shared Highlighting color change";
const char kIOSSharedHighlightingColorChangeDescription[] =
    "Changes the Shared Highlighting color of the text fragment"
    "away from the default yellow in iOS. Works with #scroll-to-text-ios flag.";

const char kIOSSharedHighlightingAmpName[] = "Shared Highlighting on AMP pages";
const char kIOSSharedHighlightingAmpDescription[] =
    "Enables the Create Link option on AMP pages.";

const char kIOSSoftLockName[] = "Soft Lock on iOS";
const char kIOSSoftLockDescription[] = "Enables experimental Soft Lock on iOS.";

const char kIOSStartTimeBrowserBackgroundRemediationsName[] =
    "Browser Background Termination remediations for the Bling Start 4 hour "
    "reduction";
const char kIOSStartTimeBrowserBackgroundRemediationsDescription[] =
    "Enables potential remediations for Browser Background Termination "
    "regressions caused by the reduction of Bling Start time from 6 hours to "
    "4.";

const char kIOSStartTimeStartupRemediationsName[] =
    "Startup remediations for the Bling Start 4 hour reduction";
const char kIOSStartTimeStartupRemediationsDescription[] =
    "Enables potential remediations for startup regressions caused by the "
    "reduction of Bling Start time from 6 hours to 4.";

const char kIOSReactivationNotificationsName[] = "Reactivation Notifications";
const char kIOSReactivationNotificationsDescription[] =
    "Enables a feature to send provisional notifications of interest to new"
    "users and encourage them to return to the app.";

const char kIOSExpandedTipsName[] = "Expanded Tips Notifications";
const char kIOSExpandedTipsDescription[] =
    "Enables a feature that adds several new Tips Notifications that can be "
    "sent.";

const char kIOSProvidesAppNotificationSettingsName[] =
    "IOS Provides App Notification Settings";
const char kIOSProvidesAppNotificationSettingsDescription[] =
    "Enabled integration with iOS's ProvidesAppNotificationSettings feature.";

extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName[] =
        "Lens blocks fetch objects interaction RPCs on separate handshake";
extern const char
    kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription[] =
        "When enabled, RPCs are blocked on separate handshake.";

const char kLensClearcutBackgroundUploadEnabledName[] =
    "Lens clearcut background upload";
const char kLensClearcutBackgroundUploadEnabledDescription[] =
    "Enables uploading of clearcut logs in the background.";

const char kLensClearcutLoggerFastQosEnabledName[] =
    "Lens clearcut logger fast QOS";
const char kLensClearcutLoggerFastQosEnabledDescription[] = "Enables fast QOS.";

const char kLensExactMatchesEnabledName[] = "Lens exact matches enabled";
const char kLensExactMatchesEnabledDescription[] =
    "Enables exact matches in the Lens results.";

const char kLensFetchSrpApiEnabledName[] = "Lens fetch SRP API enabled";
const char kLensFetchSrpApiEnabledDescription[] = "Enables the fetch SRP API.";

const char kLensFiltersAblationModeEnabledName[] =
    "Lens filters ablation mode enabled";
const char kLensFiltersAblationModeEnabledDescription[] =
    "Enables the filters ablation mode.";

const char kLensGestureTextSelectionDisabledName[] =
    "Disable Lens gesture text selection";
const char kLensGestureTextSelectionDisabledDescription[] =
    "When disabled, turns off gesture text selection.";

const char kLensInkMultiSampleModeDisabledName[] =
    "Disable Lens Ink multi-sample mode";
const char kLensInkMultiSampleModeDisabledDescription[] =
    "When disabled, turns off multi-sample mode and uses less memory.";

const char kLensLoadAIMInLensResultPageName[] =
    "Enable loading AIM in the Lens result page";
const char kLensLoadAIMInLensResultPageDescription[] =
    "Opens in Lens result page rather than a new tab.";

extern const char kLensOverlayAlternativeOnboardingName[] =
    "Lens Overlay Onboarding";
extern const char kLensOverlayAlternativeOnboardingDescription[] =
    "Selects which lens overlay onboarding/entrypoint treatment is active. "
    "No-op if lens overlay is off.";

extern const char kLensOverlayDisableIPHPanGestureName[] =
    "Disable Lens Overlay IPH Pan Dismissal";
extern const char kLensOverlayDisableIPHPanGestureDescription[] =
    "Disable the pan gesture that dismisses Lens Overlay IPH. The IPH can "
    "still be dismissed with a tap.";

extern const char kLensOverlayDisablePriceInsightsName[] =
    "Allow Lens overlay to disable price insights";
extern const char kLensOverlayDisablePriceInsightsDescription[] =
    "When enabled, price insights is disabled. The price insight entrypoint "
    "trumps lens overlay entrypoint in the location bar. This should only be "
    "used for experiments.";

extern const char kLensOverlayPriceInsightsCounterfactualName[] =
    "Lens overlay disable price insights counterfactual.";
extern const char kLensOverlayPriceInsightsCounterfactualDescription[] =
    "When enabled, show the lens overlay location bar entrypoint only when "
    "price insights should have triggered.";

extern const char kLensOverlayEnableIPadCompatibilityName[] =
    "Allow Lens overlay to also run on iPad devices if the feature is enabled";
extern const char kLensOverlayEnableIPadCompatibilityDescription[] =
    "When enabled, it allows Lens Overlay to run on iPad devices";

extern const char kLensOverlayEnableLandscapeCompatibilityName[] =
    "Allow Lens overlay to also run in landscape if the feature is enabled";
extern const char kLensOverlayEnableLandscapeCompatibilityDescription[] =
    "When enabled, it allows Lens Overlay to run in landscape orientation";

extern const char kLensOverlayEnableLVFEscapeHatchName[] =
    "Escape hatch to LVF in the overflow menu in Lens Overlay";
extern const char kLensOverlayEnableLVFEscapeHatchDescription[] =
    "When enabled, the escape hatch to LVF is presented in the overflow menu";

extern const char kLensOverlayEnableLocationBarEntrypointName[] =
    "Enable Lens overlay location bar entrypoint.";
extern const char kLensOverlayEnableLocationBarEntrypointDescription[] =
    "When enabled, shows the Lens overlay entrypoint in the location bar when "
    "no other buttons are shown (price insight or messages). Enabled by "
    "default. ";

extern const char kLensOverlayEnableLocationBarEntrypointOnSRPName[] =
    "Enable Lens overlay location bar entrypoint on SRP.";
extern const char kLensOverlayEnableLocationBarEntrypointOnSRPDescription[] =
    "When enabled, the location bar entrypoint is available on SRP. Enabled by "
    "default.";

extern const char kLensOverlayEnableSameTabNavigationName[] =
    "Lens overlay same tab navigation";
extern const char kLensOverlayEnableSameTabNavigationDescription[] =
    "When enabled, lens overlay navigations are opened in the same tab instead "
    "of a new tab.";

extern const char kLensOverlayForceShowOnboardingScreenName[] =
    "Force show Lens overlay onboarding screen";
extern const char kLensOverlayForceShowOnboardingScreenDescription[] =
    "When enabled, it forces showing the onboarding screen everytime lens "
    "overlay is open";

extern const char kLensOverlayNavigationHistoryName[] =
    "Enable Lens overlay navigation history";
extern const char kLensOverlayNavigationHistoryDescription[] =
    "When enabled, web navigation in the Lens overlay are recorded in browser "
    "history.";

extern const char kLensPrewarmHardStickinessInInputSelectionName[] =
    "Lens prewarm hard stickiness in input selection";
extern const char kLensPrewarmHardStickinessInInputSelectionDescription[] =
    "When enabled, input selection prewarms hard stickiness.";

extern const char kLensPrewarmHardStickinessInQueryFormulationName[] =
    "Lens prewarm hard stickiness in query formulation";
extern const char kLensPrewarmHardStickinessInQueryFormulationDescription[] =
    "When enabled, query formulation prewarms hard stickiness.";

extern const char kLensQRCodeParsingFixName[] =
    "Enables the Lens QR code parding fix";
extern const char kLensQRCodeParsingFixDescription[] =
    "When enabled, properly parses QR codes.";

extern const char kLensSingleTapTextSelectionDisabledName[] =
    "Disable Lens single tap text selection";
extern const char kLensSingleTapTextSelectionDisabledDescription[] =
    "When disabled, single taps do not trigger text selections.";

const char kLensTranslateToggleModeEnabledName[] =
    "Lens translate toggle mode enabled";
const char kLensTranslateToggleModeEnabledDescription[] =
    "Enables the translate toggle mode.";

const char kLensUnaryApisWithHttpTransportEnabledName[] =
    "Lens unary APIs with HTTP transport enabled";
const char kLensUnaryApisWithHttpTransportEnabledDescription[] =
    "Enables the unary APIs with HTTP transport.";

const char kLensUnaryApiSalientTextEnabledName[] =
    "Lens unary API salient text enabled";
const char kLensUnaryApiSalientTextEnabledDescription[] =
    "Enables the unary salient text API.";

const char kLensUnaryClientDataHeaderEnabledName[] =
    "Lens unary client data header enabled";
const char kLensUnaryClientDataHeaderEnabledDescription[] =
    "Enables the client data header for unary request.";

const char kLensUnaryHttpTransportEnabledName[] =
    "Lens unary HTTP transport enabled";
const char kLensUnaryHttpTransportEnabledDescription[] =
    "Enables the HTTP transport for unary requests.";

const char kLensVsintParamEnabledName[] = "Lens vsint param enabled";
const char kLensVsintParamEnabledDescription[] =
    "Enables the vsint param for requests.";

const char kLensWebPageLoadOptimizationEnabledName[] =
    "Lens web page load optimization";
const char kLensWebPageLoadOptimizationEnabledDescription[] =
    "Enables optmized loading for the Lens web page.";

const char kLinkedServicesSettingIosName[] = "Linked Services Setting";
const char kLinkedServicesSettingIosDescription[] =
    "Add Linked Services Setting to the Sync Settings page.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kManualLogUploadsInFREName[] = "Manual log uploads in the FRE";
const char kManualLogUploadsInFREDescription[] =
    "Enables triggering an UMA log upload after each FRE screen.";

const char kMetrickitNonCrashReportName[] = "Metrickit non-crash reports";
const char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

const char kMostVisitedTilesHorizontalRenderGroupName[] =
    "MVTiles Horizontal Render Group";
const char kMostVisitedTilesHorizontalRenderGroupDescription[] =
    "When enabled, the MV tiles are represented as individual matches";

const char kNativeFindInPageName[] = "Native Find in Page";
const char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

const char kNTPViewHierarchyRepairName[] = "NTP View Hierarchy Repair";
const char kNTPViewHierarchyRepairDescription[] =
    "Checks if NTP view hierarchy is broken and fixes it if necessary.";

const char kOmniboxGroupingFrameworkForZPSName[] =
    "Omnibox Grouping Framework for ZPS";
const char kOmniboxGroupingFrameworkForZPSDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxGroupingFrameworkForTypedSuggestionsName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[] =
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

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxMaxZPSMatchesName[] = "Omnibox Max ZPS Matches";
const char kOmniboxMaxZPSMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "zero-prefix state in the omnibox (e.g. on NTP when tapped on OB).";

const char kOmniboxMiaZps[] = "Omnibox Mia ZPS on NTP";
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

const char kOmniboxMobileParityUpdateName[] = "Omnibox Mobile parity update";
const char kOmniboxMobileParityUpdateDescription[] =
    "When set, applies certain assets to match Desktop visuals and "
    "descriptions";

const char kOmniboxMlUrlScoreCachingName[] = "Omnibox ML URL Score Caching";
const char kOmniboxMlUrlScoreCachingDescription[] =
    "Enables in-memory caching of ML URL scores.";

const char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
const char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

const char kOmniboxMlUrlScoringModelName[] = "Omnibox URL Scoring Model";
const char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL sugestions.";

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

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "Limit the number of URL suggestions in the omnibox. The omnibox will "
    "still display more than MaxURLMatches if there are no non-URL suggestions "
    "to replace them.";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

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

const char kOmniboxZeroSuggestInMemoryCachingName[] =
    "Omnibox Zero Prefix Suggestion in-memory caching";
const char kOmniboxZeroSuggestInMemoryCachingDescription[] =
    "Enables in-memory caching of zero prefix suggestions.";

const char kOmniboxZeroSuggestPrefetchingName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on NTP";
const char kOmniboxZeroSuggestPrefetchingDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the New Tab page.";

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

const char kOnlyAccessClipboardAsyncName[] =
    "Only access the clipboard asynchronously";
const char kOnlyAccessClipboardAsyncDescription[] =
    "Only accesses the clipboard asynchronously.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kOptimizationGuidePushNotificationClientName[] =
    "Enable optimization guide push notification client";
const char kOptimizationGuidePushNotificationClientDescription[] =
    "Enables the client that handles incoming push notifications on behalf of "
    "the optimization guide.";

const char kPageActionMenuName[] = "Page Action Menu";
const char kPageActionMenuDescription[] =
    "When enabled, the entry point for the Page Action Menu becomes available "
    "for actions relating to the web page.";

const char kGLICPromoConsentName[] = "GLIC Promo Consent";
const char kGLICPromoConsentDescription[] =
    "Whether the promo consent flow is composed of a single or a double screen "
    "view.";

const char kPageContentAnnotationsName[] = "Page content annotations";
const char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

const char kPageImageServiceSalientImageName[] =
    "Page Image Service - Optimization Guide Salient Images";
extern const char kPageImageServiceSalientImageDescription[] =
    "Enables the PageImageService fetching images from the Optimization Guide "
    "Salient Images source.";

const char kPageInfoLastVisitedIOSName[] = "Last Visited in Page Info for iOS";
const char kPageInfoLastVisitedIOSDescription[] =
    "Shows the Last Visited row in Page Info for iOS.";

const char kPageContentAnnotationsPersistSalientImageMetadataName[] =
    "Page content annotations - Persist salient image metadata";
const char kPageContentAnnotationsPersistSalientImageMetadataDescription[] =
    "Enables salient image metadata per page load to be persisted on-device.";

const char kPageContentAnnotationsRemotePageMetadataName[] =
    "Page content annotations - Remote page metadata";
const char kPageContentAnnotationsRemotePageMetadataDescription[] =
    "Enables fetching of page load metadata to be persisted on-device.";

const char kPageVisibilityPageContentAnnotationsName[] =
    "Page visibility content annotations";
const char kPageVisibilityPageContentAnnotationsDescription[] =
    "Enables annotating the page visibility model for each page load "
    "on-device.";

const char kPasswordFormClientsideClassifierName[] =
    "Clientside password form classifier.";
const char kPasswordFormClientsideClassifierDescription[] =
    "Enable usage of new password form classifier on the client.";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kPasswordSharingName[] = "Enables password sharing";
const char kPasswordSharingDescription[] =
    "Enables password sharing between members of the same family.";

const char kPriceTrackingPromoName[] =
    "Enables price tracking notification promo card";
const char kPriceTrackingPromoDescription[] =
    "Enables being able to show the card in the Magic Stack";

const char kPrivacyGuideIosName[] = "Privacy Guide on iOS";
const char kPrivacyGuideIosDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

const char kProvisionalNotificationAlertName[] =
    "Provisional notifiation alert on iOS";
const char kProvisionalNotificationAlertDescription[] =
    "Shows an alert to the user when app notification settings are changed but "
    "only provisonal notifications are enabled";

const char kIpadZpsSuggestionMatchesLimitName[] = "Ipad Zps Suggestions limit";
const char kIpadZpsSuggestionMatchesLimitDescription[] =
    "Change the zps suggestion limit";

const char kIPHPriceNotificationsWhileBrowsingName[] =
    "Price Tracking IPH Display";
const char kIPHPriceNotificationsWhileBrowsingDescription[] =
    "Displays the Price Tracking IPH when the user navigates to a "
    "product "
    "webpage that supports price tracking.";

const char kNotificationSettingsMenuItemName[] =
    "Notification Settings Menu Item";
const char kNotificationSettingsMenuItemDescription[] =
    "Displays the menu item for the notification controls inside the chrome "
    "settings UI.";

const char kReaderModeName[] = "Enables Reader Mode";
const char kReaderModeDescription[] =
    "Enables Reader Mode UI and entry points.";

const char kReaderModeDebugInfoName[] = "Enables Reader Mode Debugging";
const char kReaderModeDebugInfoDescription[] =
    "Enables additional debug information for the Reader Mode feature such as "
    "latency metrics.";

const char kReaderModePageEligibilityHeuristicName[] =
    "Enables Reader Mode page eligibility heuristic";
const char kReaderModePageEligibilityHeuristicDescription[] =
    "Enables Reader Mode heuristic to hide/show the tools menu entrypoint "
    "depending on page eligibility.";

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

extern const char kSafeBrowsingTrustedURLName[] =
    "Enable the Trusted URL for Safe Browsing";
extern const char kSafeBrowsingTrustedURLDescription[] =
    "When enabled, chrome://safe-browsing will be accessible.";

const char kSafetyCheckMagicStackName[] = "Enable Safety Check (Magic Stack)";
const char kSafetyCheckMagicStackDescription[] =
    "When enabled, the Safety Check module will be displayed in the Magic "
    "Stack.";

const char kSafetyCheckNotificationsName[] =
    "Enable Safety Check Push Notifications";
const char kSafetyCheckNotificationsDescription[] =
    "Enables push notifications for important Safety Check findings.";

const char kScreenTimeIntegrationName[] = "Enables ScreenTime Integration";
const char kScreenTimeIntegrationDescription[] =
    "Enables integration with ScreenTime in iOS 14.0 and above.";

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

const char kSegmentationPlatformTipsEphemeralCardName[] =
    "Enable Tips (Magic Stack)";
const char kSegmentationPlatformTipsEphemeralCardDescription[] =
    "When enabled, the Tips module will be displayed in the Magic Stack.";

const char kSegmentedDefaultBrowserPromoName[] =
    "Enable Personalized Messaging in Default Browser Promos";
const char kSegmentedDefaultBrowserPromoDescription[] =
    "Enables Default Browser promos with personalized messaging (using "
    "Segmentation Platform).";

const char kSendTabToSelfIOSPushNotificationsName[] =
    "Send tab to self iOS push notifications";
const char kSendTabToSelfIOSPushNotificationsDescription[] =
    "Feature to allow users to send tabs to their iOS device through a system "
    "push notitification.";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSeparateProfilesForManagedAccountsName[] =
    "Put each managed account into its own profile";
const char kSeparateProfilesForManagedAccountsDescription[] =
    "If enabled, each managed account will be assigned to its own separate "
    "profile.";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

const char kSetUpListShortenedDurationName[] = "Set Up List Shortened Duration";
const char kSetUpListShortenedDurationDescription[] =
    "Reduces the Set Up List duration in the NTP to the selected parameter.";

const char kSetUpListWithoutSignInItemName[] =
    "Set Up List without sign-in item";
const char kSetUpListWithoutSignInItemDescription[] =
    "Removes the sign-in item from the Set Up List.";

const char kShareInWebContextMenuIOSName[] = "Share in web context menu";
const char kShareInWebContextMenuIOSDescription[] =
    "Enables the Share button in the web context menu in iOS 16.0 and above.";

const char kShopCardName[] = "Enables ShopCard";
const char kShopCardDescription[] =
    "Enables being able to show ShopCard in the Magic Stack";

const char kShopCardImpressionLimitsName[] =
    "Enables ShopCard Impression limits";
const char kShopCardImpressionLimitsDescription[] =
    "Limits the number of times ShopCards can be shown in the Magic Stack";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSignInButtonNoAvatarName[] =
    "Display sign-in button without avatar";
const char kSignInButtonNoAvatarDescription[] =
    "When enabled, the sign-in button is shown without an avatar on the NTP.";

const char kNTPBackgroundCustomizationName[] =
    "Enable background customization menu on the NTP";
const char kNTPBackgroundCustomizationDescription[] =
    "When enabled, the background customization menu is available on the NTP.";

const char kNtpAlphaBackgroundCollectionsName[] =
    "Enable alpha background collections";
const char kNtpAlphaBackgroundCollectionsDescription[] =
    "When enabled, the alpha background collections are available on the NTP.";

const char kSpotlightNeverRetainIndexName[] = "Don't retain spotlight index";
const char kSpotlightNeverRetainIndexDescription[] =
    "Tentative spotlight memory improvement by not storing a strong pointer to "
    "the spotlight default index";

const char kSuggestStrongPasswordInAddPasswordName[] =
    "Suggest Strong password in add password page";
const char kSuggestStrongPasswordInAddPasswordDescription[] =
    "Add field suggest strong password in add password page if the user is "
    "signed in and syncing passwords to their Google Account.";

const char kSupervisedUserBlockInterstitialV3Name[] =
    "Enable URL filter interstitial V3";
const char kSupervisedUserBlockInterstitialV3Description[] =
    "Enables URL filter interstitial V3 for Family Link users.";

const char kSupervisedUserLocalWebApprovalsName[] =
    "Enable local web approvals feature";
const char kSupervisedUserLocalWebApprovalsDescription[] =
    "Enables parents to approve blocked websites on a child's device.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncTrustedVaultInfobarImprovementsName[] =
    "Trusted vault infobar UI improvements";
const char kSyncTrustedVaultInfobarImprovementsDescription[] =
    "Enabled improvements to the UI of the trusted vault error infobar (e.g. "
    "displaying it on pages with password forms).";

const char kSyncTrustedVaultInfobarMessageImprovementsName[] =
    "Trusted vault infobar message improvements";
const char kSyncTrustedVaultInfobarMessageImprovementsDescription[] =
    "Enables massage improvements for the UI of the trusted vault error "
    "infobar.";

const char kTabGroupIndicatorName[] = "Tab Group Indicator";
const char kTabGroupIndicatorDescription[] =
    "When enabled, displays a tab group indicator next to the omnibox.";

const char kTabGroupSyncName[] = "Enable Tab Group Sync";
const char kTabGroupSyncDescription[] =
    "When enabled, tab groups are synced between syncing devices. Requires "
    "#tab-groups-on-ipad to also be enabled on iPad.";

const char kStartSurfaceName[] = "Start Surface";
const char kStartSurfaceDescription[] =
    "Enable showing the Start Surface when launching Chrome via clicking the "
    "icon or the app switcher.";

const char kDownloadServiceForegroundSessionName[] =
    "Download service foreground download";
const char kDownloadServiceForegroundSessionDescription[] =
    "Enable download service to download in app foreground only";

const char kTFLiteLanguageDetectionName[] = "TFLite-based Language Detection";
const char kTFLiteLanguageDetectionDescription[] =
    "Uses TFLite for language detection in place of CLD3";

const char kThemeColorInTopToolbarName[] = "Top toolbar use page's theme color";
const char kThemeColorInTopToolbarDescription[] =
    "When enabled with bottom omnibox, the top toolbar background color is the "
    "page's theme color. Disabled when a dynamic color flag is enabled.";

const char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
const char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

const char kEnableLensOverlayName[] = "Enable Lens Overlay";
const char kEnableLensOverlayDescription[] = "Enables lens overlay UI";

const char kEnableLensViewFinderUnifiedExperienceName[] =
    "Enable LVF Unified Experience";
const char kEnableLensViewFinderUnifiedExperienceDescription[] =
    "Enables Lens View Finder unified experience";

const char kEnableLensContextMenuUnifiedExperienceName[] =
    "Enable Lens Context Menu Unified Experience";
const char kEnableLensContextMenuUnifiedExperienceDescription[] =
    "Enables unified native experience for Lens Context Menu";

const char kTabGridNewTransitionsName[] = "Enable new TabGrid transitions";
const char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

const char kTabResumptionName[] = "Enable Tab Resumption";
const char kTabResumptionDescription[] =
    "When enabled, offer users with a quick shortcut to resume the last synced "
    "tab from another device.";

const char kTabResumptionImagesName[] = "Enable Tab Resumption images";
const char kTabResumptionImagesDescription[] =
    "When enabled, a relevant image is displayed in Tab resumption items.";

const char kUpdatedFRESequenceName[] =
    "Update the sequence of the First Run screens";
const char kUpdatedFRESequenceDescription[] =
    "Updates the sequence of the FRE screens to show the DB promo first, "
    "remove the Sin-In & Sync screens, or both.";

const char kUseFeedEligibilityServiceName[] =
    "[iOS] Use the new feed eligibility service";
const char kUseFeedEligibilityServiceDescription[] =
    "Use the new eligibility service to handle whether the Discover "
    "feed is displayed on NTP";

const char kWaitThresholdMillisecondsForCapabilitiesApiName[] =
    "Maximum wait time (in seconds) for a response from the Account "
    "Capabilities API";
const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[] =
    "Used for testing purposes to test waiting thresholds in dev.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWebFeedFeedbackRerouteName[] =
    "Send discover feed feedback to a updated destination";
const char kWebFeedFeedbackRerouteDescription[] =
    "Directs discover feed feedback to a new target for better handling of the"
    "feedback reports.";

const char kWebPageDefaultZoomFromDynamicTypeName[] =
    "Use dynamic type size for default text zoom level";
const char kWebPageDefaultZoomFromDynamicTypeDescription[] =
    "When enabled, the default text zoom level for a website comes from the "
    "current dynamic type setting.";

const char kWebPageAlternativeTextZoomName[] =
    "Use different method for zooming web pages";
const char kWebPageAlternativeTextZoomDescription[] =
    "When enabled, switches the method used to zoom web pages.";

const char kWebPageTextZoomIPadName[] = "Enable text zoom on iPad";
const char kWebPageTextZoomIPadDescription[] =
    "When enabled, text zoom works again on iPad";

extern const char kWelcomeBackInFirstRunName[] = "Enable Welcome Back screen";
extern const char kWelcomeBackInFirstRunDescription[] =
    "When enabled, returning users will see the Welcome Back screen after the "
    "First Run sequence.";

extern const char kWidgetsForMultiprofileName[] =
    "Enable Widgets for multiprofile";
extern const char kWidgetsForMultiprofileDescription[] =
    "When enabled, returning users will see the new per-account widget "
    "implementation";

const char kYoutubeIncognitoName[] =
    "Enable the opening of links from Youtube incognito in Chrome incognito";
const char kYoutubeIncognitoDescription[] =
    "When enabled, the links from Youtube incognito will be opened in Chrome "
    "incognito.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

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

const char kAutofillEnableCardArtImageName[] = "Enable showing card art images";
const char kAutofillEnableCardArtImageDescription[] =
    "When enabled, card product images (instead of network icons) will be "
    "shown in Payments Autofill UI.";

const char kAutofillEnableCardBenefitsForAmericanExpressName[] =
    "Enable showing American Express card benefits";
const char kAutofillEnableCardBenefitsForAmericanExpressDescription[] =
    "When enabled, card benefits offered by American Express will be shown in "
    "Autofill suggestions.";

const char kAutofillEnableCardBenefitsForCapitalOneName[] =
    "Enable showing Capital One card benefits";
const char kAutofillEnableCardBenefitsForCapitalOneDescription[] =
    "When enabled, card benefits offered by Capital One will be shown in "
    "Autofill suggestions.";

const char kAutofillEnableCardBenefitsSyncName[] =
    "Enable syncing card benefits from the server";
const char kAutofillEnableCardBenefitsSyncDescription[] =
    "When enabled, card benefits offered by issuers will be synced from "
    "the Payments server.";

const char kAutofillEnableDynamicallyLoadingFieldsForAddressInputName[] =
    "Enable dynamically loading fields for address input";
const char kAutofillEnableDynamicallyLoadingFieldsForAddressInputDescription[] =
    "When enabled, the address fields for input would be dynamically loaded "
    "based on the country value";

const char kAutofillEnableLogFormEventsToAllParsedFormTypesName[] =
    "Enable logging form events to all parsed form on a web page.";
const char kAutofillEnableLogFormEventsToAllParsedFormTypesDescription[] =
    "When enabled, a form event will log to all of the parsed forms of the "
    "same type on a webpage. This means credit card form events will log to "
    "all credit card form types and address form events will log to all "
    "address form types.";

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

const char kAutofillEnableSaveCardLoadingAndConfirmationName[] =
    "Enable save card loading and confirmation UX";
const char kAutofillEnableSaveCardLoadingAndConfirmationDescription[] =
    "When enabled, a loading spinner will be shown when uploading a card to "
    "the server and a confirmation screen will be will be shown based on the "
    "result of the upload. If the upload is unsuccessful in being uploaded to "
    "the server, it will be saved locally.";

const char kAutofillEnableSaveCardLocalSaveFallbackName[] =
    "Enable save card local save fallback";
const char kAutofillEnableSaveCardLocalSaveFallbackDescription[] =
    "When enabled, if a card fails to be uploaded to the server, the card "
    "details will be saved locally instead. If a card with the same card "
    "number and expiration date already exists in the local database, this "
    "will be a no-op and the existing card will not be updated with any card "
    "details from the form.";

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments UI.";

const char kAutofillEnableVcnEnrollLoadingAndConfirmationName[] =
    "Enable showing loading and confirmation screens for virtual card "
    "enrollment";
const char kAutofillEnableVcnEnrollLoadingAndConfirmationDescription[] =
    "When enabled, the virtual card enrollment screen will present a loading "
    "spinner while enrolling the card to the server and present a confirmation "
    "screen with the result when completed.";

const char kAutofillEnableVerveCardSupportName[] =
    "Enable autofill support for Verve cards";
const char kAutofillEnableVerveCardSupportDescription[] =
    "When enabled, Verve-branded card art will be shown for Verve cards.";

const char kAutofillIsolatedWorldForJavascriptIOSName[] =
    "Isolated content world for Autofill";
const char kAutofillIsolatedWorldForJavascriptIOSDescription[] =
    "Use the isolated content world instead of the page content world "
    "for the Autofill JS feature scripts.";

const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[] =
    "Parse standalone CVC fields for VCN card on file in forms";
const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[] =
    "When enabled, Autofill will attempt to find standalone CVC fields for VCN "
    "card on file when parsing forms.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillShowManualFillForVirtualCardsName[] =
    "Show Manual Fill for Virtual Cards";
const char kAutofillShowManualFillForVirtualCardsDescription[] =
    "When enabled, Autfoill will show the manual fill view directly on form "
    "focusing events for virtual cards.";

const char kAutofillStickyInfobarName[] = "Sticky Autofill Infobar";
const char kAutofillStickyInfobarDescription[] =
    "Makes the Address Infobar sticky to only dismiss on navigation from user "
    "gesture.";

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

const char kClearDeviceDataOnSignOutForManagedUsersName[] =
    "Clear Device Data on Signout for Managed Users";
const char kClearDeviceDataOnSignOutForManagedUsersDescription[] =
    "Enables clearing data saved on the device for managed users on signout.";

const char kClearUndecryptablePasswordsName[] =
    "Removes passwords that can no longer be decrypted";
const char kClearUndecryptablePasswordsDescription[] =
    "If enabled local passwords that current encyrption key cannot decrypt, "
    "will be deleted to restore the full functionality of password manager.";

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

extern const char kContextualPanelForceShowEntrypointName[] =
    "Force show Contextual Panel entrypoint";
extern const char kContextualPanelForceShowEntrypointDescription[] =
    "When enabled, the Contextual Panel entrypoint will be shown regardless of "
    "appearance prerequisites.";

const char kContextualPanelName[] = "Contextual Panel";
const char kContextualPanelDescription[] =
    "Enables the contextual panel feature.";

const char kCredentialProviderPerformanceImprovementsName[] =
    "Credential Provider Performance Improvements";
const char kCredentialProviderPerformanceImprovementsDescription[] =
    "Enables a series of performance improvements for the Credential Provider "
    "Extension.";

extern const char kPhoneNumberName[] = "Phone number experience enable";
extern const char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

const char kMeasurementsName[] = "Measurements experience enable";
const char kMeasurementsDescription[] =
    "When enabled, one tapping or long pressing on a measurement will trigger "
    "the measurement conversion experience.";

const char kEnableViewportIntentsName[] = "Viewport intent detection";
const char kEnableViewportIntentsDescription[] =
    "When enabled the intents are detected live as the viewport is moved "
    "around.";

const char kEnableNewParcelTrackingNumberDetectionName[] =
    "Improve Tracking Number Detection";
const char kEnableNewParcelTrackingNumberDetectionDescription[] =
    "When enabled carrier names are parsed out and must match tracking "
    "numbers.";

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

const char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
const char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

const char kDefaultBrowserIntentsShowSettingsName[] =
    "Default Browser Intents show settings";
const char kDefaultBrowserIntentsShowSettingsDescription[] =
    "When enabled, external apps can trigger the settings screen showing "
    "default browser tutorial.";

const char kDefaultBrowserPromoIPadExperimentalStringName[] =
    "Enable experimental strings for default browser promo on iPad";
const char kDefaultBrowserPromoIPadExperimentalStringDescription[] =
    "When enabled, the title and subtitle for default browser promo will be "
    "tailored towards iPad users. Available on iPad only.";

const char kDefaultBrowserTriggerCriteriaExperimentName[] =
    "Show default browser promo trigger criteria experiment";
const char kDefaultBrowserTriggerCriteriaExperimentDescription[] =
    "When enabled, default browser promo will be displayed to user without "
    "matching all the trigger criteria.";

const char kBlueDotOnToolsMenuButtonName[] =
    "Show blue dot promo on tools menu button";
const char kBlueDotOnToolsMenuButtonDescription[] =
    "When enabled, blue dot promo on tools menu button will be displayed to "
    "user";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDisableFullscreenScrollingName[] = "Disable fullscreen scrolling";
const char kDisableFullscreenScrollingDescription[] =
    "When this flag is enabled and a user scroll a web page, toolbars will "
    "stay extanded and the user will not enter in fullscreen mode.";

const char kEnableColorLensAndVoiceIconsInHomeScreenWidgetName[] =
    "Enable color Lens and voice icons in home screen widget.";
const char kEnableColorLensAndVoiceIconsInHomeScreenWidgetDescription[] =
    "Shows the color icons, rather than the monochrome icons.";

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

const char kEnableFollowIPHExpParamsName[] =
    "Enable Follow IPH Experiment Parameters";
const char kEnableFollowIPHExpParamsDescription[] =
    "Enable follow IPH experiment parameters.";

const char kEnableFollowUIUpdateName[] = "Enable the Follow UI Update";
const char kEnableFollowUIUpdateDescription[] =
    "Enable Follow UI Update for the Feed.";

const char kEnableTraitCollectionRegistrationName[] =
    "Enable Customizable Trait Registration";
const char kEnableTraitCollectionRegistrationDescription[] =
    "When enabled, UI elements will only observe and respond to the UITraits "
    "to which they have been registered.";

const char kEnableiPadFeedGhostCardsName[] = "Enable ghost cards on iPad feeds";
const char kEnableiPadFeedGhostCardsDescription[] =
    "Enables ghost cards placeholder when feed is loading on iPads.";

const char kEnablePreferencesAccountStorageName[] =
    "Enable the account data storage for preferences for syncing users";
const char kEnablePreferencesAccountStorageDescription[] =
    "Enables storing preferences in a second, Gaia-account-scoped storage for "
    "syncing users";

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

const char kEnableWebChannelsName[] = "Enable WebFeed";
const char kEnableWebChannelsDescription[] =
    "Enable folowing content from web and display Following feed on NTP based "
    "on sites that users followed.";

const char kEnhancedSafeBrowsingPromoName[] =
    "Enable Enhanced Safe Browsing Promos";
const char kEnhancedSafeBrowsingPromoDescription[] =
    "When enabled, the Enhanced Safe Browsing inline and infobar promos are "
    "displayed given certain preconditions are met.";

const char kFeedBackgroundRefreshName[] = "Enable feed background refresh";
const char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kFullscreenImprovementName[] = "Improve fullscreen";
const char kFullscreenImprovementDescription[] =
    "When enabled, fullscreen should have a better stability.";

const char kFullscreenPromosManagerSkipInternalLimitsName[] =
    "Fullscreen Promos Manager (Skip internal Impression Limits)";
const char kFullscreenPromosManagerSkipInternalLimitsDescription[] =
    "When enabled, the internal Impression Limits of the Promos Manager will "
    "be ignored; this is useful for local development.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kHomeCustomizationName[] = "Home Customization";
const char kHomeCustomizationDescription[] =
    "When enabled, adds a menu to personalize the Home surface.";

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

const char kInactiveTabButtonRefactoringName[] =
    "Inactive tab button refactoring";
const char kInactiveTabButtonRefactoringDescription[] =
    "When enabled, the inactive tab button is refactored to be a cell instead "
    "of a header.";

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

// Title and description for the flag to enable detecting the username in the
// username first flows for saving.
const char kIOSDetectUsernameInUffName[] = "Detect username in UFF";
const char kIOSDetectUsernameInUffDescription[] =
    "Detect the username in UFF for saving.";

const char kIOSDockingPromoName[] = "Docking Promo";
const char kIOSDockingPromoDescription[] =
    "When enabled, the user will be presented an animated, instructional "
    "promo showing how to move Chrome to their native iOS dock.";

extern const char kIOSEditMenuHideSearchWebName[] =
    "Hides Search Web in edit menu";
extern const char kIOSEditMenuHideSearchWebDescription[] =
    "Hides the Search Web entry in edit menu.";

const char kIOSKeyboardAccessoryUpgradeName[] =
    "Enable the keyboard accessory upgrade on iOS";
const char kIOSKeyboardAccessoryUpgradeDescription[] =
    "When enabled, the upgraded keyboard accessory UI will be presented.";

const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuName[] =
    "Enable the keyboard accessory upgrade on iOS with a shorter manual fill "
    "menu";
const char kIOSKeyboardAccessoryUpgradeShortManualFillMenuDescription[] =
    "When enabled, the upgraded keyboard accessory UI will be presented with a "
    "shorter manual fill menu.";

const char kIOSChooseFromDriveName[] = "IOS Choose from Drive";
const char kIOSChooseFromDriveDescription[] =
    "Enables the Choose from Drive feature on iOS.";

const char kIOSSaveToDriveName[] = "IOS Save to Drive";
const char kIOSSaveToDriveDescription[] =
    "Enables the Save to Drive feature on iOS.";

const char kIOSSaveToPhotosImprovementsName[] =
    "IOS Save to Photos Improvements";
const char kIOSSaveToPhotosImprovementsDescription[] =
    "Enables the Save to Photos Improvements on iOS.";

const char kIOSSaveToPhotosName[] = "IOS Save to Photos";
const char kIOSSaveToPhotosDescription[] =
    "Enables the Save to Photos feature on iOS.";

const char kIOSPasswordBottomSheetAutofocusName[] =
    "IOS Password Manager Bottom Sheet Autofocus";
const char kIOSPasswordBottomSheetAutofocusDescription[] =
    "Enables triggering the password bottom sheet on autofocus on IOS.";

const char kIOSProactivePasswordGenerationBottomSheetName[] =
    "IOS Proactive Password Generation Bottom Sheet";
const char kIOSProactivePasswordGenerationBottomSheetDescription[] =
    "Enables the display of the proactive password generation bottom sheet on "
    "IOS.";

const char kSyncWebauthnCredentialsName[] = "Sync WebAuthn credentials";
const char kSyncWebauthnCredentialsDescription[] =
    "Allow syncing, managing, and displaying Google Password Manager WebAuthn "
    "credential ('passkey') metadata";

const char kIOSQuickDeleteName[] = "Quick Delete for iOS";
const char kIOSQuickDeleteDescription[] =
    "Enables a new way for users to more easily delete their browsing data in "
    "iOS.";

const char kNewTabPageFieldTrialName[] =
    "New tab page features that target new users";
const char kNewTabPageFieldTrialDescription[] =
    "Enables new tab page features that are available on first run for new "
    "Chrome iOS users.";

const char kIOSSharedHighlightingColorChangeName[] =
    "IOS Shared Highlighting color change";
const char kIOSSharedHighlightingColorChangeDescription[] =
    "Changes the Shared Highlighting color of the text fragment"
    "away from the default yellow in iOS. Works with #scroll-to-text-ios flag.";

const char kIOSSharedHighlightingAmpName[] = "Shared Highlighting on AMP pages";
const char kIOSSharedHighlightingAmpDescription[] =
    "Enables the Create Link option on AMP pages.";

const char kIOSSharedHighlightingV2Name[] = "Text Fragments UI improvements";
const char kIOSSharedHighlightingV2Description[] =
    "Enables improvements to text fragments UI, including a menu for removing "
    "or resharing a highlight.";

const char kIOSSoftLockName[] = "Soft Lock on iOS";
const char kIOSSoftLockDescription[] =
    "Enables an overlay screen over Incognito tabs, whenever the browser is "
    "backgrounded for long periods of time.";

const char kIOSTipsNotificationsName[] = "Tips Notifications";
const char kIOSTipsNotificationsDescription[] =
    "Enables Notifications with content to help new users get the most out of "
    "the app.";

const char kIPHForSafariSwitcherName[] = "IPH for Safari Switcher";
const char kIPHForSafariSwitcherDescription[] =
    "Enables displaying IPH for users who are considered Safari Switcher";

const char kLensFiltersAblationModeEnabledName[] =
    "Lens filters ablation mode enabled";
const char kLensFiltersAblationModeEnabledDescription[] =
    "Enables the filters ablation mode.";

extern const char kLensOverlayForceShowOnboardingScreenName[] =
    "Force show Lens overlay onboarding screen";
extern const char kLensOverlayForceShowOnboardingScreenDescription[] =
    "When enabled, it forces showing the onboarding screen everytime lens "
    "overlay is open";

const char kLensTranslateToggleModeEnabledName[] =
    "Lens translate toggle mode enabled";
const char kLensTranslateToggleModeEnabledDescription[] =
    "Enables the translate toggle mode.";

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

const char kMetrickitNonCrashReportName[] = "Metrickit non-crash reports";
const char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

const char kMigrateSyncingUserToSignedInName[] =
    "Migrate syncing user to signed-in";
const char kMigrateSyncingUserToSignedInDescription[] =
    "Enables the migration of syncing users to the signed-in, non-syncing "
    "state.";

const char kModernTabStripName[] = "Modern TabStrip";
const char kModernTabStripDescription[] =
    "When enabled, the newly implemented tabstrip can be tested.";

const char kMostVisitedTilesHorizontalRenderGroupName[] =
    "MVTiles Horizontal Render Group";
const char kMostVisitedTilesHorizontalRenderGroupDescription[] =
    "When enabled, the MV tiles are represented as individual matches";

const char kNativeFindInPageName[] = "Native Find in Page";
const char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

const char kNewSyncOptInIllustrationName[] = "New sync opt-in illustration";
const char kNewSyncOptInIllustrationDescription[] =
    "Uses the new illustration in the sync opt-in promotion view.";

const char kNTPViewHierarchyRepairName[] = "NTP View Hierarchy Repair";
const char kNTPViewHierarchyRepairDescription[] =
    "Checks if NTP view hierarchy is broken and fixes it if necessary.";

const char kOmniboxActionsInSuggestName[] = "Omnibox actions in suggest";
const char kOmniboxActionsInSuggestDescription[] =
    "Enables actions in suggest for IOS";

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

const char kOmniboxRichAutocompletionName[] =
    "Omnibox rich inline autocompletion";
const char kOmniboxRichAutocompletionDescription[] =
    "Enables omnibox rich inline autocompletion. Expands inline autocomplete "
    "to any type of input that users repeatedly use to get to specific URLs.";

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

const char kOmniboxColorIconsName[] = "Enable color icons in the Omnibox";
const char kOmniboxColorIconsDescription[] =
    "When enabled, displays color microphone and Lens icons in the omnibox.";

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

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kPasswordSharingName[] = "Enables password sharing";
const char kPasswordSharingDescription[] =
    "Enables password sharing between members of the same family.";

const char kDownloadedPDFOpeningName[] = "Enables downloaded PDF opening";
const char kDownloadedPDFOpeningDescription[] =
    "Enables the direct opening of downloaded PDF files in Chrome";

const char kPriceTrackingPromoName[] =
    "Enables price tracking notification promo card";
const char kPriceTrackingPromoDescription[] =
    "Enables being able to show the card in the Magic Stack";

const char kPrivacyGuideIosName[] = "Privacy Guide on iOS";
const char kPrivacyGuideIosDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

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

const char kRemoveExcessNTPsExperimentName[] = "Remove extra New Tab Pages";
const char kRemoveExcessNTPsExperimentDescription[] =
    "When enabled, extra tabs with the New Tab Page open and no navigation "
    "history will be removed.";

const char kRevampPageInfoIosName[] = "Revamp Page Info";
const char kRevampPageInfoIosDescription[] =
    "Revamps Page Info to add two new sections, AboutThisPage and Last "
    "Visited.";

const char kRichBubbleWithoutImageName[] = "Remove image from rich IPH bubble";
const char kRichBubbleWithoutImageDescription[] =
    "When enabled, the rich bubble IPH type will not feature an image, instead "
    "will only have a title and body text.";

const char kSafeBrowsingAsyncRealTimeCheckName[] =
    "Safe Browsing Async Real Time Check";
const char kSafeBrowsingAsyncRealTimeCheckDescription[] =
    "Safe Browsing real-time checks are conducted asynchronously. They no "
    "longer delay page load.";

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

const char kShareInWebContextMenuIOSName[] = "Share in web context menu";
const char kShareInWebContextMenuIOSDescription[] =
    "Enables the Share button in the web context menu in iOS 16.0 and above.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSpotlightNeverRetainIndexName[] = "Don't retain spotlight index";
const char kSpotlightNeverRetainIndexDescription[] =
    "Tentative spotlight memory improvement by not storing a strong pointer to "
    "the spotlight default index";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kTabGroupIndicatorName[] = "Tab Group Indicator";
const char kTabGroupIndicatorDescription[] =
    "When enabled, displays a tab group indicator next to the omnibox.";

const char kTabGroupSyncName[] = "Enable Tab Group Sync";
const char kTabGroupSyncDescription[] =
    "When enabled, if tab-groups-in-grid is enabled, tab groups are synced "
    "between syncing devices.";

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

const char kIOSLargeFakeboxName[] = "Enable Large Fakebox on Home";
const char kIOSLargeFakeboxDescription[] =
    "When enabled, the Fakebox on Home appears larger and has an updated "
    "design.";

const char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
const char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

const char kEnableLensOverlayName[] = "Enable Lens Overlay";
const char kEnableLensOverlayDescription[] = "Enables lens overlay UI";

const char kTabGridNewTransitionsName[] = "Enable new TabGrid transitions";
const char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

const char kTabGroupsIPadName[] = "Enable Tab Groups on iPad";
const char kTabGroupsIPadDescription[] =
    "When enabled, if tab-groups-in-grid is enabled, tab group can be created "
    "on iPad.";

const char kTabInactivityThresholdName[] = "Change Tab inactivity threshold";
const char kTabInactivityThresholdDescription[] =
    "When enabled, the tabs older than the threshold are considered inactive "
    "and set aside in the Inactive Tabs section of the TabGrid."
    "IMPORTANT: If you ever used the in-app settings for Inactive Tabs, this "
    "flag is never read again.";

const char kTabResumptionName[] = "Enable Tab Resumption";
const char kTabResumptionDescription[] =
    "When enabled, offer users with a quick shortcut to resume the last synced "
    "tab from another device.";

const char kTabResumption1_5Name[] = "Enable Tab Resumption Enhancements";
const char kTabResumption1_5Description[] =
    "When enabled, add some improvements to Tab Resumption UI: Add a See more "
    "button to the cards and show better thumbnails. Requires #tab-resumption.";

const char kTabResumption2Name[] = "Enable Tab Resumption 2.0";
const char kTabResumption2Description[] =
    "When enabled, enable the version 2.0 of tab resumption. Requires Tab "
    "resumption to be enabled.";

const char kTabResumption2ReasonName[] = "Enable Tab Resumption 2.0 Reason";
const char kTabResumption2ReasonDescription[] =
    "When enabled, a bubble showing the reason why the tab is shown is "
    "displayed.";

const char kTabResumptionImagesName[] = "Enable Tab Resumption images";
const char kTabResumptionImagesDescription[] =
    "When enabled, a relevant image is displayed in Tab resumption items.";

const char kUndoMigrationOfSyncingUserToSignedInName[] =
    "Undo the migration of syncing users to signed-in";
const char kUndoMigrationOfSyncingUserToSignedInDescription[] =
    "Enables the reverse-migration of syncing users who were previously "
    "migrated to the signed-in, non-syncing state.";

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

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

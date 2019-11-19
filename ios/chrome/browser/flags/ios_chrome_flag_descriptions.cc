// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[] =
    "Autofill Enforce Min Required Fields For Heuristics";
const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before allowing heuristic field-type prediction to occur.";

const char kAutofillEnforceMinRequiredFieldsForQueryName[] =
    "Autofill Enforce Min Required Fields For Query";
const char kAutofillEnforceMinRequiredFieldsForQueryDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before querying the autofill server for field-type predictions.";

const char kAutofillEnforceMinRequiredFieldsForUploadName[] =
    "Autofill Enforce Min Required Fields For Upload";
const char kAutofillEnforceMinRequiredFieldsForUploadDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fillable fields before uploading field-type votes for that form.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillNoLocalSaveOnUnmaskSuccessName[] =
    "Remove the option to save local copies of unmasked server cards";
const char kAutofillNoLocalSaveOnUnmaskSuccessDescription[] =
    "When enabled, the server card unmask prompt will not include the checkbox "
    "to also save the card locally on the current device upon success.";

const char kAutofillNoLocalSaveOnUploadSuccessName[] =
    "Disable saving local copy of uploaded card when credit card upload "
    "succeeds";
const char kAutofillNoLocalSaveOnUploadSuccessDescription[] =
    "When enabled, no local copy of server card will be saved when credit card "
    "upload succeeds.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSaveCardDismissOnNavigationName[] =
    "Save Card Dismiss on Navigation";
const char kAutofillSaveCardDismissOnNavigationDescription[] =
    "Dismisses the Save Card Infobar on a user initiated Navigation, other "
    "than one caused by submitted form.";

const char kAutofillShowAllSuggestionsOnPrefilledFormsName[] =
    "Enable showing all suggestions when focusing prefilled field";
const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[] =
    "When enabled: show all suggestions when the focused field value has not "
    "been entered by the user. When disabled: use the field value as a filter.";

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

extern const char kLogBreadcrumbsName[] = "Log Breadcrumb Events";
extern const char kLogBreadcrumbsDescription[] =
    "When enabled, breadcrumb events will be logged.";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kBrowserContainerKeepsContentViewName[] =
    "Browser Container retains the content view";
const char kBrowserContainerKeepsContentViewDescription[] =
    "When enable, the browser container keeps the content view in the view "
    "hierarchy, to avoid WKWebView from being unloaded from the process.";

const char kCollectionsCardPresentationStyleName[] =
    "Card style presentation for Collections.";
const char kCollectionsCardPresentationStyleDescription[] =
    "When enabled collections are presented using the new iOS13 card "
    "style.";

const char kConfirmInfobarMessagesUIName[] = "Confirm Infobars Messages UI";
const char kConfirmInfobarMessagesUIDescription[] =
    "When enabled Confirm Infobars use the new Messages UI.";

const char kCreditCardScannerName[] = "Enable the 'Use Camera' button";
const char kCreditCardScannerDescription[] =
    "Allow a user to scan a credit card using the credit card camera scanner."
    "The 'Use Camera' button is located in the 'Add Payment Method' view";

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDisableAnimationOnLowBatteryName[] =
    "Disable animations on low battery";
const char kDisableAnimationOnLowBatteryDescription[] =
    "Disable animations when battery level goes below 20%";

const char kDownloadInfobarMessagesUIName[] = "Download Infobars Messages UI";
const char kDownloadInfobarMessagesUIDescription[] =
    "When enabled Downloads use the new Messages UI.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kEmbedderBlockRestoreUrlName[] =
    "Allow embedders to prevent certain URLs from restoring.";
const char kEmbedderBlockRestoreUrlDescription[] =
    "Embedders can prevent URLs from restoring.";

const char kEnableAutofillCreditCardUploadEditableCardholderNameName[] =
    "Make cardholder name editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableCardholderNameDescription[] =
    "If enabled, in certain situations when offering credit card upload to "
    "Google Payments, the cardholder name can be edited within the "
    "offer-to-save dialog, which is prefilled with the name from the signed-in "
    "Google Account.";

const char kEnableAutofillCreditCardUploadEditableExpirationDateName[] =
    "Make expiration date editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableExpirationDateDescription[] =
    "If enabled, if a credit card's expiration date was not detected when "
    "offering card upload to Google Payments, the offer-to-save dialog "
    "displays an expiration date selector.";

const char kEnableClipboardProviderImageSuggestionsName[] =
    "Enable copied image provider";
const char kEnableClipboardProviderImageSuggestionsDescription[] =
    "Enable suggesting a search for the image copied to the clipboard";

const char kEnableClipboardProviderTextSuggestionsName[] =
    "Enable copied text provider";
const char kEnableClipboardProviderTextSuggestionsDescription[] =
    "Enable suggesting a search for text copied to the clipboard";

const char kUseJSForErrorPageName[] = "Enable new error page workflow";
const char kUseJSForErrorPageDescription[] =
    "Use JavaScript for the error pages";

const char kEnablePersistentDownloadsName[] = "Enable persistent downloads";
const char kEnablePersistentDownloadsDescription[] =
    "Enables the new, experimental implementation of persistent downloads";

const char kEnableSyncUSSPasswordsName[] = "Enable USS for passwords sync";
const char kEnableSyncUSSPasswordsDescription[] =
    "Enables the new, experimental implementation of password sync";

const char kEnableSyncUSSNigoriName[] = "Enable USS for sync encryption keys";
const char kEnableSyncUSSNigoriDescription[] =
    "Enables the new, experimental implementation of sync encryption keys";

const char kFindInPageiFrameName[] = "Find in Page in iFrames.";
const char kFindInPageiFrameDescription[] =
    "When enabled, Find In Page will search in iFrames.";

const char kForceUnstackedTabstripName[] = "Force unstacked tabstrip.";
const char kForceUnstackedTabstripDescription[] =
    "When enabled, the tabstrip will draw unstacked, without tab collapsing.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kIgnoresViewportScaleLimitsName[] = "Ignore Viewport Scale Limits";
const char kIgnoresViewportScaleLimitsDescription[] =
    "When enabled the page can always be scaled, regardless of author intent.";

const char kInfobarUIRebootName[] = "Infobar UI Reboot";
const char kInfobarUIRebootDescription[] =
    "When enabled, Infobar will use the new UI.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kLanguageSettingsName[] = "Language Settings";
const char kLanguageSettingsDescription[] =
    "Enables the Language Settings page allowing modifications to user "
    "preferred languages and translate preferences.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kNewClearBrowsingDataUIName[] = "Clear Browsing Data UI";
const char kNewClearBrowsingDataUIDescription[] =
    "Enable new Clear Browsing Data UI.";

const char kNewOmniboxPopupLayoutName[] = "New omnibox popup";
const char kNewOmniboxPopupLayoutDescription[] =
    "Switches the omnibox suggestions and omnibox itself to display the new "
    "design with favicons, new suggestion layout, rich entity support.";

const char kNonModalDialogsName[] = "Use non-modal JavaScript dialogs";
const char kNonModalDialogsDescription[] =
    "Presents JavaScript dialogs non-modally so that the user can change tabs "
    "while a dialog is displayed.";

const char kOfflineVersionWithoutNativeContentName[] =
    "Use offline pages without native content";
const char kOfflineVersionWithoutNativeContentDescription[] =
    "Shows offline pages directly in the web view.  This feature is forced"
    "enabled if web::features::kSlimNavigationManager is enabled.";

const char kOmniboxPopupShortcutIconsInZeroStateName[] =
    "Show zero-state omnibox shortcuts";
const char kOmniboxPopupShortcutIconsInZeroStateDescription[] =
    "Instead of ZeroSuggest, show most visited sites and collection shortcuts "
    "in the omnibox popup.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxUseDefaultSearchEngineFaviconName[] =
    "Default search engine favicon in the omnibox";
const char kOmniboxUseDefaultSearchEngineFaviconDescription[] =
    "Shows default search engine favicon in the omnibox";

const char kOmniboxOnDeviceHeadSuggestionsName[] =
    "Omnibox on device head suggestions";
const char kOmniboxOnDeviceHeadSuggestionsDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model";

const char kPasswordLeakDetectionName[] = "Password Leak Detection";
const char kPasswordLeakDetectionDescription[] =
    "Enables the detection of leaked passwords.";

const char kSaveCardInfobarMessagesUIName[] = "Save Card Infobar Messages UI";
const char kSaveCardInfobarMessagesUIDescription[] =
    "When enabled, Save Card Infobar uses the new Messages UI.";

const char kSearchIconToggleName[] = "Change the icon for the search button";
const char kSearchIconToggleDescription[] =
    "Different icons for the search button.";

const char kSendTabToSelfName[] = "Send tab to self";
const char kSendTabToSelfDescription[] =
    "Allows users to receive tabs that were pushed from another of their "
    "synced devices, in order to easily transition tabs between devices.";

const char kSendTabToSelfBroadcastName[] = "Send tab to self broadcast";
const char kSendTabToSelfBroadcastDescription[] =
    "Allows users to broadcast the tab they send to all of their devices "
    "instead of targetting only one device.";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSettingsAddPaymentMethodName[] =
    "Enable the add payment method button";
const char kSettingsAddPaymentMethodDescription[] =
    "Allow a user to add a new credit card to payment methods from the "
    "settings menu.";

const char kSettingsRefreshName[] = "Enable the UI Refresh for Settings";
const char kSettingsRefreshDescription[] =
    "Change the UI appearance of the settings to have something in phase with "
    "UI Refresh.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSlimNavigationManagerName[] = "Use Slim Navigation Manager";
const char kSlimNavigationManagerDescription[] =
    "When enabled, uses the experimental slim navigation manager that provides "
    "better compatibility with HTML navigation spec.";

const char kSnapshotDrawViewName[] = "Use DrawViewHierarchy for Snapshots";
const char kSnapshotDrawViewDescription[] =
    "When enabled, snapshots will be taken using |-drawViewHierarchy:|.";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kSyncDeviceInfoInTransportModeName[] =
    "Enable syncing DeviceInfo in transport-only sync mode.";
const char kSyncDeviceInfoInTransportModeDescription[] =
    "When enabled, allows syncing DeviceInfo datatype for users who are "
    "signed-in but not necessary sync-ing.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kToolbarNewTabButtonName[] =
    "Enable New Tab button in the bottom toolbar";
const char kToolbarNewTabButtonDescription[] =
    "When enabled, the bottom toolbar middle button opens a new tab";

const char kTranslateInfobarMessagesUIName[] =
    "Enable Translate Infobar Messages UI";
const char kTranslateInfobarMessagesUIDescription[] =
    "When enabled, the Translate Infobar uses the new Messages UI.";

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kUseWKWebViewLoadingName[] =
    "Use WKWebView.loading for WebState::IsLoading";
const char kUseWKWebViewLoadingDescription[] =
    "Enables using WKWebView.loading for WebState::IsLoading";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWebClearBrowsingDataName[] = "Web-API for browsing data";
const char kWebClearBrowsingDataDescription[] =
    "When enabled the Clear Browsing Data feature is using the web API.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

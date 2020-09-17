// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAddWebContentDropInteractionName[] =
    "Add Web Content Drop Interaction";
const char kAddWebContentDropInteractionDescription[] =
    "When enabled, adds ability to drop a URL on the web content area to "
    "navigate to that URL.";

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

const char kAutofillEnableCardNicknameManagementName[] =
    "Enable Autofill card nickname management";
const char kAutofillEnableCardNicknameManagementDescription[] =
    "When enabled, nicknames for credit cards will be able to be modified "
    "locally.";

const char kAutofillEnableCardNicknameUpstreamName[] =
    "Enable Autofill card nickname upstream";
const char kAutofillEnableCardNicknameUpstreamDescription[] =
    "When enabled, nicknames for credit cards will be able to be uploaded to "
    "Payments.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillEnableGoogleIssuedCardName[] =
    "Enable Autofill Google-issued card";
const char kAutofillEnableGoogleIssuedCardDescription[] =
    "When enabled, Google-issued cards will be available in the autofill "
    "suggestions.";

const char kAutofillEnableOffersInDownstreamName[] =
    "Enable Autofill offers in downstream";
const char kAutofillEnableOffersInDownstreamDescription[] =
    "When enabled, offer data will be retrieved during downstream and shown in "
    "the dropdown list.";

const char kAutofillEnableSurfacingServerCardNicknameName[] =
    "Enable surfacing masked server card nicknames";
const char kAutofillEnableSurfacingServerCardNicknameDescription[] =
    "When enabled, if Google Payments cards were given nicknames in a Google "
    "Pay app, Autofill will surface these nicknames in suggestions.";

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

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSaveCardDismissOnNavigationName[] =
    "Save Card Dismiss on Navigation";
const char kAutofillSaveCardDismissOnNavigationDescription[] =
    "Dismisses the Save Card Infobar on a user initiated Navigation, other "
    "than one caused by submitted form.";

const char kAutofillSaveCardInfobarEditSupportName[] =
    "Save Card Infobar Edit Support";
const char kAutofillSaveCardInfobarEditSupportDescription[] =
    "When enabled and saving a credit card to Google Payments, a dialog is "
    "displayed that allows editing the card info before confirming save.";

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

const char kBlockPopupInfobarMessagesUIName[] =
    "Block Popup Infobars Messages UI";
const char kBlockPopupInfobarMessagesUIDescription[] =
    "When enabled Block Popup Infobars use the new Messages UI. "
    "IOSInfobarUIReboot needs to be enabled as well for this to work.";

extern const char kLogBreadcrumbsName[] = "Log Breadcrumb Events";
extern const char kLogBreadcrumbsDescription[] =
    "When enabled, breadcrumb events will be logged.";

const char kSyntheticCrashReportsForUteName[] =
    "Generate synthetic crash reports for UTE";
const char kSyntheticCrashReportsForUteDescription[] =
    "When enabled the app will create synthetic crash report when chrome "
    "starts up after Unexplained Termination Event (UTE).";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

extern const char kChangeTabSwitcherPositionName[] =
    "Change tab switcher button position";
extern const char kChangeTabSwitcherPositionDescription[] =
    "When enable, the tab switcher button position changes from tab strip to "
    "toolbar and bookmark button is removed.";

const char kCollectionsCardPresentationStyleName[] =
    "Card style presentation for Collections.";
const char kCollectionsCardPresentationStyleDescription[] =
    "When enabled collections are presented using the new iOS13 card "
    "style.";

const char kConfirmInfobarMessagesUIName[] = "Confirm Infobars Messages UI";
const char kConfirmInfobarMessagesUIDescription[] =
    "When enabled Confirm Infobars use the new Messages UI.";

const char kCrashRestoreInfobarMessagesUIName[] =
    "Crash Restore Infobars Messages UI";
const char kCrashRestoreInfobarMessagesUIDescription[] =
    "When enabled Crash Restore Infobars use the new Messages UI.";

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

const char kDefaultBrowserSettingsName[] = "Setting to change Default Browser";
const char kDefaultBrowserSettingsDescription[] =
    "When enabled, adds a button in the settings to allow changing the default "
    "browser in the Settings.app.";

const char kDefaultToDesktopOnIPadName[] = "Request desktop version by default";
const char kDefaultToDesktopOnIPadDescription[] =
    "By default, on iPad, the desktop version of the web sites will be "
    "requested";

const char kDefaultBrowserFullscreenPromoName[] =
    "Fullscreen modal promo about the default browser feature";
const char kDefaultBrowserFullscreenPromoDescription[] =
    "When enabled, will allow for a fullscreen modal promo to be shown to "
    "users informing them about the default browser feature and providing a "
    "button that takes users to Settings.app to update their default browser.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDisableProgressBarAnimationName[] =
    "Disable page load progress bar animation";
const char kDisableProgressBarAnimationDescription[] =
    "Disable progress bar animation when a page loads.";

const char kDiscoverFeedInNtpName[] = "Enable new content Suggestion Feed";
const char kDiscoverFeedInNtpDescription[] =
    "When enabled, replaces articles feed with new content Suggestion Feed in "
    "the NTP.";

const char kDownloadInfobarMessagesUIName[] = "Download Infobars Messages UI";
const char kDownloadInfobarMessagesUIDescription[] =
    "When enabled Downloads use the new Messages UI.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kEditBookmarksIOSName[] = "Edit Bookmarks IOS";
const char kEditBookmarksIOSDescription[] =
    "Enables support for the EditBookmarksEnabled enterprise policy on iOS";

const char kEmbedderBlockRestoreUrlName[] =
    "Allow embedders to prevent certain URLs from restoring.";
const char kEmbedderBlockRestoreUrlDescription[] =
    "Embedders can prevent URLs from restoring.";

const char kEnableAutofillCacheServerCardInfoName[] =
    "Enable Autofill to cache unmasked server card info";
const char kEnableAutofillCacheServerCardInfoDescription[] =
    "If enabled, when a server card is unmasked, its info will be cached until "
    "page navigation to simplify consecutive fills on the same page.";

const char kEnableAutofillPasswordReauthIOSName[] =
    "Enable Autofill reauth request for passwords";
const char kEnableAutofillPasswordReauthIOSDescription[] =
    "If enabled, when a credential will be filled, the browser will require "
    "the user to authenticate with the device first.";

const char kEnableClipboardProviderImageSuggestionsName[] =
    "Enable copied image provider";
const char kEnableClipboardProviderImageSuggestionsDescription[] =
    "Enable suggesting a search for the image copied to the clipboard";

const char kEnableFullPageScreenshotName[] = "Enable fullpage screenshots";
const char kEnableFullPageScreenshotDescription[] =
    "Enables the option of capturing an entire webpage as a PDF when a "
    "screenshot is taken.";

const char kEnableIOSManagedSettingsUIName[] = "Enable IOS Managed Settings UI";
const char kEnableIOSManagedSettingsUIDescription[] =
    "Enable showing a different UI when the setting is managed by an "
    "enterprise policy on iOS.";

const char kEnableMyGoogleName[] = "Enable MyGoogle UI";
const char kEnableMyGoogleDescription[] =
    "Enable MyGoogle account management UI in iOS Settings";

const char kEnableNativeContextMenusName[] =
    "Enable Context Menus in Native UI";
const char kEnableNativeContextMenusDescription[] =
    "Enables the new iOS 13 context menus on various pieces of UI in native "
    "Chrome (e.g. History, Bookmarks).";

const char kEnablePersistentDownloadsName[] = "Enable persistent downloads";
const char kEnablePersistentDownloadsDescription[] =
    "Enables the new, experimental implementation of persistent downloads";

const char kExpandedTabStripName[] = "Enable expanded tabstrip";
const char kExpandedTabStripDescription[] =
    "Enables the new expanded tabstrip. Activated by swiping down the tabstrip"
    " or the toolbar";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kForceUnstackedTabstripName[] = "Force unstacked tabstrip.";
const char kForceUnstackedTabstripDescription[] =
    "When enabled, the tabstrip will draw unstacked, without tab collapsing.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kFullscreenControllerBrowserScopedName[] =
    "Scope FullscreenController to Browser";
const char kFullscreenControllerBrowserScopedDescription[] =
    "When enabled, FullscreenController will be stored and retrieved using the "
    "Browser.";

const char kIllustratedEmptyStatesName[] = "Illustrated empty states";
const char kIllustratedEmptyStatesDescription[] =
    "Display new illustrations and layout on empty states.";

const char kInfobarOverlayUIName[] = "Use OverlayPresenter for infobars";
const char kInfobarOverlayUIDescription[] =
    "When enabled alongside the Infobar UI Reboot, infobars will be presented "
    "using OverlayPresenter.";

const char kInfobarUIRebootName[] = "Infobar UI Reboot";
const char kInfobarUIRebootDescription[] =
    "When enabled, Infobar will use the new UI.";

const char kInfobarUIRebootOnlyiOS13Name[] = "Infobar UI Reboot iOS13";
const char kInfobarUIRebootOnlyiOS13Description[] =
    "When enabled, Infobar will use the new UI only on iOS13";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kIOSLegacyTLSInterstitialsName[] = "Show legacy TLS interstitials";
const char kIOSLegacyTLSInterstitialsDescription[] =
    "When enabled, an interstitial will be shown on main-frame navigations "
    "that use legacy TLS connections, and subresources using legacy TLS "
    "connections will be blocked.";

const char kIOSLookalikeUrlNavigationSuggestionsUIName[] =
    "Lookalike URL Navigation Suggestions UI";
const char kIOSLookalikeUrlNavigationSuggestionsUIDescription[] =
    "When enabled, an interstitial will be shown on navigations to lookalike "
    "URLs.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kManagedBookmarksIOSName[] = "Managed Bookmarks IOS";
const char kManagedBookmarksIOSDescription[] =
    "When enabled, managed bookmarks set by an enterprise policy can be shown "
    "in the bookmarks UI on iOS";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kMobileGoogleSRPName[] = "Mobile version of Google SRP by default";
const char kMobileGoogleSRPDescription[] =
    "Request the Mobile version of Google SRP by default when the desktop mode "
    "is requested by default.";

const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName[] =
    "Omnibox Preserve Default Match Against Async Update";
const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription[] =
    "Preserves the default match against change when providers return results "
    "asynchronously. This prevents the default match from changing after the "
    "user finishes typing. Without this feature, if the default match is "
    "updated right when the user presses Enter, the user may go to a "
    "surprising destination.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

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

const char kOmniboxOnFocusSuggestionsName[] = "Omnibox on-focus suggestions";
const char kOmniboxOnFocusSuggestionsDescription[] =
    "Configures Omnibox on-focus suggestions - suggestions displayed on-focus "
    "before the user has typed any input. This provides overrides for the "
    "default suggestion locations.";

const char kOpenDownloadsInFilesAppName[] = "Open Downloads in Files.app";
const char kOpenDownloadsInFilesAppDescription[] =
    "Allows user to open Files.app after pressing the 'Downloads' button from "
    "the tools menu or after pressing the 'Open in downloads' button invoked "
    "by pressing 'Open In...' after download completes.";

const char kPasswordCheckName[] = "Bulk Password Check";
const char kPasswordCheckDescription[] =
    "Enables the Bulk Password Check feature for signed-in users.";

const char kPageInfoRefactoringName[] = "New design of the page info";
const char kPageInfoRefactoringDescription[] =
    "Uses the new design for the page security info.";

#if defined(__IPHONE_13_4)
const char kPointerSupportName[] = "Enables pointer support on tablets";
const char kPointerSupportDescription[] =
    "Enables pointer support on tablets on iOS 13.4 and above.";
#endif  // defined(__IPHONE_13_4)

const char kReloadSadTabName[] = "Reload SadTab automatically";
const char kReloadSadTabDescription[] =
    "When enabled, the first time the renderer crashes, the page is reloaded "
    "instead of showing the SadTab";

const char kSafeBrowsingAvailableName[] = "Make Safe Browsing available";
const char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

const char kSafeBrowsingRealTimeLookupName[] = "Enable real-time Safe Browsing";
const char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

const char kSafetyCheckIOSName[] = "Enable safety check on iOS";
const char kSafetyCheckIOSDescription[] =
    "When enabled, the iOS version of safety check is available in Chrome "
    "settings.";

const char kSaveCardInfobarMessagesUIName[] = "Save Card Infobar Messages UI";
const char kSaveCardInfobarMessagesUIDescription[] =
    "When enabled, Save Card Infobar uses the new Messages UI.";

const char kScrollToTextIOSName[] = "Enable Scroll to Text";
const char kScrollToTextIOSDescription[] =
    "When enabled, opening a URL with a text fragment (e.g., "
    "example.com/#:~:text=examples) will cause matching text in the page to be "
    "highlighted and scrolled into view.";

const char kSendTabToSelfName[] = "Send tab to self";
const char kSendTabToSelfDescription[] =
    "Allows users to receive tabs that were pushed from another of their "
    "synced devices, in order to easily transition tabs between devices.";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSettingsRefreshName[] = "Enable the UI Refresh for Settings";
const char kSettingsRefreshDescription[] =
    "Change the UI appearance of the settings to have something in phase with "
    "UI Refresh.";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment. Works best with the #scroll-to-text-ios flag.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSnapshotDrawViewName[] = "Use DrawViewHierarchy for Snapshots";
const char kSnapshotDrawViewDescription[] =
    "When enabled, snapshots will be taken using |-drawViewHierarchy:|.";

const char kSSLCommittedInterstitialsName[] =
    "Enable SSL committed interstitials";
const char kSSLCommittedInterstitialsDescription[] =
    "When enabled, SSL interstitial pages will be committed rather than using "
    "an overlay on the page.";

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

const char kTranslateInfobarMessagesUIName[] =
    "Enable Translate Infobar Messages UI";
const char kTranslateInfobarMessagesUIDescription[] =
    "When enabled, the Translate Infobar uses the new Messages UI.";

const char kURLBlocklistIOSName[] = "URL Blocklist Policy";
const char kURLBlocklistIOSDescription[] =
    "When enabled, URLs can be blocked/allowed by the URLBlocklist/URLAllowlist"
    "enterprise policies.";

const char kUseJSForErrorPageName[] = "Enable new error page workflow";
const char kUseJSForErrorPageDescription[] =
    "Use JavaScript for the error pages";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWebPageDefaultZoomFromDynamicTypeName[] =
    "Use dynamic type size for default text zoom level";
const char kWebPageDefaultZoomFromDynamicTypeDescription[] =
    "When enabled, the default text zoom level for a website comes from the "
    "current dynamic type setting.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

const char kWellKnownChangePasswordName[] =
    "Support for .well-known/change-password";
const char kWellKnownChangePasswordDescription[] =
    "If enabled the 'change password' button in password checkup redirects to "
    "the .well-known/change-password path. The path is supposed to point to "
    "the password change form of the site. When the site doesn't support "
    ".well-known/change-password it is checked if a fallback url is available. "
    "Otherwise the user is redirected to the origin.";
// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

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

const char kAutofillEnableOffersInDownstreamName[] =
    "Enable Autofill offers in downstream";
const char kAutofillEnableOffersInDownstreamDescription[] =
    "When enabled, offer data will be retrieved during downstream and shown in "
    "the dropdown list.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillParseMerchantPromoCodeFieldsName[] =
    "Parse promo code fields in forms";
const char kAutofillParseMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to find merchant promo/coupon/gift "
    "code fields when parsing forms.";

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

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
const char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

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

const char kChangePasswordAffiliationInfoName[] =
    "Using Affiliation Service for Change Password URLs";
const char kChangePasswordAffiliationInfoDescription[] =
    "In case site doesn't support /.well-known/change-password Chrome will try "
    "to obtain it using Affiliation Service.";

const char kCollectionsCardPresentationStyleName[] =
    "Card style presentation for Collections.";
const char kCollectionsCardPresentationStyleDescription[] =
    "When enabled collections are presented using the new iOS13 card "
    "style.";

const char kCrashpadIOSName[] = "Use Crashpad for crash collection.";
const char kCrashpadIOSDescription[] =
    "When enabled use Crashpad to generate crash reports crash collection. "
    "When disabled use Breakpad. This flag takes two restarts to take effect";

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

const char kDefaultPromoNonModalName[] = "Default Browser Non-Modal Promo";
const char kDefaultPromoNonModalDescription[] =
    "When enabled non-modal default browser promos can be triggered.";

const char kDefaultPromoTailoredName[] =
    "Default Browser Tailored Fullscreen Promo";
const char kDefaultPromoTailoredDescription[] =
    "When enabled the selected tailored fullscreen promo can be triggered.";

const char kDefaultToDesktopOnIPadName[] = "Request desktop version by default";
const char kDefaultToDesktopOnIPadDescription[] =
    "By default, on iPad, the desktop version of the web sites will be "
    "requested";

const char kDefaultBrowserFullscreenPromoExperimentName[] =
    "Default Browser Fullscreen modal experiment";
const char kDefaultBrowserFullscreenPromoExperimentDescription[] =
    "When enabled, will show a modified default browser fullscreen modal promo "
    "UI.";

const char kDefaultBrowserFullscreenPromoCTAExperimentName[] =
    "Default Browser Fullscreen modal experiment with different CTA";
const char kDefaultBrowserFullscreenPromoCTAExperimentDescription[] =
    "When enabled, will show a modified default browser fullscreen modal promo "
    "UI.";

const char kDelayThresholdMinutesToUpdateGaiaCookieName[] =
    "Delay for polling (in minutes) to verify the existence of GAIA cookies.";
const char kDelayThresholdMinutesToUpdateGaiaCookieDescription[] =
    "Used for testing purposes to reduce the amount of delay between polling "
    "intervals.";

const char kDetectFormSubmissionOnFormClearIOSName[] =
    "Detect form submission when the form is cleared.";
const char kDetectFormSubmissionOnFormClearIOSDescription[] =
    "Detect form submissions for change password forms that are cleared and "
    "not removed from the page.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDiscoverFeedInNtpName[] = "Enable new content Suggestion Feed";
const char kDiscoverFeedInNtpDescription[] =
    "When enabled, replaces articles feed with new content Suggestion Feed in "
    "the NTP.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kRestoreSessionFromCacheName[] =
    "Use iOS_TBA native WKWebView sesion restoration.";
const char kRestoreSessionFromCacheDescription[] =
    "Enable iOS_TBA instant session restoration for faster and more "
    "web session restoration.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableCloseAllTabsConfirmationName[] =
    "Enable Close All Tabs confirmation";
const char kEnableCloseAllTabsConfirmationDescription[] =
    "Enable showing an action sheet that asks for confirmation when 'Close "
    "All' button is tapped on the tab grid to avoid unwanted clearing.";

const char kEnableFREUIModuleIOSName[] = "Enable FRE UI module";
const char kEnableFREUIModuleIOSDescription[] =
    "Enable the option of using new FRE UI module to show first run screens.";

const char kEnableFullPageScreenshotName[] = "Enable fullpage screenshots";
const char kEnableFullPageScreenshotDescription[] =
    "Enables the option of capturing an entire webpage as a PDF when a "
    "screenshot is taken.";

const char kEnableIOSManagedSettingsUIName[] = "Enable IOS Managed Settings UI";
const char kEnableIOSManagedSettingsUIDescription[] =
    "Enable showing a different UI when the setting is managed by an "
    "enterprise policy on iOS.";

const char kEnableManualPasswordGenerationName[] =
    "Enable manual password generation.";
const char kEnableManualPasswordGenerationDescription[] =
    "Enable UI that allows to generate a strong password for any password "
    "field";

const char kExpandedTabStripName[] = "Enable expanded tabstrip";
const char kExpandedTabStripDescription[] =
    "Enables the new expanded tabstrip. Activated by swiping down the tabstrip"
    " or the toolbar";

const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";

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

const char kIncognitoAuthenticationName[] =
    "Device Authentication for Incognito";
extern const char kIncognitoAuthenticationDescription[] =
    "When enabled, a setting appears to enable biometric authentication for "
    "accessing incognito.";

const char kIllustratedEmptyStatesName[] = "Illustrated empty states";
const char kIllustratedEmptyStatesDescription[] =
    "Display new illustrations and layout on empty states.";

const char kInfobarOverlayUIName[] = "Use OverlayPresenter for infobars";
const char kInfobarOverlayUIDescription[] =
    "When enabled alongside the Infobar UI Reboot, infobars will be presented "
    "using OverlayPresenter.";

const char kInterestFeedNoticeCardAutoDismissName[] =
    "New Content Suggestions notice card auto-dismiss";
const char kInterestFeedNoticeCardAutoDismissDescription[] =
    "Auto-dismiss the notice card when there are enough clicks or views on the "
    "notice card.";

const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[] =
    "New Content Suggestions taps/views conditional upload";
const char kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of taps/views after satisfying conditions (e.g., "
    "user views X cards)";

const char kSigninNotificationInfobarUsernameInTitleName[] =
    "Sign-in notification infobar title";
const char kSigninNotificationInfobarUsernameInTitleDescription[] =
    "When enabled, uses the authenticated user's full name in the infobar "
    "title.";

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

const char kIOSPersistCrashRestoreName[] = "Persist Crash Restore Infobar";
const char kIOSPersistCrashRestoreDescription[] =
    "When enabled, the Crash Restore Infobar will persist through navigations "
    "instead of dismissing.";

const char kIOSSharedHighlightingColorChangeName[] =
    "IOS Shared Highlighting color change";
const char kIOSSharedHighlightingColorChangeDescription[] =
    "Changes the Shared Highlighting color of the text fragment"
    "away from the default yellow in iOS. Works with #scroll-to-text-ios flag.";

const char kSharedHighlightingUseBlocklistIOSName[] =
    "Shared Highlighting blocklist";
const char kSharedHighlightingUseBlocklistIOSDescription[] =
    "Uses a blocklist to disable Shared Highlighting link generation on "
    "certain sites where personalized or dynamic content or other technical "
    "restrictions make it unlikely that a URL can be generated and actually "
    "work when shared.";

const char kLocationPermissionsPromptName[] =
    "Location Permisssions Prompt Experiment";
const char kLocationPermissionsPromptDescription[] =
    "When enabled, a different user experience flow will be shown to ask for "
    "location permissions.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kMobileGoogleSRPName[] = "Mobile version of Google SRP by default";
const char kMobileGoogleSRPDescription[] =
    "Request the Mobile version of Google SRP by default when the desktop mode "
    "is requested by default.";

const char kMobileIdentityConsistencyName[] = "Mobile identity consistency";
const char kMobileIdentityConsistencyDescription[] =
    "Enables identity consistency on mobile by decoupling sync and sign-in.";

const char kModernTabStripName[] = "Modern TabStrip";
const char kModernTabStripDescription[] =
    "When enabled, the newly implemented tabstrip can be tested.";

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

const char kOmniboxLocalHistoryZeroSuggestName[] =
    "Omnibox local zero-prefix suggestions";
const char kOmniboxLocalHistoryZeroSuggestDescription[] =
    "Configures the omnibox zero-prefix suggestion to use local search "
    "history.";

const char kOmniboxNewImplementationName[] =
    "Use experimental omnibox textfield";
const char kOmniboxNewImplementationDescription[] =
    "Uses a textfield implementation that doesn't use UILabels internally";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kRefactoredNTPName[] = "Enables refactored new tab page";
const char kRefactoredNTPDescription[] =
    "When enabled, the new tab page is replaced with the refactored version, "
    "which changes the ownership and containment of views.";

const char kRestoreGaiaCookiesOnUserActionName[] =
    "Restore GAIA cookies on user action";
const char kRestoreGaiaCookiesOnUserActionDescription[] =
    "When enabled, will restore GAIA cookies for signed-in Chrome users if "
    "the user explicitly requests a Google service.";

const char kSafeBrowsingAvailableName[] = "Make Safe Browsing available";
const char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

const char kSafeBrowsingRealTimeLookupName[] = "Enable real-time Safe Browsing";
const char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

const char kScreenTimeIntegrationName[] = "Enables ScreenTime Integration";
const char kScreenTimeIntegrationDescription[] =
    "Enables integration with ScreenTime in iOS 14.0 and above.";

const char kScrollToTextIOSName[] = "Enable Scroll to Text";
const char kScrollToTextIOSDescription[] =
    "When enabled, opening a URL with a text fragment (e.g., "
    "example.com/#:~:text=examples) will cause matching text in the page to be "
    "highlighted and scrolled into view.";

const char kSearchHistoryLinkIOSName[] = "Enables Search History Link";
const char kSearchHistoryLinkIOSDescription[] =
    "Changes the Clear Browsing Data "
    "UI to display a link to clear search history on My Google Activity.";

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

const char kSimplifySignOutIOSName[] = "Simplify sign-out";
const char kSimplifySignOutIOSDescription[] =
    "When enabled, sign-out UI in the account table view is simplified.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kStartSurfaceName[] = "Start Surface";
const char kStartSurfaceDescription[] =
    "Enable showing the Start Surface when launching Chrome via clicking the "
    "icon or the app switcher.";

const char kTabGridContextMenuName[] = "Enable Tab Grid context menu";
const char kTabGridContextMenuDescription[] =
    "Enables the context menu for long press on tabs on the tab grid.";

const char kTabsBulkActionsName[] = "Enable Tab Grid Bulk Actions";
const char kTabsBulkActionsDescription[] =
    "Enables the selection mode in the Tab grid where users can perform "
    "actions on multiple tabs at once for iOS 13 and above.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kURLBlocklistIOSName[] = "URL Blocklist Policy";
const char kURLBlocklistIOSDescription[] =
    "When enabled, URLs can be blocked/allowed by the URLBlocklist/URLAllowlist"
    "enterprise policies.";

const char kUseJSForErrorPageName[] = "Enable new error page workflow";
const char kUseJSForErrorPageDescription[] =
    "Use JavaScript for the error pages";

const char kUseOfHashAffiliationFetcherName[] =
    "Use of Hash Affiliation Fetcher";
const char kUseOfHashAffiliationFetcherDescription[] =
    "All requests to the affiliation fetcher are made through the hash prefix "
    "lookup. Enables use of Hash Affiliation Service for non-synced users.";

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

const char kWebPageAlternativeTextZoomName[] =
    "Use different method for zooming web pages";
const char kWebPageAlternativeTextZoomDescription[] =
    "When enabled, switches the method used to zoom web pages.";

const char kWebViewNativeContextMenuName[] =
    "Use the native Context Menus in the WebView";
const char kWebViewNativeContextMenuDescription[] =
    "When enabled, the native context menu are displayed when the user long "
    "press on a link or an image.";

const char kRecordSnapshotSizeName[] =
    "Record the size of image and PDF snapshots in UMA histograms";
const char kRecordSnapshotSizeDescription[] =
    "When enabled, the app will record UMA histograms for image and PDF "
    "snapshots. PDF snaphot will be taken just for the purpose of the "
    "histogram recording.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

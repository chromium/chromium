// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAddToHomeScreenName[] = "Add to home screen";
const char kAddToHomeScreenDescription[] =
    "Allows to add a bookmark on the device home screen when sharing from a "
    "web view.";

const char kAppStoreRatingName[] = "Enable the App Store Rating promo.";
const char kAppStoreRatingDescription[] =
    "When enabled, App Store Rating promo will be presented to eligible "
    "users.";

const char kAutofillAccountProfilesStorageName[] =
    "Enable profile saving in Google Account";
const char kAutofillAccountProfilesStorageDescription[] =
    "When enabled, the profiles would be saved to the Google Account";

const char kAutofillAccountProfilesUnionViewName[] =
    "Enable compatibility with GAS";
const char kAutofillAccountProfilesUnionViewDescription[] =
    "When enabled, the GAS profiles would start up showing in settings";

const char kAutofillBrandingIOSName[] = "Autofill Branding on iOS";
const char kAutofillBrandingIOSDescription[] =
    "Adds the Chrome logo in the form input suggestions bar. If select "
    "\"Enabled\", the branding logo shows twice, and would not be "
    "dismissed with any animation.";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillEnableCardArtImageName[] = "Enable showing card art images";
const char kAutofillEnableCardArtImageDescription[] =
    "When enabled, card product images (instead of network icons) will be "
    "shown in Payments Autofill UI.";

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

const char kAutofillEnableRemadeDownstreamMetricsName[] =
    "Enable remade Autofill Downstream metrics logging";
const char kAutofillEnableRemadeDownstreamMetricsDescription[] =
    "When enabled, some extra metrics logging for Autofill Downstream will "
    "start.";

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments UI.";

const char kAutofillEnforceDelaysInStrikeDatabaseName[] =
    "Enforce delay between offering Autofill opportunities in the strike "
    "database";
const char kAutofillEnforceDelaysInStrikeDatabaseDescription[] =
    "When enabled, if previous Autofill feature offer was declined, "
    "Chrome will wait for sometime before showing the offer again.";

const char kAutofillFillMerchantPromoCodeFieldsName[] =
    "Enable Autofill of promo code fields in forms";
const char kAutofillFillMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to fill merchant promo/coupon/gift "
    "code fields when data is available.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillOfferToSaveCardWithSameLastFourName[] =
    "Offer credit card save for cards with same last-4 but different "
    "expiration dates";
const char kAutofillOfferToSaveCardWithSameLastFourDescription[] =
    "Offer credit card save when Chrome detects a card number with the same "
    "last 4 digits as an existing server card, but a different expiration "
    "date.";

const char kAutofillParseIBANFieldsName[] = "Parse IBAN fields in forms";
const char kAutofillParseIBANFieldsDescription[] =
    "When enabled, Autofill will attempt to find International Bank Account "
    "Number (IBAN) fields when parsing forms.";

const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[] =
    "Parse standalone CVC fields for VCN card on file in forms";
const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[] =
    "When enabled, Autofill will attempt to find standalone CVC fields for VCN "
    "card on file when parsing forms.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSuggestServerCardInsteadOfLocalCardName[] =
    "Suggest Server card instead of Local card for deduped cards";
const char kAutofillSuggestServerCardInsteadOfLocalCardDescription[] =
    "When enabled, Autofill suggestions that consist of a local and server "
    "version of the same card will attempt to fill the server card upon "
    "selection instead of the local card.";

const char kAutofillUpstreamAllowAdditionalEmailDomainsName[] =
    "Allow Autofill credit card upload save for select non-Google-based "
    "accounts";
const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[] =
    "When enabled, credit card upload is offered if the user's logged-in "
    "account's domain is from a common email provider.";

const char kAutofillUpstreamAllowAllEmailDomainsName[] =
    "Allow Autofill credit card upload save for all non-Google-based accounts";
const char kAutofillUpstreamAllowAllEmailDomainsDescription[] =
    "When enabled, credit card upload is offered without regard to the user's "
    "logged-in account's domain.";

const char kAutofillUpstreamAuthenticatePreflightCallName[] =
    "Set authentication token in credit card upload preflight call";
const char kAutofillUpstreamAuthenticatePreflightCallDescription[] =
    "When enabled, sets the OAuth2 token in GetUploadDetails requests to "
    "Google Payments, in order to provide a better experience for users with "
    "server-side features disabled but not client-side features.";

const char kAutofillUpstreamUseAlternateSecureDataTypeName[] =
    "Use alternate secure data type for credit card upload save";
const char kAutofillUpstreamUseAlternateSecureDataTypeDescription[] =
    "When enabled, the secure data type for cards sent during credit card "
    "upload save is updated to match newer server requirements.";

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

const char kBottomOmniboxSteadyStateName[] = "Bottom Omnibox (Steady)";
const char kBottomOmniboxSteadyStateDescription[] =
    "Move the omnibox to the bottom in steady state";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kBringYourOwnTabsIOSName[] =
    "Bring Your Active Tabs from Android to iOS";
const char kBringYourOwnTabsIOSDescription[] =
    "For new users who switch to Chrome on iOS from Android, show a prompt on "
    "the tab grid with buttons to list or open those tabs. The prompt would be "
    "a half-sheet modal by default, or a bottom sticker if specified in the "
    "dropdown option.";

extern const char kAppleCalendarExperienceKitName[] =
    "Experience Kit Apple Calendar";
extern const char kAppleCalendarExperienceKitDescription[] =
    "When enabled, long pressing on dates will trigger Experience Kit Apple "
    "Calendar event handling.";

extern const char kConsistencyNewAccountInterfaceName[] =
    "Consistency New Account Interface";
extern const char kConsistencyNewAccountInterfaceDescription[] =
    "Enables a sign-in only UI for users who need to add a new account.";

extern const char kEmailName[] = "Email experience enable";
extern const char kEmailDescription[] =
    "When enabled, one tapping or long pressing on an email address will "
    "trigger the email experience.";

extern const char kPhoneNumberName[] = "Phone number experience enable";
extern const char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

extern const char kEnableExpKitTextClassifierName[] =
    "Text Classifier in Experience Kit";
extern const char kEnableExpKitTextClassifierDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "entity detection where possible.";

extern const char kOneTapForMapsName[] = "Enable one Tap Experience for Maps";
extern const char kOneTapForMapsDescription[] =
    "Enables the one tap experience for maps experience kit.";

const char kEnablePopoutOmniboxIpadName[] = "Popout omnibox (iPad)";
const char kEnablePopoutOmniboxIpadDescription[] =
    "Make omnibox popup appear in a detached rounded rectangle below the "
    "omnibox.";

extern const char kMagicStackName[] = "Enable Magic Stack";
extern const char kMagicStackDescription[] =
    "When enabled, a Magic Stack will be shown in the Home surface displaying "
    "a variety of modules.";

const char kCredentialProviderExtensionPromoName[] =
    "Enable the Credential Provider Extension promo.";
const char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, Credential Provider Extension promo will be "
    "presented to eligible users.";

const char kDefaultBrowserBlueDotPromoName[] = "Default Browser Blue Dot Promo";
const char kDefaultBrowserBlueDotPromoDescription[] =
    "When enabled, a blue dot default browser promo will be shown to eligible "
    "users.";

const char kDefaultBrowserFullscreenPromoExperimentName[] =
    "Default Browser Fullscreen modal experiment";
const char kDefaultBrowserFullscreenPromoExperimentDescription[] =
    "When enabled, will show a modified default browser fullscreen modal promo "
    "UI.";

const char kDefaultBrowserIntentsShowSettingsName[] =
    "Default Browser Intents show settings";
const char kDefaultBrowserIntentsShowSettingsDescription[] =
    "When enabled, external apps can trigger the settings screen showing "
    "default browser tutorial.";

const char kDefaultBrowserRefactoringPromoManagerName[] =
    "Enable the refactoring of the full screen default browser promos to be "
    "included in the promo manager";
const char kDefaultBrowserRefactoringPromoManagerDescription[] =
    "When enabled, the full screen default browser promos will be be included "
    "and managed in the promo manager";

const char kDefaultBrowserVideoPromoName[] =
    "Enable default browser video promo";
const char kDefaultBrowserVideoPromoDescription[] =
    "When enabled, the user will be presented a video promo showing how to set "
    "Chrome as default browser.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kEnableDiscoverFeedTopSyncPromoName[] =
    "Enables the top of feed sync promo.";
const char kEnableDiscoverFeedTopSyncPromoDescription[] =
    "When enabled, a sync promotion will be presented to eligible users on top "
    "of the feed cards.";

const char kEnableFeedHeaderSettingsName[] =
    "Enables the feed header settings.";
const char kEnableFeedHeaderSettingsDescription[] =
    "When enabled, some UI elements of the feed header can be modified.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableAutofillAddressSavePromptAddressVerificationName[] =
    "Autofill Address Save Prompts Address Verification";
const char kEnableAutofillAddressSavePromptAddressVerificationDescription[] =
    "Enable the address verification support in Autofill address save prompts.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableBookmarksAccountStorageName[] =
    "Enable Bookmarks Account Storage";
const char kEnableBookmarksAccountStorageDescription[] =
    "Enable bookmarks account storage and related UI features.";

const char kBrowserLockdownModeAvailableName[] = "Enable Browser Lockdown Mode";
const char kBrowserLockdownModeAvailableDescription[] =
    "Enable browser lockdown mode.";

const char kEnableCBDSignOutName[] = "Enable Clear Browsing Data Sign-out";
const char kEnableCBDSignOutDescription[] =
    "Offer signed-in user to sign-out from Clear Browsing Data settings.";

const char kEnableCheckVisibilityOnAttentionLogStartName[] =
    "Enable Check Feed Visibility On Attention Log Start";
const char kEnableCheckVisibilityOnAttentionLogStartDescription[] =
    "Enable checking feed visibility on attention log start.";

const char kEnableCompromisedPasswordsMutingName[] =
    "Enable the muting of compromised passwords in the Password Manager";
const char kEnableCompromisedPasswordsMutingDescription[] =
    "Enable the compromised password alert mutings in Password Manager to be "
    "respected in the app.";

const char kEnableDiscoverFeedDiscoFeedEndpointName[] =
    "Enable discover feed discofeed";
const char kEnableDiscoverFeedDiscoFeedEndpointDescription[] =
    "Enable using the discofeed endpoint for the discover feed.";

const char kTileAblationName[] = "Enables tile ablation";
const char kTileAblationDescription[] =
    "Hides the shortcuts and most visited tiles on the NTP for new users.";

const char kEnableEmailInBookmarksReadingListSnackbarName[] =
    "Enable Email In Bookmark/Reading List Snackbar";
const char kEnableEmailInBookmarksReadingListSnackbarDescription[] =
    "Enable the display of the signed-in account email in the snackbar which "
    "indicates that an item is added to the bookmarks/reading list.";

const char kEnableFeedAblationName[] = "Enables Feed Ablation";
const char kEnableFeedAblationDescription[] =
    "If Enabled the Feed will be removed from the NTP";

const char kEnableFeedBottomSignInPromoName[] =
    "Enable Feed bottom sign-in promotion";
const char kEnableFeedBottomSignInPromoDescription[] =
    "Display a sign-in promotion card at the bottom of the Discover Feed for "
    "signed out users.";

const char kEnableFeedCardMenuSignInPromoName[] =
    "Enable Feed card menu sign-in promotion";
const char kEnableFeedCardMenuSignInPromoDescription[] =
    "Display a sign-in promotion UI when signed out users click on "
    "personalization options within the feed card menu.";

const char kEnableFeedImageCachingName[] = "Enable Feed image caching";
const char kEnableFeedImageCachingDescription[] =
    "If enabled images in the Feed will be cached for the next time the feed "
    "is loaded.";

const char kEnableFeedSyntheticCapabilitiesName[] =
    "Enable Feed synthetic capabilities.";
const char kEnableFeedSyntheticCapabilitiesDescription[] =
    "If enabled synthethic capablities will be used to inform the server of "
    "the client capabilities.";

const char kEnableFullscreenAPIName[] = "Enable Fullscreen API";
const char kEnableFullscreenAPIDescription[] =
    "Enable the Fullscreen API for web content (iOS 16.0+).";

const char kEnableFollowIPHExpParamsName[] =
    "Enable Follow IPH Experiment Parameters";
const char kEnableFollowIPHExpParamsDescription[] =
    "Enable follow IPH experiment parameters.";

const char kEnableFollowManagementInstantReloadName[] =
    "Enable Follow Management Instant Reload";
const char kEnableFollowManagementInstantReloadDescription[] =
    "Enable follow management page instant reloading when being opened.";

const char kPasswordsGroupingName[] =
    "Enable password grouping for the Password Manager";
const char kPasswordsGroupingDescription[] =
    "Group passwords into the same affiliated group in the Password Manager "
    "for the Saved Passwords section";

const char kEnablePasswordsAccountStorageName[] =
    "Enable the account data storage for passwords";
const char kEnablePasswordsAccountStorageDescription[] =
    "Enables storing passwords in a second, Gaia-account-scoped storage for "
    "signed-in but not syncing users";

const char kEnablePreferencesAccountStorageName[] =
    "Enable the account data storage for preferences for syncing users";
const char kEnablePreferencesAccountStorageDescription[] =
    "Enables storing preferences in a second, Gaia-account-scoped storage for "
    "syncing users";

const char kEnablePinnedTabsName[] = "Enable Pinned Tabs";
const char kEnablePinnedTabsDescription[] = "Allows users to pin tabs.";

const char kEnableReadingListAccountStorageName[] =
    "Enable Reading List Account Storage";
const char kEnableReadingListAccountStorageDescription[] =
    "Enable the reading list account storage.";

const char kEnableReadingListSignInPromoName[] =
    "Enable Reading List Sign-in promo";
const char kEnableReadingListSignInPromoDescription[] =
    "Enable the sign-in promo view in the reading list screen.";

const char kEnableRefineDataSourceReloadReportingName[] =
    "Enable Refine Data Source Reload Reporting";
const char kEnableRefineDataSourceReloadReportingDescription[] =
    "Enable refining data source reload reporting when having a very short "
    "attention log";

const char kEnableSignedOutViewDemotionName[] =
    "Enable signed out user view demotion";
const char kEnableSignedOutViewDemotionDescription[] =
    "Enable signed out user view demotion to avoid repeated content for signed "
    "out users.";

const char kEnableSuggestionsScrollingOnIPadName[] =
    "Enable omnibox suggestions scrolling on iPad";
const char kEnableSuggestionsScrollingOnIPadDescription[] =
    "Enable omnibox suggestions scrolling on iPad and disable suggestions "
    "hiding on keyboard dismissal.";

const char kEnableUIButtonConfigurationName[] =
    "Enable UIButtonConfiguration Usage";
const char kEnableUIButtonConfigurationDescription[] =
    "Enable UIButtonConfiguration usage for UIButtons.";

const char kEnableUserPolicyName[] = "Enable user policies";
const char kEnableUserPolicyDescription[] =
    "Enable the fetch and application of user policies when synced with a "
    "managed account";

const char kEnableWebChannelsName[] = "Enable WebFeed";
const char kEnableWebChannelsDescription[] =
    "Enable folowing content from web and display Following feed on NTP based "
    "on sites that users followed.";

const char kTailoredSecurityIntegrationName[] =
    "Enable Tailored Security Integration";
const char kTailoredSecurityIntegrationDescription[] =
    "Enable integration between account level enhanced safe browsing and "
    "chrome enhanced safe browsing";

const char kExpandedTabStripName[] = "Enable expanded tabstrip";
const char kExpandedTabStripDescription[] =
    "Enables the new expanded tabstrip. Activated by swiping down the tabstrip"
    " or the toolbar";

const char kFeedBackgroundRefreshName[] = "Enable feed background refresh";
const char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kFeedDisableHotStartRefreshName[] = "Disable hot start feed refresh";
const char kFeedDisableHotStartRefreshDescription[] =
    "Disables all Discover-controlled foregrounding refreshes.";

const char kFeedExperimentTaggingName[] = "Enable Feed experiment tagging";
const char kFeedExperimentTaggingDescription[] =
    "Makes server experiments visible as client-side experiments.";

const char kFeedInvisibleForegroundRefreshName[] =
    "Enable feed invisible foreground refresh";
const char kFeedInvisibleForegroundRefreshDescription[] =
    "Invisible foreground refresh has two variations. The first is when the "
    "Feed is refreshed after the user ends a Feed session, but the app is "
    "still in the foreground (e.g., user switches tabs, user navigates away "
    "from Feed in current tab). The second is when the Feed is refreshed at "
    "the moment the app is backgrounding (e.g., during extended execution "
    "time).";

const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";

const char kFollowingFeedDefaultSortTypeName[] =
    "Following feed default sort type.";
const char kFollowingFeedDefaultSortTypeDescription[] =
    "Sets the default sort type for Following feed content.";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kIdentityStatusConsistencyName[] = "Identity Status Consistency";
const char kIdentityStatusConsistencyDescription[] =
    "If enabled, always show identity status - even for signed-out users";

const char kFullscreenPromosManagerSkipInternalLimitsName[] =
    "Fullscreen Promos Manager (Skip internal Impression Limits)";
const char kFullscreenPromosManagerSkipInternalLimitsDescription[] =
    "When enabled, the internal Impression Limits of the Promos Manager will "
    "be ignored; this is useful for local development.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

extern const char kHideContentSuggestionTilesName[] =
    "Hide content suggestions tiles";
extern const char kHideContentSuggestionTilesDescription[] =
    "Hides content suggestions tiles from the new tab page.";

extern const char kHistorySyncOptInName[] = "History Sync Opt-In";
extern const char kHistorySyncOptInDescription[] =
    "Enables history sync opt-in";

const char kHttpsOnlyModeName[] = "HTTPS-Only Mode Setting";
const char kHttpsOnlyModeDescription[] = "Enables the HTTPS-Only Mode setting";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI.";

const char kIndicateAccountStorageErrorInAccountCellName[] =
    "Indicate Account Storage Error in Account Cell";
const char kIndicateAccountStorageErrorInAccountCellDescription[] =
    "When enabled, the Account Cell indicates the Account"
    " Storage error when Sync is turned OFF";

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

const char kIOSCustomBrowserEditMenuName[] = "Custom browser edit menu";
const char kIOSCustomBrowserEditMenuDescription[] =
    "Use the new API for the WKWebView Edit menu.";

const char kIOSEditMenuPartialTranslateName[] =
    "Enable partial translate in edit menu";
const char kIOSEditMenuPartialTranslateDescription[] =
    "Replace the Apple translate entry in the web edit menu to use Google "
    "Translate instead.";

extern const char kIOSEditMenuSearchWithName[] =
    "Enable Search with in edit menu";
extern const char kIOSEditMenuSearchWithDescription[] =
    "Add an entry to search the web selection with your default search engine.";

extern const char kIOSEditMenuHideSearchWebName[] =
    "Hides Search Web in edit menu";
extern const char kIOSEditMenuHideSearchWebDescription[] =
    "Hides the Search Web entry in edit menu.";

const char kIOSForceTranslateEnabledName[] = "Allow force translate on iOS.";
const char kIOSForceTranslateEnabledDescription[] =
    "Enable the translate feature when language detection failed.";

const char kIOSNewPostRestoreExperienceName[] = "New Post Restore Experience";
const char kIOSNewPostRestoreExperienceDescription[] =
    "When enabled, a prompt will be presented after a device restore to "
    "allow the user to sign in again.";

const char kIOSPasswordCheckupName[] = "Password Checkup";
const char kIOSPasswordCheckupDescription[] =
    "Enables displaying and managing compromised, weak and reused credentials "
    "in the Password Manager.";

const char kIOSPasswordUISplitName[] = "Password Manager UI Split";
const char kIOSPasswordUISplitDescription[] =
    "Splits Password Settings and "
    "Password Manager into two separate UIs.";

const char kIOSSetUpListName[] = "IOS Set Up List";
const char kIOSSetUpListDescription[] =
    "Displays an unobtrusive list of set up tasks on Home for a new user.";

const char kIOSPasswordBottomSheetName[] = "IOS Password Manager Bottom Sheet";
const char kIOSPasswordBottomSheetDescription[] =
    "Enables the display of the password bottom sheet on IOS.";

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

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kLogBreadcrumbsName[] = "Log Breadcrumb Events";
const char kLogBreadcrumbsDescription[] =
    "When enabled, breadcrumb events will be logged.";

const char kMetrickitNonCrashReportName[] = "Metrickit non-crash reports";
const char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

const char kMixedContentAutoupgradeName[] = "Auto-upgrade mixed content";
const char kMixedContentAutoupgradeDescription[] =
    "Enables auto-upgrading of mixed content images, audio and video";

const char kModernTabStripName[] = "Modern TabStrip";
const char kModernTabStripDescription[] =
    "When enabled, the newly implemented tabstrip can be tested.";

const char kMostVisitedTilesName[] = "Most Visited Tiles";
const char kMostVisitedTilesDescription[] =
    "Enables the most visited tiles in the omnibox. Shows most visited "
    "websites in a tile format when the user focuses the omnibox on a search "
    "result page (SRP) or on web.";

const char kMultilineFadeTruncatingLabelName[] =
    "Multiline Fade Truncating Label";
const char kMultilineFadeTruncatingLabelDescription[] =
    "Enable gradient support on FadeTruncatingLabel with multiple lines, the "
    "gradient only will be applied to the last line instead of all lines.";

const char kNativeFindInPageName[] = "Native Find in Page";
const char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

const char kNewNTPOmniboxLayoutName[] = "New NTP Omnibox Layout";
const char kNewNTPOmniboxLayoutDescription[] =
    "Enables the new NTP omnibox layout with leading-edge aligned hint label "
    "and magnifying glass icon.";

const char kNewOverflowMenuName[] = "New Overflow Menu";
const char kNewOverflowMenuDescription[] = "Enables the new overflow menu";

const char kOverflowMenuCustomizationName[] = "Overflow Menu Customization";
const char kOverflowMenuCustomizationDescription[] =
    "Allow users to customize the order of the overflow menu";

const char kNTPViewHierarchyRepairName[] = "NTP View Hierarchy Repair";
const char kNTPViewHierarchyRepairDescription[] =
    "Checks if NTP view hierarchy is broken and fixes it if necessary.";

const char kOmniboxFuzzyUrlSuggestionsName[] = "Omnibox Fuzzy URL Suggestions";
const char kOmniboxFuzzyUrlSuggestionsDescription[] =
    "Enables URL suggestions for inputs that may contain typos.";

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

const char kOmniboxKeyboardPasteButtonName[] = "Omnibox keyboard paste button";
const char kOmniboxKeyboardPasteButtonDescription[] =
    "Enables paste button in the omnibox's keyboard accessory. Only available "
    "from iOS 16 onward.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxMaxZPSMatchesName[] = "Omnibox Max ZPS Matches";
const char kOmniboxMaxZPSMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "zero-prefix state in the omnibox (e.g. on NTP when tapped on OB).";

const char kOmniboxMostVisitedTilesOnSrpName[] =
    "Omnibox Most Visited Tiles on Search Results Page";
const char kOmniboxMostVisitedTilesOnSrpDescription[] =
    "Offer most visited website tiles when the User is on the Search Results "
    "Page.";

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

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "Limit the number of URL suggestions in the omnibox. The omnibox will "
    "still display more than MaxURLMatches if there are no non-URL suggestions "
    "to replace them.";

const char kOmniboxNewImplementationName[] =
    "Use experimental omnibox textfield";
const char kOmniboxNewImplementationDescription[] =
    "Uses a textfield implementation that doesn't use UILabels internally";

const char kOmniboxFocusTriggersContextualWebZeroSuggestName[] =
    "Omnibox on-focus suggestions for the contextual Web";
const char kOmniboxFocusTriggersContextualWebZeroSuggestDescription[] =
    "Enables on-focus suggestions on the Open Web, that are contextual to the "
    "current URL. Will only work if user is signed-in and syncing, or is "
    "otherwise eligible to send the current page URL to the suggest server.";

const char kOmniboxFocusTriggersSRPZeroSuggestName[] =
    "Allow Omnibox contextual web on-focus suggestions on the SRP";
const char kOmniboxFocusTriggersSRPZeroSuggestDescription[] =
    "Enables on-focus suggestions on the Search Results page. Requires "
    "on-focus suggestions for the contextual web to be enabled. Will only work "
    "if user is signed-in and syncing.";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

const char kOmniboxMultilineSearchSuggestName[] =
    "Omnibox Multiline Search Suggestion";
const char kOmniboxMultilineSearchSuggestDescription[] =
    "Change the maximum number of line displayed for a search suggestion";

const char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
const char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

const char kOmniboxReportAssistedQueryStatsName[] =
    "Omnibox Assisted Query Stats param";
const char kOmniboxReportAssistedQueryStatsDescription[] =
    "Enables reporting the Assisted Query Stats param in search destination "
    "URLs originated from the Omnibox.";

const char kOmniboxReportSearchboxStatsName[] =
    "Omnibox Searchbox Stats proto param";
const char kOmniboxReportSearchboxStatsDescription[] =
    "Enables reporting the serialized Searchbox Stats proto param in search "
    "destination URLs originated from the Omnibox.";

const char kOmniboxTailSuggestName[] = "Omnibox Tail suggestions";
const char kOmniboxTailSuggestDescription[] =
    "Enables tail search suggestions. Search suggestions only matching the end "
    "of users input text.";

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
    "Only accesses the clipboard asnchronously.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kOptimizationGuideInstallWideModelStoreName[] =
    "Enables the new optimization guide install-wide model store";
const char kOptimizationGuideInstallWideModelStoreDescription[] =
    "Enables the new model store that is per Chrome installation and can "
    "share models across user profiles.";

const char kOptimizationGuidePushNotificationClientName[] =
    "Enable optimization guide push notification client";
const char kOptimizationGuidePushNotificationClientDescription[] =
    "Enables the client that handles incoming push notifications on behalf of "
    "the optimization guide.";

const char kPasswordNotesWithBackupName[] = "Password notes in settings";
const char kPasswordNotesWithBackupDescription[] =
    "Enables a note section for each password in the settings page.";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kPolicyLogsPageIOSName[] = "Policy Logs Page on IOS";
const char kPolicyLogsPageIOSDescription[] =
    "Enable the new chrome://policy/logs page containing logs for debugging "
    "policy related issues on IOS.";

const char kPromosManagerUsesFETName[] = "Promos Manager using FET";
const char kPromosManagerUsesFETDescription[] =
    "Migrates the Promos Manager to use the Feature Engagement Tracker as its "
    "impression tracking system";

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

const char kRecordSnapshotSizeName[] =
    "Record the size of image and PDF snapshots in UMA histograms";
const char kRecordSnapshotSizeDescription[] =
    "When enabled, the app will record UMA histograms for image and PDF "
    "snapshots. PDF snaphot will be taken just for the purpose of the "
    "histogram recording.";

const char kRemoveExcessNTPsExperimentName[] = "Remove extra New Tab Pages";
const char kRemoveExcessNTPsExperimentDescription[] =
    "When enabled, extra tabs with the New Tab Page open and no navigation "
    "history will be removed.";

const char kReplaceSyncPromosWithSignInPromosName[] =
    "Replace all sync-related UI with sign-in ones";
const char kReplaceSyncPromosWithSignInPromosDescription[] =
    "When enabled, all sync-related promos will be replaced by sign-in ones.";

const char kRestoreSessionFromCacheName[] =
    "Use native WKWebView sesion restoration (iOS15 only).";
const char kRestoreSessionFromCacheDescription[] =
    "Enable instant session restoration for faster web session restoration "
    "(iOS15 only).";

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

const char kSendTabToSelfSigninPromoName[] = "Send tab to self sign-in promo";
const char kSendTabToSelfSigninPromoDescription[] =
    "Enables a sign-in promo if the user attempts to share a tab while being "
    "signed out";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowInactiveTabsCountName[] =
    "Show Inactive Tabs count in Tab Grid";
const char kShowInactiveTabsCountDescription[] =
    "When enabled, the count of Inactive Tabs is shown in the Inactive Tabs "
    "button that appears in the Tab Grid.";

const char kSmartSortingPriceTrackingDestinationName[] =
    "Price Tracking destination (with Smart Sorting)";
const char kSmartSortingPriceTrackingDestinationDescription[] =
    "Adds the Price Tracking destination (with Smart Sorting) to the "
    "new overflow menu.";

const char kSpotlightReadingListSourceName[] = "Show Reading List in Spotlight";
const char kSpotlightReadingListSourceDescription[] =
    "Donate Reading List items to iOS Search Engine Spotlight";

const char kNewOverflowMenuShareChromeActionName[] =
    "Share Chrome App action in the new overflow menu";
const char kNewOverflowMenuShareChromeActionDescription[] =
    "Enables the Share Chrome App action in the new overflow menu.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncSegmentsDataName[] = "Use synced segments data";
const char kSyncSegmentsDataDescription[] =
    "Enables history's segments to include foreign visits from syncing "
    "devices.";

const char kSynthesizedRestoreSessionName[] =
    "Use a synthesized native WKWebView sesion restoration (iOS15 only).";
const char kSynthesizedRestoreSessionDescription[] =
    "Enable instant session restoration by synthesizing WKWebView session "
    "restoration data (iOS15 only).";

const char kSyncEnableHistoryDataTypeName[] = "Enable History sync data type";
const char kSyncEnableHistoryDataTypeDescription[] =
    "Enables the History sync data type instead of TypedURLs";

const char kSyncInvalidationsName[] = "Use Sync standalone invalidations";
const char kSyncInvalidationsDescription[] =
    "If enabled, Sync will use standalone invalidations instead of topic based "
    "invalidations (Wallet and Offer data types are enabled by a dedicated "
    "flag).";

const char kSyncInvalidationsWalletAndOfferName[] =
    "Use Sync standalone invalidations for Wallet and Offer";
const char kSyncInvalidationsWalletAndOfferDescription[] =
    "If enabled, Sync will use standalone invalidations for Wallet and Offer "
    "data types. Takes effect only when Sync standalone invalidations are "
    "enabled.";

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

const char kTFLiteLanguageDetectionIgnoreName[] =
    "Ignore TFLite-based Language Detection";
const char kTFLiteLanguageDetectionIgnoreDescription[] =
    "Computes the TFLite language detection but ignore the result and uses the "
    "CLD3 detection instead.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kEnableLensInHomeScreenWidgetName[] =
    "Enable Google Lens in the Home Screen Widget";
const char kEnableLensInHomeScreenWidgetDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "home screen widget.";

const char kEnableLensInKeyboardName[] =
    "Enable Google Lens in the Omnibox Keyboard";
const char kEnableLensInKeyboardDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "omnibox keyboard.";

const char kEnableLensInNTPName[] = "Enable Google Lens in the NTP";
const char kEnableLensInNTPDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "new tab page.";

const char kEnableLensContextMenuAltTextName[] =
    "Enable alternate text for Google Lens in Context Menu";
const char kEnableLensContextMenuAltTextDescription[] =
    "When enabled, use the alternate text for the search image with Google "
    "Lens context menu string.";

const char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
const char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

const char kEnableSessionSerializationOptimizationsName[] =
    "Session Serialization Optimization";
const char kEnableSessionSerializationOptimizationsDescription[] =
    "Enables the use of multiple separate files to save the session state "
    "and the ability to load only the minimum amount of data when restoring "
    "the session from disk.";

const char kSFSymbolsFollowUpName[] = "SF Symbol follow up";
const char kSFSymbolsFollowUpDescription[] = "Change the + button.";

const char kTabGridRecencySortName[] = "Change TabGrid sorting";
const char kTabGridRecencySortDescription[] =
    "When enabled, the tabs in the Tab Grid are sorted differently.";

const char kTabGridNewTransitionsName[] = "Enable new TabGrid transitions";
const char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

const char kTabInactivityThresholdName[] = "Change Tab inactivity threshold";
const char kTabInactivityThresholdDescription[] =
    "When enabled, the tabs older than the threshold are considered inactive "
    "and set aside in the Inactive Tabs section of the TabGrid."
    "IMPORTANT: If you ever used the in-app settings for Inactive Tabs, this "
    "flag is never read again.";

const char kUseLoadSimulatedRequestForOfflinePageName[] =
    "Use loadSimulatedRequest:responseHTMLString: when displaying offline "
    "pages";
const char kUseLoadSimulatedRequestForOfflinePageDescription[] =
    "When enabled, the offline pages uses the iOS 15 "
    "loadSimulatedRequest:responseHTMLString: API";

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

const char kWhatsNewIOSName[] = "Enable What's New.";
const char kWhatsNewIOSDescription[] =
    "When enabled, What's New will display new features and chrome tips.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

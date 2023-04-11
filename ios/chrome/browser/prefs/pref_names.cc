// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/pref_names.h"

namespace prefs {

// The application locale.
const char kApplicationLocale[] = "intl.app_locale";

// Boolean that is true when the AppStoreRatingEnabled policy is enabled.
const char kAppStoreRatingPolicyEnabled[] = "ios.app_store_rating_enabled";

// Boolean that is true when Suggest support is enabled.
const char kArticlesForYouEnabled[] = "suggestions.articles_enabled";

// A map of browser state data directory to cached information. This cache can
// be used to display information about browser states without actually having
// to load them.
const char kBrowserStateInfoCache[] = "profile.info_cache";

// Directory of the browser state profile used.
const char kBrowserStateLastUsed[] = "profile.last_used";

// List of directories of the browser states last active.
const char kBrowserStatesLastActive[] = "profile.last_active_profiles";

// Total number of browser states created for this Chrome build. Used to tag
// browser states directories.
const char kBrowserStatesNumCreated[] = "profile.profiles_created";

// Boolean which indicates whether browsing data migration is/was possible in
// this or a previous cold start.
const char kBrowsingDataMigrationHasBeenPossible[] =
    "ios.browsing_data_migration_controller.migration_has_been_possible";

const char kClearBrowsingDataHistoryNoticeShownTimes[] =
    "browser.clear_data.history_notice_shown_times";

// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
const char kDefaultCharset[] = "intl.charset_default";

// Whether to send the DNT header.
const char kEnableDoNotTrack[] = "enable_do_not_track";

// Number of times the First Follow UI has been shown.
const char kFirstFollowUIShownCount[] = "follow.first_follow_ui_modal_count";

// A dictionary mapping push notification enabled features to their permission
// to send notifications to the user.
const char kFeaturePushNotificationPermissions[] =
    "push_notifications.feature_permissions";

// Prefs for persisting HttpServerProperties.
const char kHttpServerProperties[] = "net.http_server_properties";

// User preferred time for inactivity delay:
// * if == -1: Disabled by user.
// * if >= 1: Inactivity days threshold.
// * Otherwise: Default value driven by Finch config.
const char kInactiveTabsTimeThreshold[] = "ios.inactive_tabs.time_threshold";

// Boolean that is true when the Incognito interstitial for third-party intents
// is enabled.
const char kIncognitoInterstitialEnabled[] =
    "ios.settings.incognito_interstitial_enabled";

// Integer that maps to IOSCredentialProviderPromoSource, the enum type of the
// event that leads to the credential provider promo's display.
const char kIosCredentialProviderPromoSource[] =
    "ios.credential_provider_promo.source";

// Caches the folder id of user's position in the bookmark hierarchy navigator.
const char kIosBookmarkCachedFolderId[] = "ios.bookmark.cached_folder_id";

// Caches the scroll position of Bookmarks.
const char kIosBookmarkCachedTopMostRow[] = "ios.bookmark.cached_top_most_row";

// Preference that keep information about where to create a new bookmark.
const char kIosBookmarkFolderDefault[] = "ios.bookmark.default_folder";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in bookmark view.
const char kIosBookmarkPromoAlreadySeen[] = "ios.bookmark.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the bookmark view.
const char kIosBookmarkSigninPromoDisplayedCount[] =
    "ios.bookmark.signin_promo_displayed_count";

// Boolean to represent if the Bring Android Tabs prompt has been displayed for
// the user.
const char kIosBringAndroidTabsPromptDisplayed[] =
    "ios.bring_android_tabs.prompt_displayed";

// Boolean that is true when the CredentialProviderPromoEnabled policy is
// enabled.
const char kIosCredentialProviderPromoPolicyEnabled[] =
    "ios.credential_provider_promo_policy";

// Boolean to represent if the Credential Provider Promo should stop displaying
// the promo for the user.
const char kIosCredentialProviderPromoStopPromo[] =
    "ios.credential_provider_promo.stop_promo";

// Boolean to represent if the Credential Provider Promo has registered with
// Promo Manager.
const char kIosCredentialProviderPromoHasRegisteredWithPromoManager[] =
    "ios.credential_provider_promo.has_registered_with_promo_manager";

// The time when the DiscoverFeed was last refreshed while the feed was visible
// to the user.
const char kIosDiscoverFeedLastRefreshTime[] =
    "ios.discover_feed.last_refresh_time";

// The time when the DiscoverFeed was last refreshed while the feed was not
// visible to the user.
const char kIosDiscoverFeedLastUnseenRefreshTime[] =
    "ios.discover_feed.last_unseen_refresh_time";

// The user's account info from before a device restore.
const char kIosPreRestoreAccountInfo[] = "ios.pre_restore_account_info";

// List preference maintaining the list of continuous-display, active promo
// campaigns.
const char kIosPromosManagerActivePromos[] = "ios.promos_manager.active_promos";

// Dict preference maintaining the dict of single-display, pending promo
// campaigns. Key is the promo name, value is the time to become active.
const char kIosPromosManagerSingleDisplayPendingPromos[] =
    "ios.promos_manager.pending_promos";

// List preference containing the promo impression history.
const char kIosPromosManagerImpressions[] = "ios.promos_manager.impressions";

// List preference maintaining the list of single-display, active promo
// campaigns.
const char kIosPromosManagerSingleDisplayActivePromos[] =
    "ios.promos_manager.single_display_active_promos";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in settings view.
const char kIosSettingsPromoAlreadySeen[] = "ios.settings.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the settings view.
const char kIosSettingsSigninPromoDisplayedCount[] =
    "ios.settings.signin_promo_displayed_count";

// The count of how many times the user has shared the app.
const char kIosShareChromeCount[] = "ios.share_chrome.count";

// Preference to store the last time the user shared the chrome app.
const char kIosShareChromeLastShare[] = "ios.share_chrome.last_share";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in the ntp feed top section.
const char kIosNtpFeedTopPromoAlreadySeen[] =
    "ios.ntp_feed_top.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the ntp feed top section.
const char kIosNtpFeedTopSigninPromoDisplayedCount[] =
    "ios.ntp_feed_top.signin_promo_displayed_count";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in the reading list.
const char kIosReadingListPromoAlreadySeen[] =
    "ios.reading_list.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the reading list view.
const char kIosReadingListSigninPromoDisplayedCount[] =
    "ios.reading_list.signin_promo_displayed_count";

// Preference that holds a boolean indicating whether the link previews are
// enabled. Link previews display a live preview of the selected link after a
// long press.
const char kLinkPreviewEnabled[] = "ios.link_preview_enabled";

// Preference that holds a boolean indicating whether the suggestions on the NTP
// are enabled.
const char kNTPContentSuggestionsEnabled[] =
    "ios.ntp.content_suggestions_enabled";

// Preference that holds a boolean indicating whether suggestions for supervised
// users on the NTP are enabled.
const char kNTPContentSuggestionsForSupervisedUserEnabled[] =
    "ios.ntp.supervised.content_suggestions_enabled";

// Preference that represents the sorting order of the Following feed content.
const char kNTPFollowingFeedSortType[] = "ios.ntp.following_feed.sort_type";

// Preference that determines if the user changed the Following feed sort type.
const char kDefaultFollowingFeedSortTypeChanged[] =
    "ios.ntp.following_feed_default_sort_type_changed";

// Dictionary preference which tracks day(s) a given destination is clicked from
// the new overflow menu carousel.
const char kOverflowMenuDestinationUsageHistory[] =
    "overflow_menu.destination_usage_history";

// List preference which tracks new destinations added to the overflow menu
// carousel.
const char kOverflowMenuNewDestinations[] = "overflow_menu.new_destinations";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// Boolean indicating if displaying price drops for shopping URLs on Tabs
// in the Tab Switching UI is enabled.
const char kTrackPricesOnTabsEnabled[] = "track_prices_on_tabs.enabled";

// Boolean indicating if Lens camera assited searches are allowed by enterprise
// policy.
const char kLensCameraAssistedSearchPolicyAllowed[] =
    "ios.lens_camera_assited_search_policy.allowed";

// An integer set to one of the NetworkPredictionSetting enum values indicating
// network prediction settings.
const char kNetworkPredictionSetting[] =
    "ios.prerender.network_prediction_settings";

// Which bookmarks folder should be visible on the new tab page v4.
const char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";

// True if the memory debugging tools should be visible.
const char kShowMemoryDebuggingTools[] = "ios.memory.show_debugging_tools";

// List which contains the last known list of accounts.
const char kSigninLastAccounts[] = "ios.signin.last_accounts";

// Boolean which indicates if the pref which contains the last known list of
// accounts was migrated to use account ids instead of emails.
const char kSigninLastAccountsMigrated[] = "ios.signin.last_accounts_migrated";

// Boolean which indicates if user should be prompted to sign in again
// when a new tab is created.
const char kSigninShouldPromptForSigninAgain[] =
    "ios.signin.should_prompt_for_signin_again";

// Number of times the user dismissed the web sign-in dialog. This value is
// reset to zero when the user signs in (using the web sign-in dialog).
const char kSigninWebSignDismissalCount[] =
    "ios.signin.web_signin_dismissal_count";

// Dictionary which stores the zoom levels the user has changed. The zoom levels
// are unique for a given (iOS Dynamic Type, website domain) pair. Thus, the
// dictionary keys are the iOS Dynamic Type level, mapping to sub-dictionarys
// keyed by domain. The final values are double values representing the user
// zoom level (i.e. 1 means no change, 100%).
const char kIosUserZoomMultipliers[] = "ios.user_zoom_multipliers";

const char kPrintingEnabled[] = "printing.enabled";

// Bool used for the incognito biometric authentication setting.
const char kIncognitoAuthenticationSetting[] =
    "ios.settings.incognito_authentication_enabled";

// Integer that represents the value of BrowserSigninPolicy. Values are defined
// in ios/chrome/browser/policy/policy_util.h.
const char kBrowserSigninPolicy[] = "signin.browser_signin_policy";

// Bool that represents whether iCloud backups are allowed by policy.
const char kAllowChromeDataInBackups[] = "ios.allow_chrome_data_in_backups";

// Preference that holds the string value indicating the NTP URL to use for the
// NTP Location policy.
const char kNewTabPageLocationOverride[] = "ios.ntp.location_override";

// A boolean specifying whether HTTPS-Only Mode is enabled.
const char kHttpsOnlyModeEnabled[] = "ios.https_only_mode_enabled";

// A boolean specifying whether Mixed Content Autoupgrading is enabled.
const char kMixedContentAutoupgradeEnabled[] =
    "ios.mixed_content_autoupgrade_enabled";

// An int counting the remaining number of times the autofill branding icon
// should show inside form input accessories.
const char kAutofillBrandingIconAnimationRemainingCountPrefName[] =
    "ios.autofill.branding.animation.remaining_count";

// A boolean used to determine if the Price Tracking UI has been shown.
const char kPriceNotificationsHasBeenShown[] =
    "ios.price_notifications.has_been_shown";

}  // namespace prefs

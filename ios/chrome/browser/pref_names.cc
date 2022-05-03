// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/pref_names.h"

namespace prefs {

// The application locale.
const char kApplicationLocale[] = "intl.app_locale";

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

// Boolean that is true when Data Saver is enabled.
// TODO(bengr): Migrate the preference string to "data_saver.enabled"
// (crbug.com/564207).
const char kDataSaverEnabled[] = "spdy_proxy.enabled";

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
const char kDefaultCharset[] = "intl.charset_default";

// Whether to send the DNT header.
const char kEnableDoNotTrack[] = "enable_do_not_track";

// Prefs for persisting HttpServerProperties.
const char kHttpServerProperties[] = "net.http_server_properties";

// Integer that specifies whether Incognito mode is:
// 0 - Enabled. Default behaviour. Default mode is available on demand.
// 1 - Disabled. User cannot browse pages in Incognito mode.
// 2 - Forced. All pages/sessions are forced into Incognito.
const char kIncognitoModeAvailability[] = "incognito.mode_availability";

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

// The time when the DiscoverFeed was last refreshed.
const char kIosDiscoverFeedLastRefreshTime[] =
    "ios.discover_feed.last_refresh_time";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in settings view.
const char kIosSettingsPromoAlreadySeen[] = "ios.settings.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the settings view.
const char kIosSettingsSigninPromoDisplayedCount[] =
    "ios.settings.signin_promo_displayed_count";

// Preference that holds a boolean indicating whether the link previews are
// enabled. Link previews display a live preview of the selected link after a
// long press.
const char kLinkPreviewEnabled[] = "ios.link_preview_enabled";

// Preference that holds a boolean indicating whether the suggestions on the NTP
// are enabled.
const char kNTPContentSuggestionsEnabled[] =
    "ios.ntp.content_suggestions_enabled";

// Preference that represents the sorting order of the Following feed content.
const char kNTPFollowingFeedSortType[] = "ios.ntp.following_feed.sort_type";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// Boolean indicating if displaying price drops for shopping URLs on Tabs
// in the Tab Switching UI is enabled.
const char kTrackPricesOnTabsEnabled[] = "track_prices_on_tabs.enabled";

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

}  // namespace prefs

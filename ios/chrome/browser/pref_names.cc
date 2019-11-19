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

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in settings view.
const char kIosSettingsPromoAlreadySeen[] = "ios.settings.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the settings view.
const char kIosSettingsSigninPromoDisplayedCount[] =
    "ios.settings.signin_promo_displayed_count";

// True if the previous session exited cleanly.
// This can be different from kStabilityExitedCleanly, because the last run of
// the program may not have included a browsing session, and thus the last run
// of the program may have happened after the run that included the last
// session.
const char kLastSessionExitedCleanly[] =
    "ios.user_experience_metrics.last_session_exited_cleanly";

// Preference that hold a boolean indicating whether metrics reporting should
// be limited to wifi (when enabled).
const char kMetricsReportingWifiOnly[] =
    "ios.user_experience_metrics.wifi_only";

// Which page should be visible on the new tab page v4
const char kNtpShownPage[] = "ntp.shown_page";

// Boolean controlling whether history saving is disabled.
const char kSavingBrowserHistoryDisabled[] = "history.saving_disabled";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// A boolean pref set to true if prediction of network actions is allowed.
// Actions include prerendering of web pages.
// NOTE: The "dns_prefetching.enabled" value is used so that historical user
// preferences are not lost.
const char kNetworkPredictionEnabled[] = "dns_prefetching.enabled";

// Preference that hold a boolean indicating whether network prediction should
// be limited to wifi (when enabled).
const char kNetworkPredictionWifiOnly[] = "ios.dns_prefetching.wifi_only";

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

// Integer which indicates whether the user has authorized using geolocation
// for Omnibox queries or the progress towards soliciting the user's
// authorization.
const char kOmniboxGeolocationAuthorizationState[] =
    "ios.omnibox.geolocation_authorization_state";

// String which contains the application version when we last showed the
// authorization alert.
const char kOmniboxGeolocationLastAuthorizationAlertVersion[] =
    "ios.omnibox.geolocation_last_authorization_alert_version";

}  // namespace prefs

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace prefs {

// Number of times the "Address Bar" settings "new" IPH badge has been shown.
// This is set to INT_MAX when the user visites the "Address Bar" settings page.
const char kAddressBarSettingsNewBadgeShownCount[] =
    "ios.address_bar_settings_new_badge_shown_count";

// The application locale.
const char kApplicationLocale[] = "intl.app_locale";

// Boolean that is true when the AppStoreRatingEnabled policy is enabled.
const char kAppStoreRatingPolicyEnabled[] = "ios.app_store_rating_enabled";

// Boolean that is true when Suggest support is enabled.
const char kArticlesForYouEnabled[] = "suggestions.articles_enabled";

// Boolean which indicates if the omnibox should be at the bottom of the screen.
const char kBottomOmnibox[] = "ios.bottom_omnibox";

// Boolean which indicates if the default value of `kBottomOmnibox` is bottom.
// This saves the default value of the bottom omnibox setting to present the
// omnibox consistently.
const char kBottomOmniboxByDefault[] = "ios.bottom_omnibox_by_default";

// Boolean that is true when Browser Lockdown Mode is enabled.
const char kBrowserLockdownModeEnabled[] = "ios.browser_lockdown_mode_enabled";

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

// The prefs to enable address detection in web pages.
const char kDetectAddressesAccepted[] = "ios.detect_addresses_accepted";
const char kDetectAddressesEnabled[] = "ios.settings.detect_addresses_enabled";

// Whether to send the DNT header.
const char kEnableDoNotTrack[] = "enable_do_not_track";

// Number of times the First Follow UI has been shown.
const char kFirstFollowUIShownCount[] = "follow.first_follow_ui_modal_count";

// Number of times the First Follow UI has been shown with Follow UI Update
// enabled.
const char kFirstFollowUpdateUIShownCount[] =
    "follow.first_follow_update_ui_modal_count";

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

// Preference that keep information about the ID of the node of the last folder
// in which user saved or moved bookmarks. Its value is
// `kLastUsedBookmarkFolderNone` if no folder is explicitly set. The name does
// not reflect the preference key. This is because this preference we used to
// consider this folder to be the "default folder for bookmark". Today, we
// instead consider the "default folder" to be the one selected when this
// preference is set to `kLastUsedBookmarkFolderNone`. Related to
// kIosBookmarkLastUsedStorageReceivingBookmarks.
const char kIosBookmarkLastUsedFolderReceivingBookmarks[] =
    "ios.bookmark.default_folder";

// Preference that keep information about the storage type for
// kIosBookmarkLastUsedFolderReceivingBookmarks. The value is based on
// bookmarks::StorageType enum. This value should be ignored if the value of
// `kIosBookmarkLastUsedFolderReceivingBookmarks` preference is
// `kLastUsedBookmarkFolderNone`. Related to
// `kIosBookmarkLastUsedFolderReceivingBookmarks`.
const char kIosBookmarkLastUsedStorageReceivingBookmarks[] =
    "ios.bookmark.bookmark_last_storage_receiving_bookmarks";

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

// Integer to record the last action that a user has taken on the CPE promo.
const char kIosCredentialProviderPromoLastActionTaken[] =
    "ios.credential_provider_promo_last_action_taken";

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

// A list of the latest fetched Most Visited Sites.
const char kIosLatestMostVisitedSites[] = "ios.most_visited_sites";

// Integer representing the number of impressions of the Most Visited Site since
// a freshness signal.
const char kIosMagicStackSegmentationMVTImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.most_visited_sites_freshness";

// Integer representing the number of impressions of the Parcel Tracking module
// since a freshness signal.
const char kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.parcel_tracking_freshness";

// Integer representing the number of impressions of Shortcuts since a freshness
// signal.
const char kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.shortcuts_freshness";

// Integer representing the number of impressions of Safety Check since a
// freshness signal.
const char kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.safety_check_freshness";

const char kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.tab_resumption_freshness";

// Boolean to represent if the parcel tracking opt-in prompt has met its display
// limit for the user. Was previously kIosParcelTrackingOptInPromptDisplayed.
const char kIosParcelTrackingOptInPromptDisplayLimitMet[] =
    "ios.parcel_tracking.opt_in_prompt_displayed";

// Integer that maps to IOSParcelTrackingOptInStatus, the enum type of the
// user's preference for automatically tracking parcels.
const char kIosParcelTrackingOptInStatus[] =
    "ios.parcel_tracking.opt_in_status";

// Boolean to represent if the user has swiped down on the parcel trackinf
// opt-in prompt.
const char kIosParcelTrackingOptInPromptSwipedDown[] =
    "ios.parcel_tracking.opt_in_prompt_swiped_down";

// Boolean to represent if Parcel Tracking is enabled for enterprise users.
const char kIosParcelTrackingPolicyEnabled[] =
    "ios.parcel_tracking.policy_enabled";

// The number of consecutive times the user dismissed the password bottom sheet.
// This gets reset to 0 whenever the user selects a password from the bottom
// sheet or from the keyboard accessory.
const char kIosPasswordBottomSheetDismissCount[] =
    "ios.password_bottom_sheet_dismiss_count";

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

// Time preference containing the last run time of the Safety Check.
const char kIosSafetyCheckManagerLastRunTime[] =
    "ios.safety_check_manager.last_run_time";

// String preference containing the Password Check result from the most recent
// Safety Check run (using the new Safety Check Manager).
const char kIosSafetyCheckManagerPasswordCheckResult[] =
    "ios.safety_check_manager.password_check_result";

// String preference containing the Update Check result from the most recent
// Safety Check run (using the new Safety Check Manager).
const char kIosSafetyCheckManagerUpdateCheckResult[] =
    "ios.safety_check_manager.update_check_result";

// String preference containing the Safe Browsing Check result from the most
// recent Safety Check run (using the new Safety Check Manager).
const char kIosSafetyCheckManagerSafeBrowsingCheckResult[] =
    "ios.safety_check_manager.safe_browsing_check_result";

// String preference containing the default account to use for saving images to
// Google Photos.
const char kIosSaveToPhotosDefaultGaiaId[] =
    "ios.save_to_photos.default_gaia_id";

// Bool preference containing whether to skip the account picker when the user
// saves an image to Google Photos.
const char kIosSaveToPhotosSkipAccountPicker[] =
    "ios.save_to_photos.skip_account_picker";

// Integer preference indicating whether Save to Photos is enabled by enterprise
// policy.
const char kIosSaveToPhotosContextMenuPolicySettings[] =
    "ios.save_to_photos.context_menu_policy";

// Time preference containing the last run time of the Safety Check (via
// Settings).
const char kIosSettingsSafetyCheckLastRunTime[] =
    "ios.settings.safety_check.last_run_time";

// The count of how many times the user has shared the app.
const char kIosShareChromeCount[] = "ios.share_chrome.count";

// Preference to store the last time the user shared the chrome app.
const char kIosShareChromeLastShare[] = "ios.share_chrome.last_share";

// Preference to store the number of times the user opens the New Tab Page
// with foreign history included in segments data (i.e. Most Visited Tiles).
const char kIosSyncSegmentsNewTabPageDisplayCount[] =
    "ios.sync_segments.ntp.display_count";

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

// Boolean that is true when OS Lockdown Mode is enabled for their entire device
// through native iOS settings.
const char kOSLockdownModeEnabled[] = "ios.os_lockdown_mode_enabled";

// Dictionary preference which tracks day(s) a given destination is clicked from
// the new overflow menu carousel.
const char kOverflowMenuDestinationUsageHistory[] =
    "overflow_menu.destination_usage_history";

// Boolean preference that tracks whether the destination usage history feature
// is enabled on the overflow menu.
extern const char kOverflowMenuDestinationUsageHistoryEnabled[] =
    "overflow_menu.destination_usage_history.enabled";

// List preference which tracks new destinations added to the overflow menu
// carousel.
const char kOverflowMenuNewDestinations[] = "overflow_menu.new_destinations";

// List preference which tracks the current order of the overflow menu's
// destinations.
const char kOverflowMenuDestinationsOrder[] =
    "overflow_menu.destinations_order";

// List preference which tracks the current hidden overflow menu destinations.
const char kOverflowMenuHiddenDestinations[] =
    "overflow_menu.hidden_destinations";

// List preference which tracks the currently badged overflow menu destinations.
const char kOverflowMenuDestinationBadgeData[] =
    "overflow_menu.destination_badge_data";

// Dict preference which tracks the current elements and order of the overflow
// menu's actions.
const char kOverflowMenuActionsOrder[] = "overflow_menu.actions_order";

// Boolean that is true when Suggest support is enabled.
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

// Boolean that is true when the TabPickup feature is enabled.
const char kTabPickupEnabled[] = "ios.tab_pickup_enabled";

// The last time a tab pickup banner was displayed.
const char kTabPickupLastDisplayedTime[] = "ios.tab_pickup_last_displayed_time";

// The last URL used to display a tab pickup banner.
const char kTabPickupLastDisplayedURL[] = "ios.tab_pickup_last_displayed_url";

// Boolean indicating if displaying price drops for shopping URLs on Tabs
// in the Tab Switching UI is enabled.
const char kTrackPricesOnTabsEnabled[] = "track_prices_on_tabs.enabled";

// Boolean indicating if Lens camera assited searches are allowed by enterprise
// policy.
const char kLensCameraAssistedSearchPolicyAllowed[] =
    "ios.lens_camera_assited_search_policy.allowed";

// Number of times the NTP Lens button "new" IPH badge has been shown.
// This is set to INT_MAX when the user taps the button.
const char kNTPLensEntryPointNewBadgeShownCount[] =
    "ios.ntp_lens_new_badge_shown_count";

// A boolean specifying whether Web Inspector support is enabled.
const char kWebInspectorEnabled[] = "ios.web_inspector_enabled";

// The pref to enable units detection in web pages.
const char kDetectUnitsEnabled[] = "ios.settings.detect_units_enabled";

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
const char kAutofillBrandingIconAnimationRemainingCount[] =
    "ios.autofill.branding.animation.remaining_count";

// An integer representing the number of times the autofill branding icon has
// displayed.
const char kAutofillBrandingIconDisplayCount[] =
    "ios.autofill.branding.display_count";

// A boolean used to determine if the Price Tracking UI has been shown.
const char kPriceNotificationsHasBeenShown[] =
    "ios.price_notifications.has_been_shown";

// A boolean used to determine if the user has entered the password sharing flow
// from the first run experience screen.
const char kPasswordSharingFlowHasBeenEntered[] =
    "ios.password_sharing.flow_entered";

}  // namespace prefs

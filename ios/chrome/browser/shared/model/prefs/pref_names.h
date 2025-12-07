// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_NAMES_H_

namespace prefs {

// Number of times the "Address Bar" settings "new" IPH badge has been shown.
// This is set to INT_MAX when the user visites the "Address Bar" settings page.
inline constexpr char kAddressBarSettingsNewBadgeShownCount[] =
    "ios.address_bar_settings_new_badge_shown_count";

// The application locale.
inline constexpr char kApplicationLocale[] = "intl.app_locale";

// A dictionary mapping push notification enabled features to their permission
// to send notifications to the user. This is stored in LocalState prefs.
inline constexpr char kAppLevelPushNotificationPermissions[] =
    "push_notifications.app_level_permissions";

// Boolean that is true when the AppStoreRatingEnabled policy is enabled.
inline constexpr char kAppStoreRatingPolicyEnabled[] =
    "ios.app_store_rating_enabled";

// Boolean that is true when Suggest support is enabled.
inline constexpr char kArticlesForYouEnabled[] = "suggestions.articles_enabled";

// Boolean which indicates if the omnibox should be at the bottom of the screen.
inline constexpr char kBottomOmnibox[] = "ios.bottom_omnibox";

// Boolean which indicates if the default value of `kBottomOmnibox` is bottom.
// This saves the default value of the bottom omnibox setting to present the
// omnibox consistently.
inline constexpr char kBottomOmniboxByDefault[] =
    "ios.bottom_omnibox_by_default";

// Boolean that is true when Browser Lockdown Mode is enabled.
inline constexpr char kBrowserLockdownModeEnabled[] =
    "ios.browser_lockdown_mode_enabled";

// A map of sessionID (clientID) to sub-dictionaries of conversationID
// (serverID) and creation timestamp.
inline constexpr char kBwgSessionMap[] = "ios.bwg.session_map";

// Number of times the "BWG" settings "new" IPH badge has been shown.
// This is set to INT_MAX when the user visites the "BWG" settings page.
inline constexpr char kBWGSettingsNewBadgeShownCount[] =
    "ios.bwg_settings_new_badge_shown_count";

// A map of profile data directory to cached information. This cache can
// be used to display information about profiles without actually having
// to load them.
inline constexpr char kProfileInfoCache[] = "profile.info_cache";

// The name of the profile that's used as the "personal" profile (used for
// consumer accounts), as opposed to managed profiles (linked to a managed aka
// Enterprise account).
inline constexpr char kPersonalProfileName[] = "profile.personal";

// Name of the last used profile.
inline constexpr char kLastUsedProfile[] = "profile.last_used";

// A map of a scene and a profile.
inline constexpr char kProfileForScene[] = "ios.multiprofile.profile_for_scene";

// List of profiles' name that has been marked for deletion.
inline constexpr char kProfilesToRemove[] =
    "ios.multiprofile.profiles_marked_for_deletion";

// A string of NSUUID used to access the WebKit storage per Profile.
inline constexpr char kBrowserStateStorageIdentifier[] = "profile.storage_id";

// A map of legacy profile names to their information.
inline constexpr char kLegacyProfileMap[] = "profile.legacy_profiles.map";

// A boolean recording whether the legacy profiles have been marked as such.
inline constexpr char kLegacyProfileHidden[] = "profile.legacy_profiles.hidden";

inline constexpr char kClearBrowsingDataHistoryNoticeShownTimes[] =
    "browser.clear_data.history_notice_shown_times";

// A dictionary mapping content notification enrollment eligibilities. This is
// stored in Profile prefs.
inline constexpr char kContentNotificationsEnrollmentEligibility[] =
    "ios.content_notification.enrollment_eligibility";

// The default character encoding to assume for a web page in the
// absence of MIME charset specification
inline constexpr char kDefaultCharset[] = "intl.charset_default";

// The prefs to enable address detection in web pages.
inline constexpr char kDetectAddressesAccepted[] =
    "ios.detect_addresses_accepted";
inline constexpr char kDetectAddressesEnabled[] =
    "ios.settings.detect_addresses_enabled";

// The pref to enable the Download Auto-deletion system on the device.
inline constexpr char kDownloadAutoDeletionEnabled[] =
    "ios.download.auto_deletion_enabled";

// The pref tracks whether Auto-deletion's IPH has been shown to the user.
inline constexpr char kDownloadAutoDeletionIPHShown[] =
    "ios.download.auto_deletion_iph_shown";

// A list of dictionaries that represent the files scheduled for automatic
// deletion.
inline constexpr char kDownloadAutoDeletionScheduledFiles[] =
    "ios.auto_deletion.scheduled_files";

// A dictionary mapping push notification enabled features to their permission
// to send notifications to the user. This is stored in Profile prefs.
inline constexpr char kFeaturePushNotificationPermissions[] =
    "push_notifications.feature_permissions";

// A list of delivered notification identifiers that have been handled by the
// metrics recorder.
inline constexpr char kHandledDeliveredNotificationIds[] =
    "push_notifications.handled_delivered_notification_ids";

// A boolean indicating if the user has ever switched accounts via an account
// menu triggered from a web flow.
inline constexpr char kHasSwitchedAccountsViaWebFlow[] =
    "ios.signin.has_switched_accounts_via_web_flow";

// Prefs for persisting HttpServerProperties.
inline constexpr char kHttpServerProperties[] = "net.http_server_properties";

// User preferred time for inactivity delay:
// * if == -1: Disabled by user.
// * if >= 1: Inactivity days threshold.
// * Otherwise: Default value driven by Finch config.
inline constexpr char kInactiveTabsTimeThreshold[] =
    "ios.inactive_tabs.time_threshold";

// Boolean that is true when the Incognito interstitial for third-party intents
// is enabled.
inline constexpr char kIncognitoInterstitialEnabled[] =
    "ios.settings.incognito_interstitial_enabled";

// Integer that maps to IOSCredentialProviderPromoSource, the enum type of the
// event that leads to the credential provider promo's display.
inline constexpr char kIosCredentialProviderPromoSource[] =
    "ios.credential_provider_promo.source";

// Caches the folder id of user's position in the bookmark hierarchy navigator.
inline constexpr char kIosBookmarkCachedFolderId[] =
    "ios.bookmark.cached_folder_id";

// Caches the folder’s model of user's position in the bookmark hierarchy
// navigator.
// TODO(crbug.com/346918509): Deprecate this pref, as it is no longer needed
// after a single BookmarkModel instance was adopted on iOS and the node ID
// alone is able to uniquely identify the folder.
inline constexpr char kIosBookmarkCachedFolderModel[] =
    "ios.bookmark.cached_folder_model";

// Caches the scroll position of Bookmarks.
inline constexpr char kIosBookmarkCachedTopMostRow[] =
    "ios.bookmark.cached_top_most_row";

// Preference that keep information about the ID of the node of the last folder
// in which user saved or moved bookmarks. Its value is
// `kLastUsedBookmarkFolderNone` if no folder is explicitly set. The name does
// not reflect the preference key. This is because this preference we used to
// consider this folder to be the "default folder for bookmark". Today, we
// instead consider the "default folder" to be the one selected when this
// preference is set to `kLastUsedBookmarkFolderNone`. Related to
// kIosBookmarkLastUsedStorageReceivingBookmarks.
// TODO(crbug.com/346918509): Deprecate this pref, as it is no longer needed
// after a single BookmarkModel instance was adopted on iOS and the node ID
// alone is able to uniquely identify the folder.
inline constexpr char kIosBookmarkLastUsedFolderReceivingBookmarks[] =
    "ios.bookmark.default_folder";

// Preference that keep information about the storage type for
// kIosBookmarkLastUsedFolderReceivingBookmarks. The value is based on
// BookmarkStorageType enum. This value should be ignored if the value of
// `kIosBookmarkLastUsedFolderReceivingBookmarks` preference is
// `kLastUsedBookmarkFolderNone`. Related to
// `kIosBookmarkLastUsedFolderReceivingBookmarks`.
inline constexpr char kIosBookmarkLastUsedStorageReceivingBookmarks[] =
    "ios.bookmark.bookmark_last_storage_receiving_bookmarks";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in bookmark view.
inline constexpr char kIosBookmarkPromoAlreadySeen[] =
    "ios.bookmark.promo_already_seen";

// Preference that hold a boolean indicating if the user has already dismissed
// the review account settings promo in bookmark view.
inline constexpr char kIosBookmarkSettingsPromoAlreadySeen[] =
    "ios.bookmark.settings_promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the bookmark view.
inline constexpr char kIosBookmarkSigninPromoDisplayedCount[] =
    "ios.bookmark.signin_promo_displayed_count";

// Boolean to represent if the user has uploaded the sync left-behind bookmarks
// from the bookmarks manager.
inline constexpr char kIosBookmarkUploadSyncLeftBehindCompleted[] =
    "ios.bookmark.upload_sync_left_behind_completed";

// Boolean to represent if the Bring Android Tabs prompt has been displayed for
// the user.
inline constexpr char kIosBringAndroidTabsPromptDisplayed[] =
    "ios.bring_android_tabs.prompt_displayed";

// Integer to record the last action that a user has taken on the CPE promo.
inline constexpr char kIosCredentialProviderPromoLastActionTaken[] =
    "ios.credential_provider_promo_last_action_taken";

// The timestamp of the last time the CPE promo was displayed.
inline constexpr char kIosCredentialProviderPromoDisplayTime[] =
    "ios.credential_provider_promo_display_time";

// The timestamp of the last time the user had a successful login with an
// existing saved password.
inline constexpr char kIosSuccessfulLoginWithExistingPassword[] =
    "ios.successful_login_with_existing_password";

// Boolean that is true when the CredentialProviderPromoEnabled policy is
// enabled.
inline constexpr char kIosCredentialProviderPromoPolicyEnabled[] =
    "ios.credential_provider_promo_policy";

// Boolean to represent if the Credential Provider Promo should stop displaying
// the promo for the user.
inline constexpr char kIosCredentialProviderPromoStopPromo[] =
    "ios.credential_provider_promo.stop_promo";

// Boolean to represent if the Credential Provider Promo has registered with
// Promo Manager.
inline constexpr char
    kIosCredentialProviderPromoHasRegisteredWithPromoManager[] =
        "ios.credential_provider_promo.has_registered_with_promo_manager";

// The timestamp of the first time default browser blue dot promo was shown.
inline constexpr char kIosDefaultBrowserBlueDotPromoFirstDisplay[] =
    "ios.default_browser_blue_dot_promo.first_display";

// The last action that the user took when a Default Browser promo was
// presented.
inline constexpr char kIosDefaultBrowserPromoLastAction[] =
    "ios.default_browser_promo.last_action";

// The time when the DiscoverFeed was last refreshed while the feed was visible
// to the user.
inline constexpr char kIosDiscoverFeedLastRefreshTime[] =
    "ios.discover_feed.last_refresh_time";

// The time when the DiscoverFeed was last refreshed while the feed was not
// visible to the user.
inline constexpr char kIosDiscoverFeedLastUnseenRefreshTime[] =
    "ios.discover_feed.last_unseen_refresh_time";

// Boolean to represent if the user has ever met the criteria to be shown the
// Docking Promo. Once true, remains true permanently. Used only when
// `kIOSDockingPromoForEligibleUsersOnly` is enabled.
inline constexpr char kIosDockingPromoEligibilityMet[] =
    "ios.docking_promo.eligibility_met";

// A list of the latest fetched Most Visited Sites.
inline constexpr char kIosLatestMostVisitedSites[] = "ios.most_visited_sites";

// The last saved index of an NTP WebState. Only updated on app background, so
// it does not always reflect the current WebStateList.
inline constexpr char kIOSLastKnownNTPWebStateIndex[] =
    "ios.last_known_ntp_web_state_index";

// Integer representing the number of impressions of the Most Visited Site since
// a freshness signal.
inline constexpr char kIosMagicStackSegmentationMVTImpressionsSinceFreshness[] =
    "ios.magic_stack_segmentation.most_visited_sites_freshness";

// Integer representing the number of impressions of the Parcel Tracking module
// since a freshness signal.
inline constexpr char
    kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.parcel_tracking_freshness";

// Integer representing the number of impressions of the ShopCard module
// since a freshness signal.
inline constexpr char
    kIosMagicStackSegmentationShopCardImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.shop_card_freshness";

// Integer representing the number of impressions of Shortcuts since a freshness
// signal.
inline constexpr char
    kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.shortcuts_freshness";

// Integer representing the number of impressions of Safety Check since a
// freshness signal.
inline constexpr char
    kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.safety_check_freshness";

inline constexpr char
    kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.tab_resumption_freshness";

// Whether to show Google links to GoogleMaps in a native view.
inline constexpr char kIosMiniMapShowNativeMap[] =
    "ios.mini_map.show_native_map";

// The number of consecutive times the user dismissed the credential bottom
// sheet. This gets reset to 0 whenever the user selects a password from the
// bottom sheet or from the keyboard accessory.
inline constexpr char kIosPasswordBottomSheetDismissCount[] =
    "ios.password_bottom_sheet_dismiss_count";

// The number of consecutive times the user dismissed the password generation
// bottom sheet. This gets reset to 0 whenever the user selects the generated
// password from the bottom sheet or from the keyboard accessory.
inline constexpr char kIosPasswordGenerationBottomSheetDismissCount[] =
    "ios.password_generation_bottom_sheet_dismiss_count";

// The user's account info from before a device restore.
inline constexpr char kIosPreRestoreAccountInfo[] =
    "ios.pre_restore_account_info";

// List preference maintaining the list of continuous-display, active promo
// campaigns.
inline constexpr char kIosPromosManagerActivePromos[] =
    "ios.promos_manager.active_promos";

// Dict preference maintaining the dict of single-display, pending promo
// campaigns. Key is the promo name, value is the time to become active.
inline constexpr char kIosPromosManagerSingleDisplayPendingPromos[] =
    "ios.promos_manager.pending_promos";

// List preference maintaining the list of single-display, active promo
// campaigns.
inline constexpr char kIosPromosManagerSingleDisplayActivePromos[] =
    "ios.promos_manager.single_display_active_promos";

// Time preference containing the last run time of the Safety Check.
inline constexpr char kIosSafetyCheckManagerLastRunTime[] =
    "ios.safety_check_manager.last_run_time";

// String preference containing the Password Check result from the most recent
// Safety Check run (using the new Safety Check Manager).
inline constexpr char kIosSafetyCheckManagerPasswordCheckResult[] =
    "ios.safety_check_manager.password_check_result";

// String preference containing the Update Check result from the most recent
// Safety Check run (using the new Safety Check Manager).
inline constexpr char kIosSafetyCheckManagerUpdateCheckResult[] =
    "ios.safety_check_manager.update_check_result";

// String preference containing the Safe Browsing Check result from the most
// recent Safety Check run (using the new Safety Check Manager).
inline constexpr char kIosSafetyCheckManagerSafeBrowsingCheckResult[] =
    "ios.safety_check_manager.safe_browsing_check_result";

// Dictionary preference containing the counts of passwords flagged as
// compromised, dismissed, reused, and weak by the most recent Safety Check run.
inline constexpr char kIosSafetyCheckManagerInsecurePasswordCounts[] =
    "ios.safety_check_manager.insecure_password_counts";

// Time preference containing the timestamp when a Safety Check notification was
// first set to present in the device's notification center and has not been
// interacted with or dismissed.
inline constexpr char kIosSafetyCheckNotificationFirstPresentTimestamp[] =
    "ios.safety_check.notifications.first_present_timestamp";

// Integer preference containing which Safety Check notification type was sent
// last.
inline constexpr char kIosSafetyCheckNotificationsLastSent[] =
    "ios.safety_check_notifications.last_sent";

// Integer preference containing which Safety Check notification type was
// triggered last.
inline constexpr char kIosSafetyCheckNotificationsLastTriggered[] =
    "ios.safety_check_notifications.last_triggered";

// String preference containing the default account to use for saving files to
// Google Drive.
inline constexpr char kIosSaveToDriveDefaultGaiaId[] =
    "ios.save_to_drive.default_gaia_id";

// Integer preference indicating whether Save to Drive is enabled by enterprise
// policy.
inline constexpr char kIosSaveToDriveDownloadManagerPolicySettings[] =
    "ios.save_to_drive.download_manager_policy";

// Integer preference indicating whether Choose from Drive is enabled by
// enterprise policy.
inline constexpr char kIosChooseFromDriveFilePickerPolicySettings[] =
    "ios.choose_from_drive.file_picker_policy";

// Preference to store the current ThemeSpecificsIos for the user's background
// choices.
inline constexpr char kIosSavedThemeSpecificsIos[] =
    "ios.saved_theme_specifics_ios";

// Dictionary pref storing user-uploaded background image path and framing data.
inline constexpr char kIosUserUploadedBackground[] =
    "ios.user_uploaded_background";

// List pref storing recently used NTP backgrounds.
inline constexpr char kIosRecentlyUsedBackgrounds[] =
    "ios.recently_used_backgrounds";

// String preference containing the default account to use for saving images to
// Google Photos.
inline constexpr char kIosSaveToPhotosDefaultGaiaId[] =
    "ios.save_to_photos.default_gaia_id";

// Bool preference containing whether to skip the account picker when the user
// saves an image to Google Photos.
inline constexpr char kIosSaveToPhotosSkipAccountPicker[] =
    "ios.save_to_photos.skip_account_picker";

// Integer preference indicating whether Save to Photos is enabled by enterprise
// policy.
inline constexpr char kIosSaveToPhotosContextMenuPolicySettings[] =
    "ios.save_to_photos.context_menu_policy";

// Time preference containing the last run time of the Safety Check (via
// Settings).
inline constexpr char kIosSettingsSafetyCheckLastRunTime[] =
    "ios.settings.safety_check.last_run_time";

// The count of how many times the user has shared the app.
inline constexpr char kIosShareChromeCount[] = "ios.share_chrome.count";

// Preference to store the last time the user shared the chrome app.
inline constexpr char kIosShareChromeLastShare[] =
    "ios.share_chrome.last_share";

// Preference to store the number of times the user opens the New Tab Page
// with foreign history included in segments data (i.e. Most Visited Tiles).
inline constexpr char kIosSyncSegmentsNewTabPageDisplayCount[] =
    "ios.sync_segments.ntp.display_count";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in the ntp feed top section.
inline constexpr char kIosNtpFeedTopPromoAlreadySeen[] =
    "ios.ntp_feed_top.promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the ntp feed top section.
inline constexpr char kIosNtpFeedTopSigninPromoDisplayedCount[] =
    "ios.ntp_feed_top.signin_promo_displayed_count";

// Preference that hold a boolean indicating if the user has already dismissed
// the sign-in promo in the reading list.
inline constexpr char kIosReadingListPromoAlreadySeen[] =
    "ios.reading_list.promo_already_seen";

// Preference that hold a boolean indicating if the user has already dismissed
// the review account settings promo in the reading list.
inline constexpr char kIosReadingListSettingsPromoAlreadySeen[] =
    "ios.reading_list.settings_promo_already_seen";

// Integer to represent the number of time the sign-in promo has been displayed
// in the reading list view.
inline constexpr char kIosReadingListSigninPromoDisplayedCount[] =
    "ios.reading_list.signin_promo_displayed_count";

// Preference that holds a boolean indicating whether the link previews are
// enabled. Link previews display a live preview of the selected link after a
// long press.
inline constexpr char kLinkPreviewEnabled[] = "ios.link_preview_enabled";

// Preference that stores the user's acceptance of Lens Overlay ToS.
inline constexpr char kLensOverlayConditionsAccepted[] =
    "ios.lens_overlay_conditions_accepted";

// Preference that holds a boolean indicating whether the suggestions on the NTP
// are enabled.
inline constexpr char kNTPContentSuggestionsEnabled[] =
    "ios.ntp.content_suggestions_enabled";

// Preference that holds a boolean indicating whether suggestions for supervised
// users on the NTP are enabled.
inline constexpr char kNTPContentSuggestionsForSupervisedUserEnabled[] =
    "ios.ntp.supervised.content_suggestions_enabled";

// Preference that holds a boolean indicating whether users can customize their
// NTP backgrounds, as determined by enterprise policy.
inline constexpr char kNTPCustomBackgroundEnabledByPolicy[] =
    "ios.ntp.custom_background_enabled_by_policy";

// Preference that determines if the user changed the Following feed sort type.
inline constexpr char kDefaultFollowingFeedSortTypeChanged[] =
    "ios.ntp.following_feed_default_sort_type_changed";

// Boolean that is true when OS Lockdown Mode is enabled for their entire device
// through native iOS settings.
inline constexpr char kOSLockdownModeEnabled[] = "ios.os_lockdown_mode_enabled";

// Dictionary preference which tracks day(s) a given destination is clicked from
// the new overflow menu carousel.
inline constexpr char kOverflowMenuDestinationUsageHistory[] =
    "overflow_menu.destination_usage_history";

// Boolean preference that tracks whether the destination usage history feature
// is enabled on the overflow menu.
extern inline constexpr char kOverflowMenuDestinationUsageHistoryEnabled[] =
    "overflow_menu.destination_usage_history.enabled";

// List preference which tracks new destinations added to the overflow menu
// carousel.
inline constexpr char kOverflowMenuNewDestinations[] =
    "overflow_menu.new_destinations";

// List preference which tracks the current order of the overflow menu's
// destinations.
inline constexpr char kOverflowMenuDestinationsOrder[] =
    "overflow_menu.destinations_order";

// List preference which tracks the current hidden overflow menu destinations.
inline constexpr char kOverflowMenuHiddenDestinations[] =
    "overflow_menu.hidden_destinations";

// List preference which tracks the currently badged overflow menu destinations.
inline constexpr char kOverflowMenuDestinationBadgeData[] =
    "overflow_menu.destination_badge_data";

// Dict preference which tracks the current elements and order of the overflow
// menu's actions.
inline constexpr char kOverflowMenuActionsOrder[] =
    "overflow_menu.actions_order";

// Boolean that is true when Suggest support is enabled.
inline constexpr char kSearchSuggestEnabled[] = "search.suggest_enabled";

// Boolean that is true when the TabPickup feature is enabled.
inline constexpr char kTabPickupEnabled[] = "ios.tab_pickup_enabled";

// Boolean indicating if displaying price drops for shopping URLs on Tabs
// in the Tab Switching UI is enabled.
inline constexpr char kTrackPricesOnTabsEnabled[] =
    "track_prices_on_tabs.enabled";

// Boolean indicating if Lens camera assisted searches are allowed by enterprise
// policy.
inline constexpr char kLensCameraAssistedSearchPolicyAllowed[] =
    "ios.lens_camera_assited_search_policy.allowed";

// Date of the last time the user opened the Lens UI.
inline constexpr char kLensLastOpened[] = "ios.lens.last_opened";

// Date when the Lens Overlay was last presented.
inline constexpr char kLensOverlayLastPresented[] =
    "ios.lens_overlay.last_presented";

// Number of times the NTP Lens button "new" IPH badge has been shown.
// This is set to INT_MAX when the user taps the button.
inline constexpr char kNTPLensEntryPointNewBadgeShownCount[] =
    "ios.ntp_lens_new_badge_shown_count";

// Dict preference indicating what web annotation type is enabled by policy.
inline constexpr char kWebAnnotationsPolicy[] = "ios.web_annotations_policy";

// A boolean specifying whether Web Inspector support is enabled.
inline constexpr char kWebInspectorEnabled[] = "ios.web_inspector_enabled";

// The pref to enable units detection in web pages.
inline constexpr char kDetectUnitsEnabled[] =
    "ios.settings.detect_units_enabled";

// An integer set to one of the NetworkPredictionSetting enum values indicating
// network prediction settings.
inline constexpr char kNetworkPredictionSetting[] =
    "ios.prerender.network_prediction_settings";

// True if user has ever explicitly disabled Send Tab push notifications. Does
// not reflect the current permission state of Send Tab push notifications.
inline constexpr char kSendTabNotificationsPreviouslyDisabled[] =
    "push_notifications.send_tab_push_notifications_disabled";

// True if the memory debugging tools should be visible.
inline constexpr char kShowMemoryDebuggingTools[] =
    "ios.memory.show_debugging_tools";

// Boolean which indicates if user should be prompted to sign in again
// when a new tab is created.
inline constexpr char kSigninShouldPromptForSigninAgain[] =
    "ios.signin.should_prompt_for_signin_again";

// Per-account pref. True if the user has accepted the management dialog during
// signin.
inline constexpr char kSigninHasAcceptedManagementDialog[] =
    "ios.signin.has_accepted_management_dialog";

// Number of times the user dismissed the web sign-in dialog. This value is
// reset to zero when the user signs in (using the web sign-in dialog).
inline constexpr char kSigninWebSignDismissalCount[] =
    "ios.signin.web_signin_dismissal_count";

// Integer preference that stores the number of times the Synced Set Up flow has
// been shown to the user.
inline constexpr char kSyncedSetUpImpressionCount[] =
    "ios.synced_set_up.impression_count";

// Dictionary which stores the zoom levels the user has changed. The zoom levels
// are unique for a given (iOS Dynamic Type, website domain) pair. Thus, the
// dictionary keys are the iOS Dynamic Type level, mapping to sub-dictionarys
// keyed by domain. The final values are double values representing the user
// zoom level (i.e. 1 means no change, 100%).
inline constexpr char kIosUserZoomMultipliers[] = "ios.user_zoom_multipliers";

inline constexpr char kPrintingEnabled[] = "printing.enabled";

// An integer that stores the authorization state push notification permissions
// are in on a user's device.
inline constexpr char kPushNotificationAuthorizationStatus[] =
    "ios.push_notifications.authorization_status";

// Bool used for the incognito biometric authentication setting.
inline constexpr char kIncognitoAuthenticationSetting[] =
    "ios.settings.incognito_authentication_enabled";

// Bool used for the incognito soft lock setting.
inline constexpr char kIncognitoSoftLockSetting[] =
    "ios.settings.incognito_soft_lock_enabled";

// Timestamp tracking the time in which Chrome was last backgrounded for the
// purposes of locking incognito content.
inline constexpr char kLastBackgroundedTime[] = "ios.last_backgrounded_time";

// Timestamp set when a user signs in. It is used for policies that clear data
// on sign-out only for the duration the user was signed in. It is also used for
// user policies that should clear data only from the time of sign-in and not
// for the entire browser. An example of the latter would be when
// `IdleTimeoutActions` policy is set to clear data as a user policy not a
// browser policy.
inline constexpr char kLastSigninTimestamp[] = "signin.last_signin_timestamp";

// Bool that represents whether iCloud backups are allowed by policy.
inline constexpr char kAllowChromeDataInBackups[] =
    "ios.allow_chrome_data_in_backups";

// Preference that holds the string value indicating the NTP URL to use for the
// NTP Location policy.
inline constexpr char kNewTabPageLocationOverride[] =
    "ios.ntp.location_override";

// A boolean specifying whether HTTPS-Only Mode is enabled.
inline constexpr char kHttpsOnlyModeEnabled[] = "ios.https_only_mode_enabled";

// An int counting the remaining number of times the autofill branding icon
// should show inside form input accessories.
inline constexpr char kAutofillBrandingIconAnimationRemainingCount[] =
    "ios.autofill.branding.animation.remaining_count";

// An integer representing the number of times the autofill branding icon has
// displayed.
inline constexpr char kAutofillBrandingIconDisplayCount[] =
    "ios.autofill.branding.display_count";

// A boolean used for the automatically open tab groups from other devices
// setting.
inline constexpr char kAutomaticallyOpenTabGroupsEnabled[] =
    "ios.settings.automatically_open_tab_groups_enabled";

// A boolean used to determine if the Price Tracking UI has been shown.
inline constexpr char kPriceNotificationsHasBeenShown[] =
    "ios.price_notifications.has_been_shown";

// A boolean used to determine if the user has entered the password sharing flow
// from the first run experience screen.
inline constexpr char kPasswordSharingFlowHasBeenEntered[] =
    "ios.password_sharing.flow_entered";

// A time object used to determine when the Notifications promo was last
// dismissed.
inline constexpr char kNotificationsPromoLastDismissed[] =
    "ios.content_notifications.promo_last_dismissed";
// A time object used to determine when the Notifications promo was last shown.
inline constexpr char kNotificationsPromoLastShown[] =
    "ios.content_notifications.promo_last_shown";
// An int used to determine how many times the Notifications promo has been
// shown to the user.
inline constexpr char kNotificationsPromoTimesShown[] =
    "ios.content_notifications.promo_times_shown";
inline constexpr char kNotificationsPromoTimesDismissed[] =
    "ios.content_notifications.promo_times_dismissed";

inline constexpr char kInsecureFormWarningsEnabled[] =
    "ios.insecure_form_warnings_enabled";

// TODO(crbug.com/329381234) Remove once we have a better solution.
// This value is true if the default user agent was changed. To be used
// only when raccoon is enabled.
inline constexpr char kUserAgentWasChanged[] = "UserAgentWasChanged";

// A time object storing the last time size metrics of the documents directory
// were logged.
inline constexpr char kLastApplicationStorageMetricsLogTime[] =
    "LastApplicationStorageMetricsLogTime";

// Count the number of times the Search Engine Choice Screen was skipped
// because the application was started via an external Intent.
inline constexpr char kChoiceScreenSkippedCount[] =
    "ios.search_engine_choice_screen.skip_count";

// Prefs indicating whether Home surface modules are enabled.
inline constexpr char kHomeCustomizationMostVisitedEnabled[] =
    "ios.home_customization.most_visited.enabled";
inline constexpr char kHomeCustomizationMagicStackEnabled[] =
    "ios.home_customization.magic_stack.enabled";

// Prefs indicating whether Magic Stack cards are enabled.
inline constexpr char kHomeCustomizationMagicStackSafetyCheckEnabled[] =
    "ios.home_customization.magic_stack.safety_check.enabled";
inline constexpr char kHomeCustomizationMagicStackTabResumptionEnabled[] =
    "ios.home_customization.magic_stack.tab_resumption.enabled";
inline constexpr char kHomeCustomizationMagicStackTipsEnabled[] =
    "ios.home_customization.magic_stack.tips.enabled";
inline constexpr char
    kHomeCustomizationMagicStackShopCardPriceTrackingEnabled[] =
        "ios.home_customization.magic_stack.shop_card_price_tracking.enabled";
inline constexpr char kHomeCustomizationMagicStackShopCardReviewsEnabled[] =
    "ios.home_customization.magic_stack.shop_card_price_reviews.enabled";

// List preference that stores the positions in the Magic Stack where the Safety
// Check module with the notifications opt-in button is shown.
inline constexpr char kMagicStackSafetyCheckNotificationsShown[] =
    "ios.home_customization.magic_stack.safety_check.notifications_shown";

// Integer preference that stores the most recent count of Safety Check issues
// presented to the user in the Safety Check module (part of the Magic Stack).
inline constexpr char kHomeCustomizationMagicStackSafetyCheckIssuesCount[] =
    "ios.home_customization.magic_stack.safety_check.issues_count";

// A time object storing when the last the identity confirmation snackbar was
// prompted. Used to limit the frequency of this snackbar.
inline constexpr char kIdentityConfirmationSnackbarLastPromptTime[] =
    "ios.identity_confirmation_snackbar_last_prompt_time";

// Integer storing the latest display iteration of the identity confirmation
// snackbar. Used to limit the frequency of this snackbar.
inline constexpr char kIdentityConfirmationSnackbarDisplayCount[] =
    "ios.identity_confirmation_snackbar_display_count";

// The number of times that the new badge has been shown on the Home
// Customization menu's entrypoint.
inline constexpr char kNTPHomeCustomizationNewBadgeImpressionCount[] =
    "ios.home_customization.new_badge_impressions";

// The number of times that the prominence alert about the user's push
// notification silent authorization state has been shown.
inline constexpr char kProminenceNotificationAlertImpressionCount[] =
    "ios.push_notification.prominence_alert_impressions";

// Integer value controlling the data region to store covered data from Chrome.
// By default, no preference is selected.
// - 0: No preference
// - 1: United States
// - 2: Europe
inline constexpr char kChromeDataRegionSetting[] = "chrome_data_region_setting";

// A boolean used to determine if the Youtube Incognito Interstitial sheet has
// been shown.
inline constexpr char kYoutubeIncognitoHasBeenShown[] =
    "ios.youtube_incognito.has_been_shown";

// A dictionary to store reminders that the user has set.
inline constexpr char kReminderNotifications[] = "ios.notifications.reminders";

// A bool checking that keys used to add multi-profile support to widgets are
// set.
inline constexpr char kMigrateWidgetsPrefs[] =
    "ios.widgets.update_to_support_mim";

// A boolean specifying whether provisional notifications are allowed by policy.
inline constexpr char kProvisionalNotificationsAllowedByPolicy[] =
    "ios.notifications.provisional.allowed_by_policy";

// Timestamp tracking when the sync error infobar was dismissed the last time
// (either explicitly swiped by the user or through the dismissal timeout).
inline constexpr char kIosSyncInfobarErrorLastDismissedTimestamp[] =
    "ios.sync_infobar_error.last_dismissed_timestamp";

// A boolean specifying whether the bwg consent form has been accepted.
inline constexpr char kIOSBwgConsent[] = "ios.bwg.consent";

// A boolean specifying whether the BWG precise location setting is enabled.
inline constexpr char kIOSBWGPreciseLocationSetting[] =
    "ios.bwg.precise.location.setting";

// A boolean specifying whether the BWG page content setting is enabled.
inline constexpr char kIOSBWGPageContentSetting[] =
    "ios.bwg.page.content.setting";

// An integer specifying how many times the BWG Promo was shown.
inline constexpr char kIOSBWGPromoImpressionCount[] =
    "ios.bwg.promo_impressions";

// Timestamp tracking the last interaction with the Gemini floaty.
inline constexpr char kLastGeminiInteractionTimestamp[] =
    "ios.gemini.last_interaction_timestamp";

// The URL where the user last had a Gemini interaction.
inline constexpr char kLastGeminiInteractionURL[] =
    "ios.gemini.last_interaction_url";

// Timestamp tracking the last time the Gemini contextual chip was displayed.
inline constexpr char kLastGeminiContextualChipDisplayedTimestamp[] =
    "ios.gemini.last_contextual_chip_displayed";

// A string specifying the active conversation ID.
inline constexpr char kGeminiConversationId[] = "ios.gemini.conversation_id";

// A time object storing the first browser startup with a managed primary
// identity in the personal profile after multi-profile becomes supported. Used
// to trigger forced migration after some grace period.
inline constexpr char kWaitingForMultiProfileForcedMigrationTimestamp[] =
    "ios.waiting_for_multi_profile_forced_migration_timestamp";

// A time object storing when the sign-in promo should be displayed again.
// The value is set on the first cold start to make sure sign-in promo is not
// triggered right after the FRE.
inline constexpr char kNextSSORecallTime[] = "ios.next_sso_recall_time";

// An integer determining the enabled status of Gemini by policy.
// 0 means Gemini is enabled (default), and 1 means it's disabled.
inline constexpr char kGeminiEnabledByPolicy[] = "ios.gemini_enabled_by_policy";

// A boolean specifying whether the user has ever been eligible for AI Hub.
inline constexpr char kAIHubEligibilityTriggered[] =
    "ios.ai_hub_eligibility_triggered";

// A boolean specifying if the multi-profile force-migration is done.
inline constexpr char kMultiProfileForcedMigrationDone[] =
    "ios.multi_profile_forced_migration_done";

// A bool checking that multi-profile support for widgets is available.
inline constexpr char kWidgetsForMultiProfile[] =
    "ios.multi_profile_for_widgets";

// An integer pref to store the placement ID of the acceptance data if the
// install was attributable to the external promo.
inline constexpr char kIOSGMOSKOLastAttributionPlacementID[] =
    "ios.gmosko_last_attribution_placement_id";

// A time pref to store the date after which the placement ID can be logged.
inline constexpr char kIOSGMOSKOPlacementIDNextLogDate[] =
    "ios.gmosko_placement_id_next_log_date";

// An integer storing whether the install attribution was attributable within
// the short window or the long window.
inline constexpr char kIOSGMOSKOLastAttributionWindowType[] =
    "ios.gmosko_last_attribution_window_type";

// An integer pref to store the placement ID of the acceptance data if the
// install was attributable to the external promo from the App Preview.
inline constexpr char kIOSAppPreviewLastAttributionPlacementID[] =
    "ios.app_preview_last_attribution_placement_id";

// A time pref to store the date after which the placement ID can be logged for
// the App Preview.
inline constexpr char kIOSAppPreviewPlacementIDNextLogDate[] =
    "ios.app_preview_placement_id_next_log_date";

// An integer storing whether the install attribution was attributable within
// the short window or the long window for the App Preview.
inline constexpr char kIOSAppPreviewLastAttributionWindowType[] =
    "ios.app_preview_last_attribution_window_type";

// A profile pref for storing a list of timestamps of days the user was active.
inline constexpr char kCrossPlatformPromosActiveDays[] =
    "cross_platform_promos.active_days";

// A profile pref for storing the 16th most recent day the user was active.
// This is calculating by starting at the current day, and iterating back
// through the days that the user was active, and counting until the 16th
// active day is reached. This can be used to determine if the user has been
// active at least 16 out of the last 28 days, by checking if this is more
// recent than 28 days ago.
inline constexpr char kCrossPlatformPromosIOS16thActiveDay[] =
    "cross_platform_promos.ios_16th_active_day";

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_PREF_NAMES_H_

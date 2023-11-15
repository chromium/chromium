// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#import <StoreKit/StoreKit.h>
#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/shopping_service.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/signin/ios/browser/active_state_manager.h"
#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_util.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/java_script_find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/follow/followed_web_site.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/prerender/model/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/autofill_bottom_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/feed_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_protection_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/shared/public/commands/passwords_account_storage_notice_commands.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/page_animation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/account_consistency_browser_agent.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_type.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/browser_view/safe_area_provider.h"
#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"
#import "ios/chrome/browser/ui/browser_view/tab_lifecycle_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_promo_non_modal_presentation_delegate.h"
#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"
#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/download/pass_kit_coordinator.h"
#import "ios/chrome/browser/ui/download/safari_download_coordinator.h"
#import "ios/chrome/browser/ui/download/vcard_coordinator.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"
#import "ios/chrome/browser/ui/follow/follow_iph_coordinator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_coordinator.h"
#import "ios/chrome/browser/ui/mini_map/mini_map_coordinator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_coordinator.h"
#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_coordinator.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_coordinator_delegate.h"
#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_protection_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_protection_coordinator_delegate.h"
#import "ios/chrome/browser/ui/passwords/password_suggestion_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_iph_coordinator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"
#import "ios/chrome/browser/ui/print/print_coordinator.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"
#import "ios/chrome/browser/ui/safe_browsing/safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_mediator.h"
#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_coordinator.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller_factory.h"
#import "ios/chrome/browser/ui/webui/net_export_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/view_source/model/view_source_browser_agent.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web/web_state_update_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Duration of the toolbar animation.
constexpr base::TimeDelta kLegacyFullscreenControllerToolbarAnimationDuration =
    base::Milliseconds(300);

// URL to share when user selects "Share Chrome"
const char kChromeAppStoreUrl[] =
    "https://apps.apple.com/app/id535886823?pt=9008&ct=iosChromeShare&mt=8";

// Enum for toolbar to present.
enum class ToolbarKind {
  kTextZoom,
  kFindInPage,
};

}  // anonymous namespace

@interface BrowserCoordinator () <
    AppLauncherTabHelperBrowserPresentationProvider,
    AutofillAddCreditCardCoordinatorDelegate,
    BrowserCoordinatorCommands,
    BubblePresenterDelegate,
    DefaultBrowserPromoCommands,
    DefaultPromoNonModalPresentationDelegate,
    EnterprisePromptCoordinatorDelegate,
    FormInputAccessoryCoordinatorNavigator,
    MiniMapCommands,
    NetExportTabHelperDelegate,
    NewTabPageCommands,
    OverscrollActionsControllerDelegate,
    PageInfoCommands,
    PageInfoPresentation,
    PasswordBreachCommands,
    PasswordControllerDelegate,
    PasswordProtectionCommands,
    PasswordProtectionCoordinatorDelegate,
    PasswordSettingsCoordinatorDelegate,
    PasswordSuggestionCommands,
    PasswordSuggestionCoordinatorDelegate,
    PasswordSuggestionBottomSheetCoordinatorDelegate,
    PasswordsAccountStorageNoticeCommands,
    PriceNotificationsCommands,
    PromosManagerCommands,
    PolicyChangeCommands,
    PreloadControllerDelegate,
    ReadingListCoordinatorDelegate,
    RecentTabsCoordinatorDelegate,
    RepostFormCoordinatorDelegate,
    RepostFormTabHelperDelegate,
    SaveToPhotosCommands,
    SigninPresenter,
    SnapshotGeneratorDelegate,
    StoreKitCoordinatorDelegate,
    ToolbarAccessoryCoordinatorDelegate,
    UnitConversionCommands,
    URLLoadingDelegate,
    WebContentCommands,
    WebNavigationNTPDelegate,
    ParcelTrackingOptInCommands>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// Whether web usage is enabled for the WebStates in `self.browser`.
@property(nonatomic, assign, getter=isWebUsageEnabled) BOOL webUsageEnabled;

// Handles command dispatching, provided by the Browser instance.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The coordinator managing the container view controller.
@property(nonatomic, strong)
    BrowserContainerCoordinator* browserContainerCoordinator;

// Mediator for incognito reauth.
@property(nonatomic, strong) IncognitoReauthMediator* incognitoAuthMediator;

// Mediator for tab lifecylce.
@property(nonatomic, strong) TabLifecycleMediator* tabLifecycleMediator;

// Mediator for tab events.
@property(nonatomic, strong) TabEventsMediator* tabEventsMediator;

// =================================================
// Child Coordinators, listed in alphabetical order.
// =================================================

// Coordinator for displaying a modal overlay with activity indicator to prevent
// the user from interacting with the browser view.
@property(nonatomic, strong)
    ActivityOverlayCoordinator* activityOverlayCoordinator;

// Coordinator to add new credit card.
@property(nonatomic, strong)
    AutofillAddCreditCardCoordinator* addCreditCardCoordinator;

// Presents a QLPreviewController in order to display USDZ format 3D models.
@property(nonatomic, strong) ARQuickLookCoordinator* ARQuickLookCoordinator;

// Coordinator in charge of the presenting autofill options in a bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetCoordinator*
    passwordSuggestionBottomSheetCoordinator;

// Coordinator in charge of the presenting autofill options in a bottom sheet.
@property(nonatomic, strong) PaymentsSuggestionBottomSheetCoordinator*
    paymentsSuggestionBottomSheetCoordinator;

// Coordinator for the choice screen.
@property(nonatomic, strong) ChromeCoordinator* choiceCoordinator;

// Coordinator-ish provider for context menus.
@property(nonatomic, strong)
    ContextMenuConfigurationProvider* contextMenuProvider;

// Coordinator that manages the default browser promo modal.
@property(nonatomic, strong)
    DefaultBrowserPromoCoordinator* defaultBrowserPromoCoordinator;

// Coordinator that manages the presentation of Download Manager UI.
@property(nonatomic, strong)
    DownloadManagerCoordinator* downloadManagerCoordinator;

// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;

// Coordinator for the find bar.
@property(nonatomic, strong) FindBarCoordinator* findBarCoordinator;

// Coordinator for the First Follow modal.
@property(nonatomic, strong) FirstFollowCoordinator* firstFollowCoordinator;

// Coordinator for the Follow IPH feature.
@property(nonatomic, strong) FollowIPHCoordinator* followIPHCoordinator;

// Coordinator in charge of the presenting autofill options above the
// keyboard.
@property(nonatomic, strong)
    FormInputAccessoryCoordinator* formInputAccessoryCoordinator;

// The container coordinators for the infobar modalities.
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarBannerOverlayContainerCoordinator;
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarModalOverlayContainerCoordinator;

// Coordinator in charge of presenting a mini map.
@property(nonatomic, strong) MiniMapCoordinator* miniMapCoordinator;

// The coordinator that manages net export.
@property(nonatomic, strong) NetExportCoordinator* netExportCoordinator;

// Coordinator for the non-modal default promo.
@property(nonatomic, strong)
    DefaultBrowserPromoNonModalCoordinator* nonModalPromoCoordinator;

// Coordinator for Page Info UI.
@property(nonatomic, strong) ChromeCoordinator* pageInfoCoordinator;

// Coordinator for parcel tracking opt-in UI presentation.
@property(nonatomic, strong)
    ParcelTrackingOptInCoordinator* parcelTrackingOptInCoordinator;

// Coordinator for the PassKit UI presentation.
@property(nonatomic, strong) PassKitCoordinator* passKitCoordinator;

// Coordinator for the password breach UI presentation.
@property(nonatomic, strong)
    PasswordBreachCoordinator* passwordBreachCoordinator;

// Coordinator for the password protection UI presentation.
@property(nonatomic, strong)
    PasswordProtectionCoordinator* passwordProtectionCoordinator;

// Coordinator for the password settings UI presentation.
@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

// Coordinator for the password suggestion UI presentation.
@property(nonatomic, strong)
    PasswordSuggestionCoordinator* passwordSuggestionCoordinator;

// Coordinator for the passwords account storage notice.
@property(nonatomic, strong) PasswordsAccountStorageNoticeCoordinator*
    passwordsAccountStorageNoticeCoordinator;

// Coordinator for the popup menu.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

// Coordinator for the price notifications IPH feature.
@property(nonatomic, strong)
    PriceNotificationsIPHCoordinator* priceNotificationsIPHCoordinator;

// Coordinator for the price notifications UI presentation.
@property(nonatomic, strong)
    PriceNotificationsViewCoordinator* priceNotificationsViewCoordiantor;

// Used to display the Print UI. Nil if not visible.
@property(nonatomic, strong) PrintCoordinator* printCoordinator;

// Coordinator for app-wide promos.
@property(nonatomic, strong) PromosManagerCoordinator* promosManagerCoordinator;

// Coordinator for the QR scanner.
@property(nonatomic, strong) QRScannerLegacyCoordinator* qrScannerCoordinator;

// Coordinator for displaying the Reading List.
@property(nonatomic, strong) ReadingListCoordinator* readingListCoordinator;

// Coordinator for Recent Tabs.
@property(nonatomic, strong) RecentTabsCoordinator* recentTabsCoordinator;

// Coordinator for displaying Repost Form dialog.
@property(nonatomic, strong) RepostFormCoordinator* repostFormCoordinator;

// Coordinator for displaying Sad Tab.
@property(nonatomic, strong) SadTabCoordinator* sadTabCoordinator;

// Presents a SFSafariViewController in order to download .mobileconfig file.
@property(nonatomic, strong)
    SafariDownloadCoordinator* SafariDownloadCoordinator;

// Coordinator for Safe Browsing.
@property(nonatomic, strong) SafeBrowsingCoordinator* safeBrowsingCoordinator;

// Coordinator for sharing scenarios.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// The coordinator used for Spotlight Debugger.
@property(nonatomic, strong)
    SpotlightDebuggerCoordinator* spotlightDebuggerCoordinator;

// Coordinator for presenting SKStoreProductViewController.
@property(nonatomic, strong) StoreKitCoordinator* storeKitCoordinator;

// Coordinator that manages the tailored promo modals.
@property(nonatomic, strong) TailoredPromoCoordinator* tailoredPromoCoordinator;

// The coordinator used for the Text Fragments feature.
@property(nonatomic, strong) TextFragmentsCoordinator* textFragmentsCoordinator;

// Coordinator for Text Zoom.
@property(nonatomic, strong) TextZoomCoordinator* textZoomCoordinator;

// Coordinator in charge of presenting a unit converter.
@property(nonatomic, strong)
    UnitConversionCoordinator* unitConversionCoordinator;

// Opens downloaded Vcard.
@property(nonatomic, strong) VcardCoordinator* vcardCoordinator;

// The coordinator used for What's New feature.
@property(nonatomic, strong) WhatsNewCoordinator* whatsNewCoordinator;

// The manager used to display a default browser promo.
@property(nonatomic, strong)
    DefaultBrowserPromoManager* defaultBrowserPromoManager;

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* activeWebState;

@end

@implementation BrowserCoordinator {
  BrowserViewControllerDependencies _viewControllerDependencies;
  KeyCommandsProvider* _keyCommandsProvider;
  BubblePresenter* _bubblePresenter;
  ToolbarAccessoryPresenter* _toolbarAccessoryPresenter;
  LensCoordinator* _lensCoordinator;
  ToolbarCoordinator* _toolbarCoordinator;
  TabStripCoordinator* _tabStripCoordinator;
  TabStripLegacyCoordinator* _legacyTabStripCoordinator;
  SideSwipeMediator* _sideSwipeMediator;
  FullscreenController* _fullscreenController;
  // The coordinator that shows the Send Tab To Self UI.
  SendTabToSelfCoordinator* _sendTabToSelfCoordinator;
  BookmarksCoordinator* _bookmarksCoordinator;
  absl::optional<ToolbarKind> _nextToolbarToPresent;
  CredentialProviderPromoCoordinator* _credentialProviderPromoCoordinator;
  // Used to display the Voice Search UI.  Nil if not visible.
  id<VoiceSearchController> _voiceSearchController;
  UrlLoadingNotifierBrowserAgent* _urlLoadingNotifierBrowserAgent;
  id<LoadQueryCommands> _loadQueryCommandsHandler;
  id<OmniboxCommands> _omniboxCommandsHandler;
  LayoutGuideCenter* _layoutGuideCenter;
  WebNavigationBrowserAgent* _webNavigationBrowserAgent;
  UrlLoadingBrowserAgent* _urlLoadingBrowserAgent;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;

  DCHECK(!self.viewController);

  // TabLifeCycleMediator should start before createViewController because it
  // needs to register itself as a WebStateListObserver before the rest of the
  // UI in order to be able to install the tab helper delegate before the UI is
  // notified of WebStateList events.
  [self startTabLifeCycleMediator];

  [self createViewControllerDependencies];

  [self createViewController];

  [self updateViewControllerDependencies];

  // Force the view load at a specific time.
  // TODO(crbug.com/1431971): This should ideally go in createViewController,
  // but part of creating the view controller involves setting up a dispatch to
  // a command that isn't handled until updateViewControllerDependencies
  // (OmniboxCommands).
  BOOL created = [self ensureViewIsCreated];
  CHECK(created);

  // Independent mediators should start before coordinators so model state is
  // accurate for any UI that starts up.
  [self startIndependentMediators];

  [self startChildCoordinators];

  // Browser delegates can have dependencies on coordinators.
  [self installDelegatesForBrowser];
  [self installDelegatesForBrowserState];

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;

  self.started = NO;
  [super stop];
  self.active = NO;
  [self uninstallDelegatesForBrowserState];
  [self uninstallDelegatesForBrowser];
  [self.tabEventsMediator disconnect];
  [self.tabLifecycleMediator disconnect];
  [self.dispatcher stopDispatchingToTarget:self];
  [self stopChildCoordinators];
  [self destroyViewController];
  [self destroyViewControllerDependencies];
}

- (void)dealloc {
  DCHECK(!_bookmarksCoordinator);
}

#pragma mark - Public

- (BOOL)isPlayingTTS {
  return _voiceSearchController.audioPlaying;
}

- (void)setActive:(BOOL)active {
  DCHECK_EQ(_active, self.viewController.active);
  if (_active == active) {
    return;
  }
  _active = active;

  // If not active, display an activity indicator overlay over the view to
  // prevent interaction with the web page.
  if (active) {
    [self hideActivityOverlay];
  } else if (!self.activityOverlayCoordinator) {
    [self showActivityOverlay];
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    // TODO(crbug.com/1272520): Refactor ActiveStateManager for multiwindow.
    ActiveStateManager* active_state_manager =
        ActiveStateManager::FromBrowserState(browserState);
    active_state_manager->SetActive(active);

    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetEnabled(active);
  }
  self.webUsageEnabled = active;
  if (active) {
    // If the NTP was stopped because of a -setActive:NO call, then the NTP
    // needs to be restarted when -setActive:YES is called subsequently (i.e.
    // clear browsing data). This should not be needed for any other use case,
    // but on initial startup this is inevitably called after restoring tabs, so
    // cannot assert that it has not been started.
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    if (webState && NewTabPageTabHelper::FromWebState(webState)->IsActive() &&
        !self.NTPCoordinator.started) {
      [self.NTPCoordinator start];
    }
  } else {
    [self.NTPCoordinator stop];
  }
  self.viewController.active = active;
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self stopSaveToPhotos];

  [self.passKitCoordinator stop];

  [self.printCoordinator dismissAnimated:YES];

  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self stopPasswordProtectionCoordinator];

  [self stopPasswordSuggestionBottomSheetCoordinator];
  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.passwordsAccountStorageNoticeCoordinator stop];
  self.passwordsAccountStorageNoticeCoordinator = nil;

  [self.pageInfoCoordinator stop];

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.priceNotificationsViewCoordiantor stop];
  self.priceNotificationsViewCoordiantor = nil;

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;

  [self.viewController clearPresentedStateWithCompletion:completion
                                          dismissOmnibox:dismissOmnibox];
}

#pragma mark - Private

// Stops the password protection coordinator.
- (void)stopPasswordProtectionCoordinator {
  [self.passwordProtectionCoordinator stop];
  self.passwordProtectionCoordinator.delegate = nil;
  self.passwordProtectionCoordinator = nil;
}

- (void)stopAutofillAddCreditCardCoordinator {
  [self.addCreditCardCoordinator stop];
  self.addCreditCardCoordinator.delegate = nil;
  self.addCreditCardCoordinator = nil;
}

- (void)stopRepostFormCoordinator {
  [self.repostFormCoordinator stop];
  self.repostFormCoordinator.delegate = nil;
  self.repostFormCoordinator = nil;
}

// Stops the recent tabs coordinator
- (void)stopRecentTabsCoordinator {
  [self.recentTabsCoordinator stop];
  self.recentTabsCoordinator.delegate = nil;
  self.recentTabsCoordinator = nil;
}

// Stops the password suggestion bottom sheet coordinator.
- (void)stopPasswordSuggestionBottomSheetCoordinator {
  [self.passwordSuggestionBottomSheetCoordinator stop];
  self.passwordSuggestionBottomSheetCoordinator.delegate = nil;
  self.passwordSuggestionBottomSheetCoordinator = nil;
}

// Stop the store kit coordinator.
- (void)stopStoreKitCoordinator {
  [self.storeKitCoordinator stop];
  self.storeKitCoordinator.delegate = nil;
  self.storeKitCoordinator = nil;
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (!self.browser->GetBrowserState() || !self.started) {
    return;
  }
  WebUsageEnablerBrowserAgent::FromBrowser(self.browser)
      ->SetWebUsageEnabled(webUsageEnabled);
  _webUsageEnabled = webUsageEnabled;
  self.viewController.webUsageEnabled = webUsageEnabled;
}

// Displays activity overlay.
- (void)showActivityOverlay {
  self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.activityOverlayCoordinator start];
}

// Hides activity overlay.
- (void)hideActivityOverlay {
  [self.activityOverlayCoordinator stop];
  self.activityOverlayCoordinator = nil;
}

// Shows a default promo with the passed type or nothing if a tailored promo is
// already present.
- (void)showTailoredPromoWithType:(DefaultPromoType)type {
  if (self.tailoredPromoCoordinator) {
    // Another promo is being shown, return early.
    return;
  }
  self.tailoredPromoCoordinator = [[TailoredPromoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            type:type];
  self.tailoredPromoCoordinator.handler = self;
  [self.tailoredPromoCoordinator start];
}

// Instantiates a BrowserViewController.
- (void)createViewController {
  DCHECK(self.browserContainerCoordinator.viewController);

  _viewController = [[BrowserViewController alloc]
      initWithBrowserContainerViewController:self.browserContainerCoordinator
                                                 .viewController
                         keyCommandsProvider:_keyCommandsProvider
                                dependencies:_viewControllerDependencies];
  self.tabLifecycleMediator.baseViewController = self.viewController;
  self.tabLifecycleMediator.passwordControllerDelegate = self;

  _webNavigationBrowserAgent->SetDelegate(self);

  self.contextMenuProvider = [[ContextMenuConfigurationProvider alloc]
         initWithBrowser:self.browser
      baseViewController:_viewController];
}

// Shuts down the BrowserViewController.
- (void)destroyViewController {
  self.viewController.active = NO;
  self.viewController.webUsageEnabled = NO;

  [self.contextMenuProvider stop];
  self.contextMenuProvider = nil;

  // TODO(crbug.com/1415244): Remove when BVC will no longer handle commands.
  [self.dispatcher stopDispatchingToTarget:self.viewController];
  [self.viewController shutdown];
  _viewController = nil;
}

// Ensure BrowserViewController's view is created
- (BOOL)ensureViewIsCreated {
  // Call `-view` for the side effect of creating the view.
  UIView* view = self.viewController.view;
  return view != nil;
}

// Creates the browser view controller dependencies.
- (void)createViewControllerDependencies {
  _dispatcher = self.browser->GetCommandDispatcher();

  // Add commands protocols handled by this class in this array to let the
  // dispatcher know where to dispatch such commands. This must be done before
  // starting any child coordinator, otherwise they won't be able to resolve
  // handlers.
  NSArray<Protocol*>* protocols = @[
    @protocol(ActivityServiceCommands),
    @protocol(AutofillBottomSheetCommands),
    @protocol(BrowserCoordinatorCommands),
    @protocol(DefaultPromoCommands),
    @protocol(DefaultBrowserPromoNonModalCommands),
    @protocol(FeedCommands),
    @protocol(PromosManagerCommands),
    @protocol(FindInPageCommands),
    @protocol(NewTabPageCommands),
    @protocol(PageInfoCommands),
    @protocol(PasswordBreachCommands),
    @protocol(PasswordProtectionCommands),
    @protocol(PasswordSuggestionCommands),
    @protocol(PasswordsAccountStorageNoticeCommands),
    @protocol(PolicyChangeCommands),
    @protocol(PriceNotificationsCommands),
    @protocol(SaveToPhotosCommands),
    @protocol(TextZoomCommands),
    @protocol(WebContentCommands),
    @protocol(DefaultBrowserPromoCommands),
    @protocol(MiniMapCommands),
    @protocol(ParcelTrackingOptInCommands),
    @protocol(UnitConversionCommands),
  ];

  for (Protocol* protocol in protocols) {
    [_dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  _keyCommandsProvider =
      [[KeyCommandsProvider alloc] initWithBrowser:self.browser];
  _keyCommandsProvider.dispatcher =
      static_cast<id<ApplicationCommands, FindInPageCommands>>(_dispatcher);
  _keyCommandsProvider.omniboxHandler =
      static_cast<id<OmniboxCommands>>(_dispatcher);
  _keyCommandsProvider.bookmarksCommandsHandler =
      static_cast<id<BookmarksCommands>>(_dispatcher);
  _keyCommandsProvider.browserCoordinatorCommandsHandler =
      HandlerForProtocol(_dispatcher, BrowserCoordinatorCommands);

  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browserState);
  if (!browserState->IsOffTheRecord()) {
    DCHECK(prerenderService);
    prerenderService->SetDelegate(self);
  }

  _fullscreenController = FullscreenController::FromBrowser(self.browser);
  _layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _webNavigationBrowserAgent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  _urlLoadingBrowserAgent = UrlLoadingBrowserAgent::FromBrowser(self.browser);
  _urlLoadingNotifierBrowserAgent =
      UrlLoadingNotifierBrowserAgent::FromBrowser(self.browser);

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  HostContentSettingsMap* settingsMap =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
  segmentation_platform::DeviceSwitcherResultDispatcher*
      deviceSwitcherResultDispatcher = nullptr;
  if (!browserState->IsOffTheRecord()) {
    deviceSwitcherResultDispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForBrowserState(browserState);
  }

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    if (base::FeatureList::IsEnabled(kModernTabStrip)) {
      _tabStripCoordinator =
          [[TabStripCoordinator alloc] initWithBrowser:self.browser];
    } else {
      _legacyTabStripCoordinator =
          [[TabStripLegacyCoordinator alloc] initWithBrowser:self.browser];
      _legacyTabStripCoordinator.animationWaitDuration =
          kLegacyFullscreenControllerToolbarAnimationDuration.InSecondsF();
    }
  }

  id<TabStripCommands> tabStripCommandsHandler = nil;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // The -startDispatching for TabStripCommands will be called at a later
    // point for tablet only, so cannot use `HandlerForProtocol`.
    tabStripCommandsHandler = static_cast<id<TabStripCommands>>(_dispatcher);
  }
  _bubblePresenter = [[BubblePresenter alloc]
      initWithDeviceSwitcherResultDispatcher:deviceSwitcherResultDispatcher
                      hostContentSettingsMap:settingsMap
                             loadingNotifier:_urlLoadingNotifierBrowserAgent
                                 prefService:browserState->GetPrefs()
                                  sceneState:SceneStateBrowserAgent::
                                                 FromBrowser(self.browser)
                                                     ->GetSceneState()
                     tabStripCommandsHandler:tabStripCommandsHandler
                                     tracker:engagementTracker
                                webStateList:self.browser->GetWebStateList()];
  _bubblePresenter.layoutGuideCenter = _layoutGuideCenter;
  _bubblePresenter.delegate = self;
  [_dispatcher startDispatchingToTarget:_bubblePresenter
                            forProtocol:@protocol(HelpCommands)];

  _toolbarCoordinator =
      [[ToolbarCoordinator alloc] initWithBrowser:self.browser];

  _toolbarAccessoryPresenter = [[ToolbarAccessoryPresenter alloc]
      initWithIsIncognito:browserState->IsOffTheRecord()];
  _toolbarAccessoryPresenter.toolbarLayoutGuide =
      [_layoutGuideCenter makeLayoutGuideNamed:kPrimaryToolbarGuide];

  SnapshotBrowserAgent* snapshotBrowserAgent =
      SnapshotBrowserAgent::FromBrowser(self.browser);
  _sideSwipeMediator = [[SideSwipeMediator alloc]
      initWithFullscreenController:_fullscreenController
              snapshotBrowserAgent:snapshotBrowserAgent
                      webStateList:self.browser->GetWebStateList()];
  [_sideSwipeMediator setSnapshotDelegate:self];
  _sideSwipeMediator.toolbarInteractionHandler = _toolbarCoordinator;
  _sideSwipeMediator.toolbarSnapshotProvider = _toolbarCoordinator;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      !base::FeatureList::IsEnabled(kModernTabStrip)) {
    [_sideSwipeMediator setTabStripDelegate:_legacyTabStripCoordinator];
  }

  _bookmarksCoordinator =
      [[BookmarksCoordinator alloc] initWithBrowser:self.browser];

  self.browserContainerCoordinator = [[BrowserContainerCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  [self.browserContainerCoordinator start];

  self.downloadManagerCoordinator = [[DownloadManagerCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:self.browser];
  self.downloadManagerCoordinator.presenter =
      [[VerticalAnimationContainer alloc] init];
  self.tabLifecycleMediator.downloadManagerTabHelperDelegate =
      self.downloadManagerCoordinator;

  self.qrScannerCoordinator =
      [[QRScannerLegacyCoordinator alloc] initWithBrowser:self.browser];

  self.popupMenuCoordinator =
      [[PopupMenuCoordinator alloc] initWithBrowser:self.browser];
  self.popupMenuCoordinator.bubblePresenter = _bubblePresenter;
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinator;
  // Coordinator `start` is executed before setting it's `baseViewController`.
  // It is done intentionally, since this does not affecting the coordinator's
  // behavior but helps command handler setup below.
  [self.popupMenuCoordinator start];

  _NTPCoordinator = [[NewTabPageCoordinator alloc]
       initWithBrowser:self.browser
      componentFactory:[[NewTabPageComponentFactory alloc] init]];
  _NTPCoordinator.toolbarDelegate = _toolbarCoordinator;
  _NTPCoordinator.bubblePresenter = _bubblePresenter;
  self.tabLifecycleMediator.NTPCoordinator = _NTPCoordinator;

  _lensCoordinator = [[LensCoordinator alloc] initWithBrowser:self.browser];
  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

  _viewControllerDependencies.bubblePresenter = _bubblePresenter;
  _viewControllerDependencies.toolbarAccessoryPresenter =
      _toolbarAccessoryPresenter;
  _viewControllerDependencies.popupMenuCoordinator = self.popupMenuCoordinator;
  _viewControllerDependencies.ntpCoordinator = _NTPCoordinator;
  _viewControllerDependencies.toolbarCoordinator = _toolbarCoordinator;
  _viewControllerDependencies.tabStripCoordinator = _tabStripCoordinator;
  _viewControllerDependencies.legacyTabStripCoordinator =
      _legacyTabStripCoordinator;
  _viewControllerDependencies.sideSwipeMediator = _sideSwipeMediator;
  _viewControllerDependencies.bookmarksCoordinator = _bookmarksCoordinator;
  _viewControllerDependencies.fullscreenController = _fullscreenController;
  _viewControllerDependencies.textZoomHandler =
      HandlerForProtocol(_dispatcher, TextZoomCommands);
  _viewControllerDependencies.helpHandler =
      HandlerForProtocol(_dispatcher, HelpCommands);
  _viewControllerDependencies.popupMenuCommandsHandler =
      HandlerForProtocol(_dispatcher, PopupMenuCommands);
  _viewControllerDependencies.applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  _viewControllerDependencies.findInPageCommandsHandler =
      HandlerForProtocol(_dispatcher, FindInPageCommands);
  _viewControllerDependencies.isOffTheRecord = browserState->IsOffTheRecord();
  _viewControllerDependencies.urlLoadingBrowserAgent = _urlLoadingBrowserAgent;
  _viewControllerDependencies.urlLoadingNotifierBrowserAgent =
      _urlLoadingNotifierBrowserAgent;
  _viewControllerDependencies.tabUsageRecorderBrowserAgent =
      TabUsageRecorderBrowserAgent::FromBrowser(self.browser);
  _viewControllerDependencies.layoutGuideCenter = _layoutGuideCenter;
  _viewControllerDependencies.webStateList =
      self.browser->GetWebStateList()->AsWeakPtr();
  _viewControllerDependencies.voiceSearchController = _voiceSearchController;
  _viewControllerDependencies.safeAreaProvider =
      [[SafeAreaProvider alloc] initWithBrowser:self.browser];
  _viewControllerDependencies.pagePlaceholderBrowserAgent =
      PagePlaceholderBrowserAgent::FromBrowser(self.browser);
  _viewControllerDependencies.webStateUpdateBrowserAgent =
      WebStateUpdateBrowserAgent::FromBrowser(self.browser);
}

- (void)updateViewControllerDependencies {
  _bookmarksCoordinator.baseViewController = self.viewController;
  _bubblePresenter.rootViewController = self.viewController;

  _toolbarAccessoryPresenter.baseViewController = self.viewController;

  self.qrScannerCoordinator.baseViewController = self.viewController;
  [self.qrScannerCoordinator start];

  self.popupMenuCoordinator.baseViewController = self.viewController;

  // The Lens coordinator needs to be started before the primary toolbar
  // coordinator so that the LensCommands dispatcher is correctly registered in
  // time.
  _lensCoordinator.baseViewController = self.viewController;
  _lensCoordinator.delegate = self.viewController;
  [_lensCoordinator start];

  _toolbarCoordinator.omniboxFocusDelegate = self.viewController;
  _toolbarCoordinator.popupPresenterDelegate = self.viewController;
  _toolbarCoordinator.toolbarHeightDelegate = self.viewController;
  [_toolbarCoordinator start];

  _loadQueryCommandsHandler =
      HandlerForProtocol(_dispatcher, LoadQueryCommands);
  _viewController.loadQueryCommandsHandler = _loadQueryCommandsHandler;
  _voiceSearchController.dispatcher = _loadQueryCommandsHandler;
  _omniboxCommandsHandler = HandlerForProtocol(_dispatcher, OmniboxCommands);
  _viewController.omniboxCommandsHandler = _omniboxCommandsHandler;

  _legacyTabStripCoordinator.baseViewController = self.viewController;
  _NTPCoordinator.baseViewController = self.viewController;

  _bubblePresenter.toolbarCommandsHandler =
      HandlerForProtocol(_dispatcher, ToolbarCommands);

  [_dispatcher startDispatchingToTarget:self.viewController
                            forProtocol:@protocol(BrowserCommands)];
}

// Destroys the browser view controller dependencies.
- (void)destroyViewControllerDependencies {
  _viewControllerDependencies.bubblePresenter = nil;
  _viewControllerDependencies.toolbarAccessoryPresenter = nil;
  _viewControllerDependencies.popupMenuCoordinator = nil;
  _viewControllerDependencies.ntpCoordinator = nil;
  _viewControllerDependencies.toolbarCoordinator = nil;
  _viewControllerDependencies.tabStripCoordinator = nil;
  _viewControllerDependencies.legacyTabStripCoordinator = nil;
  _viewControllerDependencies.sideSwipeMediator = nil;
  _viewControllerDependencies.bookmarksCoordinator = nil;
  _viewControllerDependencies.fullscreenController = nil;
  _viewControllerDependencies.textZoomHandler = nil;
  _viewControllerDependencies.helpHandler = nil;
  _viewControllerDependencies.popupMenuCommandsHandler = nil;
  _viewControllerDependencies.applicationCommandsHandler = nil;
  _viewControllerDependencies.findInPageCommandsHandler = nil;
  _viewControllerDependencies.urlLoadingBrowserAgent = nil;
  _viewControllerDependencies.urlLoadingNotifierBrowserAgent = nil;
  _viewControllerDependencies.tabUsageRecorderBrowserAgent = nil;
  _viewControllerDependencies.layoutGuideCenter = nil;
  _viewControllerDependencies.voiceSearchController = nil;
  _viewControllerDependencies.safeAreaProvider = nil;
  _viewControllerDependencies.pagePlaceholderBrowserAgent = nil;
  _viewControllerDependencies.webStateUpdateBrowserAgent = nil;

  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;

  [_bookmarksCoordinator stop];
  _bookmarksCoordinator = nil;

  _legacyTabStripCoordinator = nil;
  _tabStripCoordinator = nil;
  [_sideSwipeMediator disconnect];
  _sideSwipeMediator = nil;
  _toolbarCoordinator = nil;
  _loadQueryCommandsHandler = nil;
  _omniboxCommandsHandler = nil;

  [_dispatcher stopDispatchingToTarget:_bubblePresenter];
  [_bubblePresenter stop];
  _bubblePresenter = nil;
  _toolbarAccessoryPresenter = nil;

  _fullscreenController = nullptr;

  [self.popupMenuCoordinator stop];
  self.popupMenuCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [_lensCoordinator stop];
  _lensCoordinator = nil;

  [self.downloadManagerCoordinator stop];
  self.downloadManagerCoordinator = nil;

  [self.browserContainerCoordinator stop];
  self.browserContainerCoordinator = nil;

  [_NTPCoordinator stop];
  _NTPCoordinator = nil;

  _keyCommandsProvider = nil;
  _dispatcher = nil;
  _layoutGuideCenter = nil;
  _webNavigationBrowserAgent = nil;
  _urlLoadingBrowserAgent = nil;
}

// Starts child coordinators.
- (void)startChildCoordinators {
  // Dispatcher should be instantiated so that it can be passed to child
  // coordinators.
  DCHECK(self.dispatcher);

  self.ARQuickLookCoordinator = [[ARQuickLookCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.ARQuickLookCoordinator start];

  self.followIPHCoordinator = [[FollowIPHCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.followIPHCoordinator start];
  // Updates the followIPHPresenter value inside tabLifecycleMediator.
  self.tabLifecycleMediator.followIPHPresenter = self.followIPHCoordinator;

  self.SafariDownloadCoordinator = [[SafariDownloadCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.SafariDownloadCoordinator start];

  self.vcardCoordinator =
      [[VcardCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser];
  [self.vcardCoordinator start];

  self.printCoordinator =
      [[PrintCoordinator alloc] initWithBaseViewController:self.viewController];
  // Updates the printControllar value inside tabLifecycleMediator.
  self.tabLifecycleMediator.printCoordinator = self.printCoordinator;

  // Help should only show in regular, non-incognito.
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    [self.popupMenuCoordinator startPopupMenuHelpCoordinator];
  }

  /* choiceCoordinator is created and started by a BrowserCommand */

  /* NetExportCoordinator is created and started by a delegate method */

  /* passwordBreachCoordinator is created and started by a BrowserCommand */

  /* passwordProtectionCoordinator is created and started by a BrowserCommand */

  /* passwordSettingsCoordinator is created and started by a delegate method */

  /* passwordSuggestionBottomSheetCoordinator is created and started by a
   * BrowserCommand */

  /* passwordSuggestionCoordinator is created and started by a BrowserCommand */

  /* paymentsSuggestionBottomSheetCoordinator is created and started by a
   * BrowserCommand */

  /* PriceNotificationsViewCoordinator is created and started by a
   * BrowserCommand */

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  /* RecentTabsCoordinator is created and started by a BrowserCommand */

  /* RepostFormCoordinator is created and started by a delegate method */

  /* WhatsNewCoordinator is created and started by a BrowserCommand */

  // TODO(crbug.com/1298934): Should start when the Sad Tab UI appears.
  self.sadTabCoordinator =
      [[SadTabCoordinator alloc] initWithBaseViewController:self.viewController
                                                    browser:self.browser];
  [self.sadTabCoordinator setOverscrollDelegate:self];

  /* SharingCoordinator is created and started by an ActivityServiceCommand */

  self.safeBrowsingCoordinator = [[SafeBrowsingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.safeBrowsingCoordinator start];

  self.textFragmentsCoordinator = [[TextFragmentsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.textFragmentsCoordinator start];

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.formInputAccessoryCoordinator.navigator = self;
  [self.formInputAccessoryCoordinator start];

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't dirctly
  // access the BVC's view.
  self.infobarModalOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarModal];
  [self.infobarModalOverlayContainerCoordinator start];
  self.viewController.infobarModalOverlayContainerViewController =
      self.infobarModalOverlayContainerCoordinator.viewController;

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.infobarBannerOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarBanner];
  [self.infobarBannerOverlayContainerCoordinator start];
  self.viewController.infobarBannerOverlayContainerViewController =
      self.infobarBannerOverlayContainerCoordinator.viewController;

  self.priceNotificationsIPHCoordinator =
      [[PriceNotificationsIPHCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.priceNotificationsIPHCoordinator start];
  // Updates the priceNotificationsIPHPresenter value inside
  // tabLifecycleMediator.
  self.tabLifecycleMediator.priceNotificationsIPHPresenter =
      self.priceNotificationsIPHCoordinator;

  if (IsCredentialProviderExtensionPromoEnabled() || IsIOSSetUpListEnabled()) {
    _credentialProviderPromoCoordinator =
        [[CredentialProviderPromoCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    _credentialProviderPromoCoordinator.promosUIHandler =
        _promosManagerCoordinator;
    [_credentialProviderPromoCoordinator start];
  }
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  [self.ARQuickLookCoordinator stop];
  self.ARQuickLookCoordinator = nil;

  [self.findBarCoordinator stop];
  self.findBarCoordinator = nil;

  [self.firstFollowCoordinator stop];
  self.firstFollowCoordinator = nil;

  [self.followIPHCoordinator stop];
  self.followIPHCoordinator = nil;

  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;

  [self.SafariDownloadCoordinator stop];
  self.SafariDownloadCoordinator = nil;

  [self.vcardCoordinator stop];
  self.vcardCoordinator = nil;

  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self stopPasswordProtectionCoordinator];

  [self stopPasswordSuggestionBottomSheetCoordinator];

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.passwordsAccountStorageNoticeCoordinator stop];
  self.passwordsAccountStorageNoticeCoordinator = nil;

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [self.printCoordinator stop];
  self.printCoordinator = nil;

  [self.priceNotificationsViewCoordiantor stop];
  self.priceNotificationsViewCoordiantor = nil;

  [self.promosManagerCoordinator stop];
  self.promosManagerCoordinator = nil;

  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;

  [self stopRecentTabsCoordinator];

  [self stopRepostFormCoordinator];

  // TODO(crbug.com/1298934): Should stop when the Sad Tab UI appears.
  [self.sadTabCoordinator stop];
  [self.sadTabCoordinator disconnect];
  self.sadTabCoordinator = nil;

  [self.safeBrowsingCoordinator stop];
  self.safeBrowsingCoordinator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [self stopStoreKitCoordinator];

  [self.textZoomCoordinator stop];
  self.textZoomCoordinator = nil;

  [self stopAutofillAddCreditCardCoordinator];

  [self.infobarBannerOverlayContainerCoordinator stop];
  self.infobarBannerOverlayContainerCoordinator = nil;

  [self.infobarModalOverlayContainerCoordinator stop];
  self.infobarModalOverlayContainerCoordinator = nil;

  [self.defaultBrowserPromoCoordinator stop];
  self.defaultBrowserPromoCoordinator = nil;

  [self.tailoredPromoCoordinator stop];
  self.tailoredPromoCoordinator = nil;

  [self.textFragmentsCoordinator stop];
  self.textFragmentsCoordinator = nil;

  [self.nonModalPromoCoordinator stop];
  self.nonModalPromoCoordinator = nil;

  [self.netExportCoordinator stop];
  self.netExportCoordinator = nil;

  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;

  [self.whatsNewCoordinator stop];
  self.whatsNewCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.priceNotificationsIPHCoordinator stop];
  self.priceNotificationsIPHCoordinator = nil;

  [_credentialProviderPromoCoordinator stop];
  _credentialProviderPromoCoordinator = nil;

  [self.defaultBrowserPromoManager stop];
  self.defaultBrowserPromoManager = nil;

  [self.choiceCoordinator stop];
  self.choiceCoordinator = nil;

  [self.miniMapCoordinator stop];
  self.miniMapCoordinator = nil;

  [self.saveToPhotosCoordinator stop];
  self.saveToPhotosCoordinator = nil;

  [self.parcelTrackingOptInCoordinator stop];
  self.parcelTrackingOptInCoordinator = nil;

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;
}

// Starts independent mediators owned by this coordinator.
- (void)startIndependentMediators {
  // Cache frequently repeated property values to curb generated code bloat.

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  BrowserViewController* browserViewController = self.viewController;

  DCHECK(self.browserContainerCoordinator.viewController);
  self.tabEventsMediator = [[TabEventsMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            ntpCoordinator:_NTPCoordinator
              browserState:browserState
           loadingNotifier:_urlLoadingNotifierBrowserAgent];
  self.tabEventsMediator.toolbarSnapshotProvider = _toolbarCoordinator;
  self.tabEventsMediator.consumer = browserViewController;

  CHECK(self.tabLifecycleMediator);
  self.tabLifecycleMediator.NTPTabHelperDelegate = self.tabEventsMediator;

  browserViewController.reauthHandler =
      HandlerForProtocol(self.dispatcher, IncognitoReauthCommands);

  browserViewController.nonModalPromoPresentationDelegate = self;

  if (browserState->IsOffTheRecord()) {
    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:sceneState];

    self.incognitoAuthMediator =
        [[IncognitoReauthMediator alloc] initWithReauthAgent:reauthAgent];
    self.incognitoAuthMediator.consumer = browserViewController;
  }
}

- (void)startTabLifeCycleMediator {
  Browser* browser = self.browser;

  TabLifecycleMediator* tabLifecycleMediator = [[TabLifecycleMediator alloc]
      initWithWebStateList:browser->GetWebStateList()];

  // Set properties that are already valid.
  tabLifecycleMediator.prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browser->GetBrowserState());
  tabLifecycleMediator.commandDispatcher = browser->GetCommandDispatcher();
  tabLifecycleMediator.tabHelperDelegate = self;
  tabLifecycleMediator.repostFormDelegate = self;
  tabLifecycleMediator.tabInsertionBrowserAgent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  tabLifecycleMediator.snapshotGeneratorDelegate = self;
  tabLifecycleMediator.overscrollActionsDelegate = self;
  tabLifecycleMediator.appLauncherBrowserPresentationProvider = self;

  self.tabLifecycleMediator = tabLifecycleMediator;
}

- (web::WebState*)activeWebState {
  WebStateList* webStateList = self.browser->GetWebStateList();
  return webStateList ? webStateList->GetActiveWebState() : nullptr;
}

#pragma mark - ActivityServiceCommands

- (void)stopAndStartSharingCoordinator {
  SharingParams* params =
      [[SharingParams alloc] initWithScenario:SharingScenario::TabShareButton];

  // Exit fullscreen if needed to make sure that share button is visible.
  _fullscreenController->ExitFullscreen();

  id<SharingPositioner> positioner = _toolbarCoordinator.sharingPositioner;
  UIBarButtonItem* anchor = nil;
  if ([positioner respondsToSelector:@selector(barButtonItem)]) {
    anchor = positioner.barButtonItem;
  }

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                          params:params
                      originView:positioner.sourceView
                      originRect:positioner.sourceRect
                          anchor:anchor];
  [self.sharingCoordinator start];
}

- (void)sharePage {
  if (!self.sharingCoordinator) {
    [self stopAndStartSharingCoordinator];
  } else {
    [self.sharingCoordinator cancelIfNecessaryAndCreateNewCoordinator];
  }
}

- (void)shareChromeApp {
  GURL URL = GURL(kChromeAppStoreUrl);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_SHARE_CHROME_TITLE);
  NSString* additionalText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_SHARE_CHROME_DESC);
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                          additionalText:additionalText
                                scenario:SharingScenario::ShareChrome];

  // Exit fullscreen if needed to make sure that share button is visible.
  _fullscreenController->ExitFullscreen();

  UIView* originView =
      [_layoutGuideCenter referencedViewUnderName:kToolsMenuGuide];
  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                      params:params
                                                  originView:originView];
  [self.sharingCoordinator start];
}

- (void)shareHighlight:(ShareHighlightCommand*)command {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:command.URL
                                   title:command.title
                          additionalText:command.selectedText
                                scenario:SharingScenario::SharedHighlight];

  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                      params:params
                                                  originView:command.sourceView
                                                  originRect:command.sourceRect
                                                      anchor:nil];
  [self.sharingCoordinator start];
}

#pragma mark - AutofillBottomSheetCommands

- (void)showPasswordBottomSheet:(const autofill::FormActivityParams&)params {
  self.passwordSuggestionBottomSheetCoordinator =
      [[PasswordSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              params:params
                            delegate:self];
  self.passwordSuggestionBottomSheetCoordinator.delegate = self;
  [self.passwordSuggestionBottomSheetCoordinator start];
}

- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params {
  if (self.paymentsSuggestionBottomSheetCoordinator) {
    return;
  }
  self.paymentsSuggestionBottomSheetCoordinator =
      [[PaymentsSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              params:params];
  self.paymentsSuggestionBottomSheetCoordinator
      .applicationSettingsCommandsHandler =
      HandlerForProtocol(self.dispatcher, ApplicationSettingsCommands);
  self.paymentsSuggestionBottomSheetCoordinator
      .browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands);
  [self.paymentsSuggestionBottomSheetCoordinator start];
}

#pragma mark - BrowserCoordinatorCommands

- (void)printTabWithBaseViewController:(UIViewController*)baseViewController {
  DCHECK(self.printCoordinator);
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  [self.printCoordinator printWebState:webState
                    baseViewController:baseViewController];
}

- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController {
  DCHECK(self.printCoordinator);
  [self.printCoordinator printImage:image
                              title:title
                 baseViewController:baseViewController];
}

- (void)showReadingList {
  if (self.readingListCoordinator) {
    [self closeReadingList];
  }
  self.readingListCoordinator = [[ReadingListCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.readingListCoordinator.delegate = self;
  [self.readingListCoordinator start];
}

- (void)showBookmarksManager {
  [IntentDonationHelper donateIntent:IntentType::kOpenBookmarks];
  [_bookmarksCoordinator presentBookmarks];
}

- (void)showFollowWhileBrowsingIPH {
  [_bubblePresenter presentFollowWhileBrowsingTipBubble];
}

- (void)showDefaultSiteViewIPH {
  [_bubblePresenter presentDefaultSiteViewTipBubble];
}

- (void)showDownloadsFolder {
  NSURL* URL = GetFilesAppUrl();
  if (!URL)
    return;

  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];

  base::UmaHistogramEnumeration(
      "Download.OpenDownloads.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.browser->GetBrowserState()));
}

- (void)showRecentTabs {
  [IntentDonationHelper donateIntent:IntentType::kOpenRecentTabs];

  // TODO(crbug.com/825431): If BVC's clearPresentedState is ever called (such
  // as in tearDown after a failed egtest), then this coordinator is left in a
  // started state even though its corresponding VC is no longer on screen.
  // That causes issues when the coordinator is started again and we destroy the
  // old mediator without disconnecting it first.  Temporarily work around these
  // issues by not having a long lived coordinator.  A longer-term solution will
  // require finding a way to stop this coordinator so that the mediator is
  // properly disconnected and destroyed and does not live longer than its
  // associated VC.
  [self.recentTabsCoordinator stop];
  self.recentTabsCoordinator = nil;

  self.recentTabsCoordinator = [[RecentTabsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.recentTabsCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  self.recentTabsCoordinator.delegate = self;
  [self.recentTabsCoordinator start];
}

- (void)showTranslate {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  engagementTracker->NotifyEvent(
      feature_engagement::events::kTriggeredTranslateInfobar);
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);

  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(activeWebState);
  if (translateClient) {
    translate::TranslateManager* translateManager =
        translateClient->GetTranslateManager();
    DCHECK(translateManager);
    translateManager->ShowTranslateUI(/*auto_translate=*/true,
                                      /*triggered_from_menu=*/true);
  }
}

- (void)showHelpPage {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  UrlLoadParams params = UrlLoadParams::InNewTab(helpUrl);
  params.append_to = OpenPosition::kCurrentTab;
  params.user_initiated = NO;
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  _urlLoadingBrowserAgent->Load(params);
}

- (void)showAddCreditCard {
  self.addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.addCreditCardCoordinator.delegate = self;
  [self.addCreditCardCoordinator start];
}

- (void)showSendTabToSelfUI:(const GURL&)url title:(NSString*)title {
  // TODO(crbug.com/1488763): Convert to CHECK.
  DUMP_WILL_BE_CHECK(!_sendTabToSelfCoordinator);

  _sendTabToSelfCoordinator = [[SendTabToSelfCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                 signinPresenter:self
                             url:url
                           title:title];
  [_sendTabToSelfCoordinator start];
}

- (void)hideSendTabToSelfUI {
  DCHECK(_sendTabToSelfCoordinator);
  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;
}

#if !defined(NDEBUG)
- (void)viewSource {
  ViewSourceBrowserAgent* viewSourceAgent =
      ViewSourceBrowserAgent::FromBrowser(self.browser);
  viewSourceAgent->ViewSourceForActiveWebState();
}
#endif  // !defined(NDEBUG)

- (void)focusFakebox {
  if ([self isNTPActiveForCurrentWebState]) {
    [_NTPCoordinator focusFakebox];
  }
}

// TODO(crbug.com/1272498): Refactor this command away, and add a mediator to
// observe the active web state closing and push updates into the BVC for UI
// work.
- (void)closeCurrentTab {
  WebStateList* webStateList = self.browser->GetWebStateList();

  int active_index = webStateList->active_index();
  if (active_index == WebStateList::kInvalidIndex)
    return;

  BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);

  UIView* contentArea = self.browserContainerCoordinator.viewController.view;
  UIView* snapshotView = nil;

  if (!canShowTabStrip) {
    snapshotView = [contentArea snapshotViewAfterScreenUpdates:NO];
    snapshotView.frame = contentArea.frame;
  }

  webStateList->CloseWebStateAt(active_index, WebStateList::CLOSE_USER_ACTION);

  if (!canShowTabStrip) {
    [contentArea addSubview:snapshotView];
    page_animation_util::AnimateOutWithCompletion(snapshotView, ^{
      [snapshotView removeFromSuperview];
    });
  }
}

- (void)showWhatsNew {
  self.whatsNewCoordinator = [[WhatsNewCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.whatsNewCoordinator start];
}

- (void)dismissWhatsNew {
  if (self.whatsNewCoordinator) {
    [self.whatsNewCoordinator stop];
    self.whatsNewCoordinator = nil;
  }
}

- (void)showWhatsNewIPH {
  [_bubblePresenter presentWhatsNewBottomToolbarBubble];
}

- (void)showSpotlightDebugger {
  [self.spotlightDebuggerCoordinator stop];
  self.spotlightDebuggerCoordinator = [[SpotlightDebuggerCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.spotlightDebuggerCoordinator start];
}

- (void)preloadVoiceSearch {
  // Preload VoiceSearchController and views and view controllers needed
  // for voice search.
  [_voiceSearchController prepareToAppear];
}

- (void)dismissPaymentSuggestions {
  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;
}

#pragma mark - DefaultPromoCommands

- (void)showTailoredPromoStaySafe {
  [self showTailoredPromoWithType:DefaultPromoTypeStaySafe];
}

- (void)showTailoredPromoMadeForIOS {
  [self showTailoredPromoWithType:DefaultPromoTypeMadeForIOS];
}

- (void)showTailoredPromoAllTabs {
  [self showTailoredPromoWithType:DefaultPromoTypeAllTabs];
}

- (void)showDefaultBrowserFullscreenPromo {
  if (!self.defaultBrowserPromoCoordinator) {
    self.defaultBrowserPromoCoordinator =
        [[DefaultBrowserPromoCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    self.defaultBrowserPromoCoordinator.handler = self;
  }
  [self.defaultBrowserPromoCoordinator start];
}

- (void)showChoice {
  if (!ios::provider::IsChoiceEnabled()) {
    return;
  }

  if (!self.choiceCoordinator) {
    self.choiceCoordinator =
        ios::provider::CreateChoiceCoordinatorWithViewController(
            self.viewController, self.browser);
  }
  [self.choiceCoordinator start];
}

- (void)dismissChoice {
  [self.choiceCoordinator stop];
  self.choiceCoordinator = nil;
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  [self.defaultBrowserPromoCoordinator stop];
  self.defaultBrowserPromoCoordinator = nil;
  [self.tailoredPromoCoordinator stop];
  self.tailoredPromoCoordinator = nil;
  if (IsDefaultBrowserInPromoManagerEnabled()) {
    [self.defaultBrowserPromoManager stop];
    self.defaultBrowserPromoManager = nil;
  }
}

#pragma mark - FeedCommands

- (void)showFirstFollowUIForWebSite:(FollowedWebSite*)followedWebSite {
  self.firstFollowCoordinator = [[FirstFollowCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                 followedWebSite:followedWebSite];
  [self.firstFollowCoordinator start];
}

#pragma mark - FindInPageCommands

- (void)openFindInPage {
  if (_toolbarAccessoryPresenter.isPresenting) {
    _nextToolbarToPresent = ToolbarKind::kFindInPage;
    [self closeTextZoom];
    return;
  }

  if (IsNativeFindInPageAvailable()) {
    [self showSystemFindPanel];
  } else {
    [self showFindBar];
  }
}

- (void)closeFindInPage {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }

  AbstractFindTabHelper* helper =
      GetConcreteFindTabHelperFromWebState(activeWebState);
  DCHECK(helper);
  if (helper->IsFindUIActive()) {
    helper->StopFinding();
  } else {
    [self.findBarCoordinator stop];
  }
}

- (void)showFindUIIfActive {
  auto* findHelper = GetConcreteFindTabHelperFromWebState(self.activeWebState);
  if (!findHelper || !findHelper->IsFindUIActive()) {
    return;
  }

  if (IsNativeFindInPageAvailable()) {
    [self showSystemFindPanel];
  } else if (!_toolbarAccessoryPresenter.isPresenting) {
    DCHECK(!self.findBarCoordinator);
    self.findBarCoordinator = [self newFindBarCoordinator];
    [self.findBarCoordinator start];
  }
}

- (void)hideFindUI {
  if (IsNativeFindInPageAvailable()) {
    web::WebState* activeWebState = self.activeWebState;
    DCHECK(activeWebState);
    auto* helper = FindTabHelper::FromWebState(activeWebState);
    helper->DismissFindNavigator();
  } else {
    [self.findBarCoordinator stop];
  }
}

- (void)defocusFindInPage {
  if (IsNativeFindInPageAvailable()) {
    // The System Find Panel UI cannot be "defocused" so closing Find in Page
    // altogether instead.
    [self closeFindInPage];
  } else {
    [self.findBarCoordinator defocusFindBar];
  }
}

- (void)searchFindInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  auto* helper = GetConcreteFindTabHelperFromWebState(activeWebState);
  helper->StartFinding([self.findBarCoordinator.findBarController searchTerm]);

  if (!self.browser->GetBrowserState()->IsOffTheRecord())
    helper->PersistSearchTerm();
}

- (void)findNextStringInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  GetConcreteFindTabHelperFromWebState(activeWebState)
      ->ContinueFinding(JavaScriptFindTabHelper::FORWARD);
}

- (void)findPreviousStringInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  GetConcreteFindTabHelperFromWebState(activeWebState)
      ->ContinueFinding(JavaScriptFindTabHelper::REVERSE);
}

#pragma mark - FindInPageCommands Helpers

- (void)showSystemFindPanel {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  auto* helper = FindTabHelper::FromWebState(activeWebState);

  if (!helper->IsFindUIActive()) {
    // Hide the Omnibox if possible, so as not to confuse the user as to what
    // text field is currently focused.
    _fullscreenController->EnterFullscreen();
    helper->SetFindUIActive(true);
  }

  // If the Native Find in Page variant does not use the Chrome Find bar, it
  // is sufficient to call `StartFinding()` directly on the Find tab helper of
  // the current web state.
  helper->StartFinding(@"");
}

- (void)showFindBar {
  if (!self.canShowFindBar) {
    return;
  }

  FindBarCoordinator* findBarCoordinator = self.findBarCoordinator;
  [findBarCoordinator stop];
  self.findBarCoordinator = [self newFindBarCoordinator];
  [self.findBarCoordinator start];
}

- (BOOL)canShowFindBar {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(activeWebState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

- (FindBarCoordinator*)newFindBarCoordinator {
  FindBarCoordinator* findBarCoordinator =
      [[FindBarCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];

  findBarCoordinator.presenter = _toolbarAccessoryPresenter;
  findBarCoordinator.delegate = self;
  findBarCoordinator.presentationDelegate = self.viewController;

  return findBarCoordinator;
}

#pragma mark - PromosManagerCommands

- (void)maybeDisplayPromo {
  if (!self.promosManagerCoordinator) {
    id<CredentialProviderPromoCommands> credentialProviderPromoHandler = nil;
    if (IsCredentialProviderExtensionPromoEnabled() ||
        IsIOSSetUpListEnabled()) {
      credentialProviderPromoHandler =
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             CredentialProviderPromoCommands);
    }
    self.promosManagerCoordinator = [[PromosManagerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
        credentialProviderPromoHandler:credentialProviderPromoHandler];

    // CredentialProviderPromoCoordinator is initialized earlier than this, so
    // make sure to set its UI handler.
    _credentialProviderPromoCoordinator.promosUIHandler =
        self.promosManagerCoordinator;

    [self.promosManagerCoordinator start];
  } else {
    [self.promosManagerCoordinator displayPromoIfAvailable];
  }
}

- (void)requestAppStoreReview {
  if (IsAppStoreRatingEnabled()) {
    UIWindowScene* scene =
        [SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState()
            scene];
    [SKStoreReviewController requestReviewInScene:scene];

    // Apple doesn't tell whether the app store review window will show or
    // provide a callback for when it is dismissed, so alert the coordinator
    // here so it can do any necessary cleanup.
    [self.promosManagerCoordinator promoWasDismissed];
  }
}

- (void)showWhatsNewPromo {
  [self showWhatsNew];
  self.whatsNewCoordinator.promosUIHandler = self.promosManagerCoordinator;
  self.whatsNewCoordinator.shouldShowBubblePromoOnDismiss = YES;
}

- (void)showChoicePromo {
  [self showChoice];
}

- (void)maybeDisplayDefaultBrowserPromo {
  if (!IsDefaultBrowserInPromoManagerEnabled()) {
    return;
  }

  if (self.defaultBrowserPromoManager) {
    // The default browser promo manager is already being displayed. Early
    // return as this is expected if a default browser promo was open and the
    // app was backgrounded.
    return;
  }

  self.defaultBrowserPromoManager = [[DefaultBrowserPromoManager alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.defaultBrowserPromoManager.promosUIHandler =
      self.promosManagerCoordinator;
  [self.defaultBrowserPromoManager start];
}

- (void)displayDefaultBrowserPromoAfterRemindMeLater {
  CHECK(IsDefaultBrowserInPromoManagerEnabled());

  self.defaultBrowserPromoManager = [[DefaultBrowserPromoManager alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.defaultBrowserPromoManager.promosUIHandler =
      self.promosManagerCoordinator;
  self.defaultBrowserPromoManager.promoWasFromRemindMeLater = YES;
  [self.defaultBrowserPromoManager start];
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  PageInfoCoordinator* pageInfoCoordinator = [[PageInfoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  pageInfoCoordinator.presentationProvider = self;
  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = pageInfoCoordinator;
  [self.pageInfoCoordinator start];
}

- (void)hidePageInfo {
  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;
}

- (void)showSecurityHelpPage {
  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kPageInfoHelpCenterURL));
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  _urlLoadingBrowserAgent->Load(params);
  [self hidePageInfo];
}

#pragma mark - FormInputAccessoryCoordinatorNavigator

- (void)openPasswordManager {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showSavedPasswordsSettingsFromViewController:self.viewController
                                  showCancelButton:YES
                                startPasswordCheck:NO];
}

- (void)openPasswordSettings {
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordSettingsCoordinator);

  // Use main browser to open the password settings.
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:sceneState.browserProviderInterface
                                     .mainBrowserProvider.browser];
  self.passwordSettingsCoordinator.delegate = self;
  [self.passwordSettingsCoordinator start];
}

- (void)openAddressSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showProfileSettingsFromViewController:self.viewController];
}

- (void)openCreditCardSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showCreditCardSettings];
}

#pragma mark - RepostFormTabHelperDelegate

- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completion {
  self.repostFormCoordinator = [[RepostFormCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                  dialogLocation:location
                        webState:webState
               completionHandler:completion];
  self.repostFormCoordinator.delegate = self;
  [self.repostFormCoordinator start];
}

- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)helper {
  [self stopRepostFormCoordinator];
}

#pragma mark - ToolbarAccessoryCoordinatorDelegate

- (void)toolbarAccessoryCoordinatorDidDismissUI:
    (ChromeCoordinator*)coordinator {
  if (self.findBarCoordinator) {
    self.findBarCoordinator = nil;
  }

  if (self.textZoomCoordinator) {
    self.textZoomCoordinator = nil;
  }

  if (!_nextToolbarToPresent.has_value()) {
    return;
  }

  const ToolbarKind nextToolbarToPresent = *_nextToolbarToPresent;
  _nextToolbarToPresent = absl::nullopt;

  switch (nextToolbarToPresent) {
    case ToolbarKind::kTextZoom:
      [self openTextZoom];
      break;

    case ToolbarKind::kFindInPage:
      [self openFindInPage];
      break;
  }
}

#pragma mark - TextZoomCommands

- (void)openTextZoom {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  AbstractFindTabHelper* findTabHelper =
      GetConcreteFindTabHelperFromWebState(activeWebState);
  DCHECK(findTabHelper);
  if (findTabHelper->IsFindUIActive()) {
    // If Find UI is active, close Find in Page.
    [self closeFindInPage];
    if (_toolbarAccessoryPresenter.isPresenting) {
      // If the Chrome Find Bar is presented (as opposed to the System Find
      // Panel UI) then open Text Zoom asynchronously once the Find Bar is
      // dismissed.
      _nextToolbarToPresent = ToolbarKind::kTextZoom;
      return;
    }
  }

  TextZoomCoordinator* textZoomCoordinator = self.textZoomCoordinator;
  if (textZoomCoordinator) {
    [textZoomCoordinator stop];
  }

  self.textZoomCoordinator = [self newTextZoomCoordinator];
  [self.textZoomCoordinator start];
}

- (void)closeTextZoom {
  web::WebState* activeWebState = self.activeWebState;
  if (activeWebState) {
    if (ios::provider::IsTextZoomEnabled()) {
      FontSizeTabHelper* fontSizeTabHelper =
          FontSizeTabHelper::FromWebState(activeWebState);
      fontSizeTabHelper->SetTextZoomUIActive(false);
    }
  }
  [self.textZoomCoordinator stop];
}

- (void)showTextZoomUIIfActive {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }

  FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(activeWebState);
  if (fontSizeTabHelper && fontSizeTabHelper->IsTextZoomUIActive() &&
      !_toolbarAccessoryPresenter.isPresenting) {
    DCHECK(!self.textZoomCoordinator);
    self.textZoomCoordinator = [self newTextZoomCoordinator];
    [self.textZoomCoordinator start];
  }
}

- (void)hideTextZoomUI {
  [self.textZoomCoordinator stop];
}

- (TextZoomCoordinator*)newTextZoomCoordinator {
  TextZoomCoordinator* textZoomCoordinator = [[TextZoomCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  textZoomCoordinator.presenter = _toolbarAccessoryPresenter;
  textZoomCoordinator.delegate = self;

  return textZoomCoordinator;
}

#pragma mark - UnitConversionCommands

- (void)presentUnitConversionForSourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location {
  self.unitConversionCoordinator = [[UnitConversionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                      sourceUnit:sourceUnit
                 sourceUnitValue:sourceUnitValue
                        location:location];
  [self.unitConversionCoordinator start];
}

- (void)hideUnitConversion {
  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;
}

#pragma mark - URLLoadingDelegate

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
  [self.viewController
      animateOpenBackgroundTabFromOriginPoint:params.origin_point
                                   completion:completion];
}

#pragma mark - Private WebState management methods

// Installs delegates for self.browser.
- (void)installDelegatesForBrowser {
  // The view controller should have been created.
  DCHECK(self.viewController);

  SyncErrorBrowserAgent::FromBrowser(self.browser)->SetUIProviders(self, self);

  WebStateDelegateBrowserAgent::FromBrowser(self.browser)
      ->SetUIProviders(self.contextMenuProvider,
                       self.formInputAccessoryCoordinator, self.viewController);

  if (_urlLoadingBrowserAgent) {
    _urlLoadingBrowserAgent->SetDelegate(self);
  }

  id<ApplicationCommands> applicationCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  AccountConsistencyBrowserAgent::CreateForBrowser(
      self.browser, self.viewController, applicationCommandHandler);

  if (FollowBrowserAgent::FromBrowser(self.browser)) {
    CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
    FollowBrowserAgent::FromBrowser(self.browser)
        ->SetUIProviders(
            HandlerForProtocol(commandDispatcher, NewTabPageCommands),
            static_cast<id<SnackbarCommands>>(commandDispatcher),
            HandlerForProtocol(commandDispatcher, FeedCommands));
  }
}

// Installs delegates for self.browser->GetBrowserState()
- (void)installDelegatesForBrowserState {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetWebStateList(self.browser->GetWebStateList());
  }
}

// Uninstalls delegates for self.browser->GetBrowserState()
- (void)uninstallDelegatesForBrowserState {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetWebStateList(nullptr);
  }
}

// Uninstalls delegates for self.browser.
- (void)uninstallDelegatesForBrowser {
  if (_urlLoadingBrowserAgent) {
    _urlLoadingBrowserAgent->SetDelegate(nil);
  }

  WebStateDelegateBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();

  SyncErrorBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();

  if (FollowBrowserAgent::FromBrowser(self.browser)) {
    FollowBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();
  }
}

#pragma mark - ParcelTrackingOptInCommands

- (void)showTrackingForParcels:(NSArray<CustomTextCheckingResult*>*)parcels {
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  if (!shoppingService) {
    return;
  }
  // Filter out parcels that are already being tracked and post
  // `showParcelTrackingUIWithNewParcels` command for the new parcel list.
  FilterParcelsAndShowParcelTrackingUI(
      shoppingService, parcels,
      HandlerForProtocol(self.dispatcher, ParcelTrackingOptInCommands));
}

- (void)showTrackingForFilteredParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels {
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  if (!shoppingService) {
    return;
  }
  if (IsUserEligibleParcelTrackingOptInPrompt(
          self.browser->GetBrowserState()->GetPrefs(), shoppingService)) {
    [self showParcelTrackingOptInPromptWithParcels:parcels];
  } else {
    [self maybeShowParcelTrackingInfobarWithParcels:parcels];
  }
}

- (void)showParcelTrackingInfobarWithParcels:
            (NSArray<CustomTextCheckingResult*>*)parcels
                                     forStep:(ParcelTrackingStep)step {
  web::WebState* activeWebState = self.activeWebState;
  CHECK(activeWebState);
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (!commerce::ShoppingServiceFactory::GetForBrowserState(browserState)
           ->IsParcelTrackingEligible()) {
    return;
  }
  if (step == ParcelTrackingStep::kNewPackageTracked) {
    feature_engagement::Tracker* engagementTracker =
        feature_engagement::TrackerFactory::GetForBrowserState(browserState);
    engagementTracker->NotifyEvent(feature_engagement::events::kParcelTracked);
  }
  std::unique_ptr<ParcelTrackingInfobarDelegate> delegate =
      std::make_unique<ParcelTrackingInfobarDelegate>(
          activeWebState, step, parcels,
          HandlerForProtocol(self.dispatcher, ApplicationCommands),
          HandlerForProtocol(self.dispatcher, ParcelTrackingOptInCommands));
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(activeWebState);

  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeParcelTracking, std::move(delegate));
  infobar_manager->AddInfoBar(std::move(infobar),
                              /*replace_existing=*/true);
}

- (void)showParcelTrackingIPH {
  [_bubblePresenter presentParcelTrackingTipBubble];
}

#pragma mark - ParcelTrackingOptInCommands helpers

- (void)maybeShowParcelTrackingInfobarWithParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels {
  IOSParcelTrackingOptInStatus optInStatus =
      static_cast<IOSParcelTrackingOptInStatus>(
          self.browser->GetBrowserState()->GetPrefs()->GetInteger(
              prefs::kIosParcelTrackingOptInStatus));
  switch (optInStatus) {
    case IOSParcelTrackingOptInStatus::kAlwaysTrack: {
      commerce::ShoppingService* shoppingService =
          commerce::ShoppingServiceFactory::GetForBrowserState(
              self.activeWebState->GetBrowserState());
      // Track parcels and display infobar if successful.
      TrackParcels(
          shoppingService, parcels, std::string(),
          HandlerForProtocol(self.dispatcher, ParcelTrackingOptInCommands),
          /*display_infobar=*/true, TrackingSource::kAutoTrack);
      break;
    }
    case IOSParcelTrackingOptInStatus::kAskToTrack:
      [self showParcelTrackingInfobarWithParcels:parcels
                                         forStep:ParcelTrackingStep::
                                                     kAskedToTrackPackage];
      break;
    case IOSParcelTrackingOptInStatus::kNeverTrack:
      // Do not display infobar.
      break;
  }
}

- (void)showParcelTrackingOptInPromptWithParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels {
  web::WebState* activeWebState = self.activeWebState;
  CHECK(activeWebState);
  [self dismissParcelTrackingOptInPrompt];
  self.parcelTrackingOptInCoordinator = [[ParcelTrackingOptInCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        webState:activeWebState
                         parcels:parcels];
  [self.parcelTrackingOptInCoordinator start];
}

- (void)dismissParcelTrackingOptInPrompt {
  if (self.parcelTrackingOptInCoordinator) {
    [self.parcelTrackingOptInCoordinator stop];
    self.parcelTrackingOptInCoordinator = nil;
  }
}

#pragma mark - PasswordBreachCommands

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType {
  self.passwordBreachCoordinator = [[PasswordBreachCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        leakType:leakType];
  [self.passwordBreachCoordinator start];
}

#pragma mark - PasswordProtectionCommands

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  self.passwordProtectionCoordinator = [[PasswordProtectionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                     warningText:warningText];
  self.passwordProtectionCoordinator.delegate = self;
  [self.passwordProtectionCoordinator startWithCompletion:completion];
}

#pragma mark - PasswordSuggestionCommands

- (void)showPasswordSuggestion:(NSString*)passwordSuggestion
               decisionHandler:(void (^)(BOOL accept))decisionHandler {
  self.passwordSuggestionCoordinator = [[PasswordSuggestionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
              passwordSuggestion:passwordSuggestion
                 decisionHandler:decisionHandler];
  self.passwordSuggestionCoordinator.delegate = self;
  [self.passwordSuggestionCoordinator start];
}

#pragma mark - PasswordsAccountStorageNoticeCommands

- (void)showPasswordsAccountStorageNoticeForEntryPoint:
            (PasswordsAccountStorageNoticeEntryPoint)entryPoint
                                      dismissalHandler:
                                          (void (^)())dismissalHandler {
  DCHECK(dismissalHandler);
  DCHECK(!self.passwordsAccountStorageNoticeCoordinator);
  self.passwordsAccountStorageNoticeCoordinator =
      [[PasswordsAccountStorageNoticeCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                          entryPoint:entryPoint
                    dismissalHandler:dismissalHandler];
  [self.passwordsAccountStorageNoticeCoordinator start];
}

- (void)hidePasswordsAccountStorageNotice {
  [self.passwordsAccountStorageNoticeCoordinator stop];
  self.passwordsAccountStorageNoticeCoordinator = nil;
}

#pragma mark - PriceNotificationsCommands

- (void)showPriceNotifications {
  self.priceNotificationsViewCoordiantor =
      [[PriceNotificationsViewCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.priceNotificationsViewCoordiantor start];
}

- (void)hidePriceNotifications {
  [self.priceNotificationsViewCoordiantor stop];
}

- (void)presentPriceNotificationsWhileBrowsingIPH {
  [_bubblePresenter presentPriceNotificationsWhileBrowsingTipBubble];
}

#pragma mark - PolicyChangeCommands

- (void)showForceSignedOutPrompt {
  if (!self.enterprisePromptCoordinator) {
    self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser
                        promptType:EnterprisePromptTypeForceSignOut];
    self.enterprisePromptCoordinator.delegate = self;
  }
  [self.enterprisePromptCoordinator start];
}

- (void)showSyncDisabledPrompt {
  if (!self.enterprisePromptCoordinator) {
    self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser
                        promptType:EnterprisePromptTypeSyncDisabled];
    self.enterprisePromptCoordinator.delegate = self;
  }
  [self.enterprisePromptCoordinator start];
}

- (void)showRestrictAccountSignedOutPrompt {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    if (!self.enterprisePromptCoordinator) {
      self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                          promptType:
                              EnterprisePromptTypeRestrictAccountSignedOut];
      self.enterprisePromptCoordinator.delegate = self;
    }
    [self.enterprisePromptCoordinator start];
  } else {
    __weak BrowserCoordinator* weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf showRestrictAccountSignedOutPrompt];
                   });
  }
}

#pragma mark - SaveToPhotosCommands

- (void)saveImageToPhotos:(SaveImageToPhotosCommand*)command {
  if (!command.webState) {
    // If the web state does not exist anymore, don't do anything.
    return;
  }

  // If the Save to Photos coordinator is not nil, stop it.
  [self stopSaveToPhotos];

  self.saveToPhotosCoordinator = [[SaveToPhotosCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        imageURL:command.imageURL
                        referrer:command.referrer
                        webState:command.webState.get()];
  [self.saveToPhotosCoordinator start];
}

- (void)stopSaveToPhotos {
  [self.saveToPhotosCoordinator stop];
  self.saveToPhotosCoordinator = nil;
}

#pragma mark - WebContentCommands

- (void)showAppStoreWithParameters:(NSDictionary*)productParameters {
  self.storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.storeKitCoordinator.delegate = self;
  self.storeKitCoordinator.iTunesProductParameters = productParameters;
  [self.storeKitCoordinator start];
}

- (void)showDialogForPassKitPasses:(NSArray<PKPass*>*)passes {
  if (self.passKitCoordinator.passes) {
    // Another pass is being displayed -- early return (this is unexpected).
    return;
  }

  self.passKitCoordinator =
      [[PassKitCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];

  self.passKitCoordinator.passes = passes;
  [self.passKitCoordinator start];
}

#pragma mark - DefaultBrowserPromoNonModalCommands

- (void)showDefaultBrowserNonModalPromo {
  self.nonModalPromoCoordinator =
      [[DefaultBrowserPromoNonModalCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.nonModalPromoCoordinator start];
  self.nonModalPromoCoordinator.browser = self.browser;
  self.nonModalPromoCoordinator.baseViewController = self.viewController;
  [self.nonModalPromoCoordinator presentInfobarBannerAnimated:YES
                                                   completion:nil];
}

- (void)dismissDefaultBrowserNonModalPromoAnimated:(BOOL)animated {
  [self.nonModalPromoCoordinator dismissInfobarBannerAnimated:animated
                                                   completion:nil];
}

- (void)defaultBrowserNonModalPromoWasDismissed {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
      logPromoWasDismissed];
  [self.nonModalPromoCoordinator stop];
  self.nonModalPromoCoordinator = nil;
}

#pragma mark - DefaultPromoNonModalPresentationDelegate

- (BOOL)defaultNonModalPromoIsShowing {
  return self.nonModalPromoCoordinator != nil;
}

- (void)dismissDefaultNonModalPromoAnimated:(BOOL)animated
                                 completion:(void (^)())completion {
  [self.nonModalPromoCoordinator dismissInfobarBannerAnimated:animated
                                                   completion:completion];
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
}

#pragma mark - NetExportTabHelperDelegate

- (void)netExportTabHelper:(NetExportTabHelper*)tabHelper
    showMailComposerWithContext:(ShowMailComposerContext*)context {
  self.netExportCoordinator = [[NetExportCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
             mailComposerContext:context];

  [self.netExportCoordinator start];
}

#pragma mark - PasswordSuggestionCoordinatorDelegate

- (void)closePasswordSuggestion {
  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;
}

#pragma mark - PreloadControllerDelegate methods

- (web::WebState*)webStateToReplace {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

- (UIView*)webViewContainer {
  return self.browserContainerCoordinator.viewController.view;
}

#pragma mark - SyncPresenter (Public)

- (void)showPrimaryAccountReauth {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
              showSignin:[[ShowSigninCommand alloc]
                             initWithOperation:AuthenticationOperation::
                                                   kPrimaryAccountReauth
                                   accessPoint:signin_metrics::AccessPoint::
                                                   ACCESS_POINT_REAUTH_INFO_BAR]
      baseViewController:self.viewController];
}

- (void)showSyncPassphraseSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showSyncPassphraseSettingsFromViewController:self.viewController];
}

- (void)showGoogleServicesSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showGoogleServicesSettingsFromViewController:self.viewController];
}

- (void)showAccountSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showAccountsSettingsFromViewController:self.viewController
                        skipIfUINotAvailable:NO];
}

- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                                   trigger:trigger];
}

- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                                trigger:
                                                                    trigger];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
              showSignin:command
      baseViewController:self.viewController];
}

#pragma mark - SnapshotGeneratorDelegate methods
// TODO(crbug.com/1272491): Refactor snapshot generation into (probably) a
// mediator with a narrowly-defined API to get UI-layer information from the
// BVC.

- (BOOL)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    canTakeSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);
  PagePlaceholderTabHelper* pagePlaceholderTabHelper =
      PagePlaceholderTabHelper::FromWebState(webState);
  return !pagePlaceholderTabHelper->displaying_placeholder() &&
         !pagePlaceholderTabHelper->will_add_placeholder_for_next_navigation();
}

- (UIEdgeInsets)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  DCHECK(webState);

  UIEdgeInsets maxViewportInsets =
      _fullscreenController->GetMaxViewportInsets();

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, or for the incognito NTP, the NTP is laid
    // out between the toolbars, so it should not be inset while snapshotting.
    if (canShowTabStrip || self.browser->GetBrowserState()->IsOffTheRecord()) {
      return UIEdgeInsetsZero;
    }

    // For the regular NTP without tab strip, it sits above the bottom toolbar
    // but, since it is displayed as full-screen at the top, it requires maximum
    // viewport insets.
    maxViewportInsets.bottom = 0;
    return maxViewportInsets;
  } else {
    // If the NTP is inactive, the WebState's view is used as the base view for
    // snapshotting.  If fullscreen is implemented by resizing the scroll view,
    // then the WebState view is already laid out within the visible viewport
    // and doesn't need to be inset.  If fullscreen uses the content inset, then
    // the WebState view is laid out fullscreen and should be inset by the
    // viewport insets.
    return _fullscreenController->ResizesScrollView() ? UIEdgeInsetsZero
                                                      : maxViewportInsets;
  }
}

- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState {
  DCHECK(webState);
  WebStateList* webStateList = self.browser->GetWebStateList();
  DCHECK_NE(webStateList->GetIndexOfWebState(webState),
            WebStateList::kInvalidIndex);

  if (!self.webUsageEnabled || webState != webStateList->GetActiveWebState()) {
    return @[];
  }

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];

  UIView* downloadManagerView = _downloadManagerCoordinator.viewController.view;
  if (downloadManagerView) {
    [overlays addObject:downloadManagerView];
  }

  UIView* sadTabView = self.sadTabCoordinator.viewController.view;
  if (sadTabView) {
    [overlays addObject:sadTabView];
  }

  BrowserContainerViewController* browserContainerViewController =
      self.browserContainerCoordinator.viewController;
  // The overlay container view controller is presenting something if it has
  // a `presentedViewController` AND that view controller's
  // `presentingViewController` is the overlay container. Otherwise, some other
  // view controller higher up in the hierarchy is doing the presenting. E.g.
  // for the overflow menu, the BVC (and eventually the tab grid view
  // controller) are presenting the overflow menu, but because those view
  // controllers are also above tthe `overlayContainerViewController` in the
  // view hierarchy, the overflow menu view controller is also the
  // `overlayContainerViewController`'s presentedViewController.
  UIViewController* overlayContainerViewController =
      browserContainerViewController.webContentsOverlayContainerViewController;
  UIViewController* presentedOverlayViewController =
      overlayContainerViewController.presentedViewController;
  if (presentedOverlayViewController &&
      presentedOverlayViewController.presentingViewController ==
          overlayContainerViewController) {
    [overlays addObject:presentedOverlayViewController.view];
  }

  UIView* screenTimeView =
      browserContainerViewController.screenTimeViewController.view;
  if (screenTimeView) {
    [overlays addObject:screenTimeView];
  }

  UIView* childOverlayView =
      overlayContainerViewController.childViewControllers.firstObject.view;
  if (childOverlayView) {
    DCHECK_EQ(1U, overlayContainerViewController.childViewControllers.count);
    [overlays addObject:childOverlayView];
  }

  return overlays;
}

- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);

  if ([self isNTPActiveForCurrentWebState]) {
    [_NTPCoordinator willUpdateSnapshot];
  }
  OverscrollActionsTabHelper::FromWebState(webState)->Clear();
}

- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    // If NTPCoordinator is not started yet, fall back to using the
    // webState's view. `_NTPCoordinator.started` should be true in most cases
    // but it can be false when the app will be terminated or the browser data
    // is removed. In particular, it can be false when this method is called as
    // a delayed task while the app is being terminated.
    if (_NTPCoordinator.started) {
      return _NTPCoordinator.viewController.view;
    }
  }
  return webState->GetView();
}

#pragma mark - NewTabPageCommands

- (void)openNTPScrolledIntoFeedType:(FeedType)feedType {
  // Dismiss any presenting modal. Ex. Follow management page.
  __weak __typeof(self) weakSelf = self;
  [self.viewController
      clearPresentedStateWithCompletion:^{
        [weakSelf scrollToNTPAfterPresentedStateCleared:feedType];
      }
                         dismissOmnibox:YES];
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  [_NTPCoordinator updateFollowingFeedHasUnseenContent:hasUnseenContent];
}

- (void)handleFeedModelDidEndUpdates:(FeedType)feedType {
  [_NTPCoordinator handleFeedModelDidEndUpdates:feedType];
}

- (void)scrollToNTPAfterPresentedStateCleared:(FeedType)feedType {
  web::WebState* activeWebState = self.activeWebState;

  // Configure next NTP to be scrolled into `feedType`.
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(activeWebState);
  if (NTPHelper) {
    NewTabPageState* ntpState = NTPHelper->GetNTPState();
    ntpState.selectedFeed = feedType;
    ntpState.shouldScrollToTopOfFeed = YES;
    NTPHelper->SetNTPState(ntpState);
  }

  // Navigate to NTP in same tab.
  UrlLoadParams urlLoadParams =
      UrlLoadParams::InCurrentTab(GURL(kChromeUINewTabURL));
  _urlLoadingBrowserAgent->Load(urlLoadParams);
}

#pragma mark - WebNavigationNTPDelegate

- (BOOL)isNTPActiveForCurrentWebState {
  return [_NTPCoordinator isNTPActiveForCurrentWebState];
}

- (void)reloadNTPForWebState:(web::WebState*)webState {
  [_NTPCoordinator reload];
}

#pragma mark - PageInfoPresentation

- (void)presentPageInfoView:(UIView*)pageInfoView {
  [pageInfoView setFrame:self.viewController.view.bounds];
  [self.viewController.view addSubview:pageInfoView];
}

- (void)prepareForPageInfoPresentation {
  // Dismiss the omnibox (if open).
  id<OmniboxCommands> omniboxHandler =
      HandlerForProtocol(_dispatcher, OmniboxCommands);
  [omniboxHandler cancelOmniboxEdit];
}

- (CGPoint)convertToPresentationCoordinatesForOrigin:(CGPoint)origin {
  return [self.viewController.view convertPoint:origin fromView:nil];
}

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordSettingsCoordinator, coordinator);
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;
}

#pragma mark - ReadingListCoordinatorDelegate

- (void)closeReadingList {
  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;
}

#pragma mark - BubblePresenterDelegate

- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == _bubblePresenter);
  return self.viewController.viewVisible;
}

- (BOOL)isNTPActiveForBubblePresenter:(BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == _bubblePresenter);
  return self.NTPCoordinator.isNTPActiveForCurrentWebState;
}

- (BOOL)isNTPScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == _bubblePresenter);
  return [self.NTPCoordinator isScrolledToTop];
}

#pragma mark - OverscrollActionsControllerDelegate methods.

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  [applicationCommandsHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithIncognito:self.browser->GetBrowserState()
                                                   ->IsOffTheRecord()]];
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  [self closeCurrentTab];
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  // Instruct the SnapshotTabHelper to ignore the next load event.
  // Attempting to snapshot while the overscroll "bounce back" animation is
  // occurring will cut the animation short.
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  SnapshotTabHelper::FromWebState(activeWebState)->IgnoreNextLoad();
  _webNavigationBrowserAgent->Reload();
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // When screeen size is not regular, overscroll actions should be enabled.
  return !_toolbarAccessoryPresenter.presenting &&
         !IsRegularXRegularSizeClass(self.viewController);
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return _toolbarCoordinator.primaryToolbarViewController.view;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [_toolbarCoordinator.primaryToolbarViewController.view
      snapshotViewAfterScreenUpdates:NO];
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // The current WebState can be nil if the Browser's WebStateList is empty
  // (e.g. after closing the last tab, etc).
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return 0.0;
  }

  OverscrollActionsTabHelper* activeTabHelper =
      OverscrollActionsTabHelper::FromWebState(activeWebState);
  if (controller == activeTabHelper->GetOverscrollActionsController()) {
    return self.viewController.headerHeight;
  } else {
    return 0.0;
  }
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.viewController.headerHeight;
}

- (CGFloat)initialContentOffsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return ios::provider::IsFullscreenSmoothScrollingSupported()
             ? -[self headerInsetForOverscrollActionsController:controller]
             : 0.0;
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return _fullscreenController;
}

#pragma mark - PasswordControllerDelegate methods

- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId {
  NSString* visibleTabId = self.activeWebState->GetStableIdentifier();
  // Ignore unless the call comes from currently visible tab.
  if (![tabId isEqualToString:visibleTabId]) {
    return NO;
  }
  [self.viewController addChildViewController:viewController];
  [self.viewController.view addSubview:viewController.view];
  [viewController didMoveToParentViewController:self.viewController];
  return YES;
}

- (void)displaySavedPasswordList {
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  [applicationCommandsHandler
      showSavedPasswordsSettingsFromViewController:self.viewController
                                  showCancelButton:YES
                                startPasswordCheck:NO];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  [applicationCommandsHandler showPasswordDetailsForCredential:credential
                                              showCancelButton:YES];
}

#pragma mark - MiniMapCommands

- (void)presentConsentThenMiniMapForText:(NSString*)text
                              inWebState:(web::WebState*)webState {
  self.miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                    webState:webState
                                                        text:text
                                             consentRequired:YES
                                                        mode:MiniMapMode::kMap];
  [self.miniMapCoordinator start];
}

- (void)presentMiniMapForText:(NSString*)text
                   inWebState:(web::WebState*)webState {
  self.miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                    webState:webState
                                                        text:text
                                             consentRequired:NO
                                                        mode:MiniMapMode::kMap];
  [self.miniMapCoordinator start];
}

- (void)presentMiniMapDirectionsForText:(NSString*)text
                             inWebState:(web::WebState*)webState {
  self.miniMapCoordinator = [[MiniMapCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        webState:webState
                            text:text
                 consentRequired:NO
                            mode:MiniMapMode::kDirections];
  [self.miniMapCoordinator start];
}

- (void)hideMiniMap {
  [self.miniMapCoordinator stop];
  self.miniMapCoordinator = nil;
}

#pragma mark - PasswordSuggestionBottomSheetCoordinatorDelegate

- (void)passwordSuggestionBottomSheetCoordinatorWantsToBeStopped:
    (PasswordSuggestionBottomSheetCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.passwordSuggestionBottomSheetCoordinator);
  [self stopPasswordSuggestionBottomSheetCoordinator];
}

#pragma mark - PasswordProtectionCoordinator

- (void)passwordProtectionCoordinatorWantsToBeStopped:
    (PasswordProtectionCoordinator*)coordinator {
  CHECK_EQ(self.passwordProtectionCoordinator, coordinator);
  [self stopPasswordProtectionCoordinator];
}

#pragma mark - RepostFormCoordinatorDelegate

- (void)repostFormCoordinatorWantsToBeDismissed:
    (RepostFormCoordinator*)coordinator {
  CHECK_EQ(self.repostFormCoordinator, coordinator);
  [self stopRepostFormCoordinator];
}

#pragma mark - RecentTabsCoordinatorDelegate

- (void)recentTabsCoordinatorWantsToBeDismissed:
    (RecentTabsCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.recentTabsCoordinator);
  [self stopRecentTabsCoordinator];
}

#pragma mark - StoreKitCoordinatorDelegate

- (void)storeKitCoordinatorWantsToStop:(StoreKitCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.storeKitCoordinator);
  [self stopStoreKitCoordinator];
}

#pragma mark - AutofillAddCreditCardCoordinatorDelegate

- (void)autofillAddCreditCardCoordinatorWantsToBeStopped:
    (AutofillAddCreditCardCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.addCreditCardCoordinator);
  [self stopAutofillAddCreditCardCoordinator];
}

#pragma mark - AppLauncherTabHelperBrowserPresentationProvider

- (BOOL)isBrowserPresentingUI {
  return self.viewController.presentedViewController != nil;
}

@end

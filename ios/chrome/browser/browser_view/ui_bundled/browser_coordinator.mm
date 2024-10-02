// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"

#import <StoreKit/StoreKit.h>

#import <memory>
#import <optional>

#import "base/check_deref.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/features.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator+Testing.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller+private.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_visibility_consumer.h"
#import "ios/chrome/browser/browser_view/ui_bundled/key_commands_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_events_mediator.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_lifecycle_mediator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_delegate.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_coordinator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_coordinator.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_coordinator.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_promo_non_modal_presentation_delegate.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_coordinator.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/ui_bundled/ar_quick_look_coordinator.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_coordinator.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/browser/download/ui_bundled/pass_kit_coordinator.h"
#import "ios/chrome/browser/download/ui_bundled/safari_download_coordinator.h"
#import "ios/chrome/browser/download/ui_bundled/vcard_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_util.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_controller_ios.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_coordinator.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/java_script_find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_coordinator.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/followed_web_site.h"
#import "ios/chrome/browser/follow/ui_bundled/first_follow_coordinator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_coordinator.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/chrome/browser/parcel_tracking/ui_bundled/parcel_tracking_opt_in_coordinator.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"
#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/prerender/model/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_share_url_command.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/feed_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_protection_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/features_utils.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/page_animation_util.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"
#import "ios/chrome/browser/spotlight_debugger/ui_bundled/spotlight_debugger_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/text_fragments/ui_bundled/text_fragments_coordinator.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_coordinator.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/tips_notifications/coordinator/enhanced_safe_browsing_promo_coordinator.h"
#import "ios/chrome/browser/tips_notifications/coordinator/lens_promo_coordinator.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_type.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/lens/lens_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"
#import "ios/chrome/browser/ui/print/print_coordinator.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/ui/safe_browsing/safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/view_source/model/view_source_browser_agent.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/ui_bundled/net_export_coordinator.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"
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
    ActivityServiceCommands,
    AddContactsCommands,
    AppLauncherTabHelperBrowserPresentationProvider,
    AutofillAddCreditCardCoordinatorDelegate,
    BrowserCoordinatorCommands,
    BrowserViewVisibilityConsumer,
    BubblePresenterDelegate,
    ContextualPanelEntrypointIPHCommands,
    ContextualSheetCommands,
    CountryCodePickerCommands,
    DefaultBrowserGenericPromoCommands,
    DefaultPromoNonModalPresentationDelegate,
    DriveFilePickerCommands,
    EnterprisePromptCoordinatorDelegate,
    FormInputAccessoryCoordinatorNavigator,
    MiniMapCommands,
    NetExportTabHelperDelegate,
    NewTabPageCommands,
    OverscrollActionsControllerDelegate,
    PageInfoCommands,
    PageInfoPresentation,
    ParcelTrackingOptInCommands,
    PasswordBreachCommands,
    PasswordControllerDelegate,
    PasswordProtectionCommands,
    PasswordProtectionCoordinatorDelegate,
    PasswordSettingsCoordinatorDelegate,
    PasswordSuggestionCommands,
    PasswordSuggestionCoordinatorDelegate,
    PriceNotificationsCommands,
    PromosManagerCommands,
    PolicyChangeCommands,
    PreloadControllerDelegate,
    QuickDeleteCommands,
    ReadingListCoordinatorDelegate,
    RecentTabsCoordinatorDelegate,
    RepostFormCoordinatorDelegate,
    RepostFormTabHelperDelegate,
    SaveToDriveCommands,
    SaveToPhotosCommands,
    SigninPresenter,
    SnapshotGeneratorDelegate,
    StoreKitCoordinatorDelegate,
    ToolbarAccessoryCoordinatorDelegate,
    UnitConversionCommands,
    URLLoadingDelegate,
    WebContentCommands,
    WebNavigationNTPDelegate,
    WebUsageEnablerBrowserAgentObserving,
    WhatsNewCommands>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

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

// Coordinator to show the Autofill progress dialog.
@property(nonatomic, strong)
    AutofillProgressDialogCoordinator* autofillProgressDialogCoordinator;

// Presents a QLPreviewController in order to display USDZ format 3D models.
@property(nonatomic, strong) ARQuickLookCoordinator* ARQuickLookCoordinator;

// Coordinator in charge of the presenting autofill options in a bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetCoordinator*
    passwordSuggestionBottomSheetCoordinator;

// Coordinator in charge of the presenting autofill options in a bottom sheet.
@property(nonatomic, strong) PaymentsSuggestionBottomSheetCoordinator*
    paymentsSuggestionBottomSheetCoordinator;

// Coordinator for the authentication when unmasking card during autofill.
@property(nonatomic, strong)
    CardUnmaskAuthenticationCoordinator* cardUnmaskAuthenticationCoordinator;

@property(nonatomic, strong)
    PlusAddressBottomSheetCoordinator* plusAddressBottomSheetCoordinator;

@property(nonatomic, strong) AutofillEditProfileBottomSheetCoordinator*
    autofillEditProfileBottomSheetCoordinator;

@property(nonatomic, strong) VirtualCardEnrollmentBottomSheetCoordinator*
    virtualCardEnrollmentBottomSheetCoordinator;

// Coordinator for the choice screen.
@property(nonatomic, strong) ChromeCoordinator* choiceCoordinator;

// Coordinator-ish provider for context menus.
@property(nonatomic, strong)
    ContextMenuConfigurationProvider* contextMenuProvider;

// Coordinator that manages the presentation of Download Manager UI.
@property(nonatomic, strong)
    DownloadManagerCoordinator* downloadManagerCoordinator;

// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;

// Coordinator to show the Autofill error dialog.
@property(nonatomic, strong)
    AutofillErrorDialogCoordinator* autofillErrorDialogCoordinator;

// Coordinator for the find bar.
@property(nonatomic, strong) FindBarCoordinator* findBarCoordinator;

// Coordinator for the First Follow modal.
@property(nonatomic, strong) FirstFollowCoordinator* firstFollowCoordinator;

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

// Coordinator for new tab pages.
@property(nonatomic, strong) NewTabPageCoordinator* NTPCoordinator;

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

// Coordinator for the popup menu.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

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

// Coordinator for displaying the Save to Drive UI.
@property(nonatomic, strong) SaveToDriveCoordinator* saveToDriveCoordinator;

// Coordinator for displaying the Save to Photos UI.
@property(nonatomic, strong) SaveToPhotosCoordinator* saveToPhotosCoordinator;

// Coordinator for sharing scenarios.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// The coordinator used for Spotlight Debugger.
@property(nonatomic, strong)
    SpotlightDebuggerCoordinator* spotlightDebuggerCoordinator;

// Coordinator for presenting SKStoreProductViewController.
@property(nonatomic, strong) StoreKitCoordinator* storeKitCoordinator;

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
@property(nonatomic, strong) DefaultBrowserGenericPromoCoordinator*
    defaultBrowserGenericPromoCoordinator;

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* activeWebState;

@end

@implementation BrowserCoordinator {
  BrowserViewControllerDependencies _viewControllerDependencies;
  KeyCommandsProvider* _keyCommandsProvider;
  BubblePresenterCoordinator* _bubblePresenterCoordinator;
  BubbleViewControllerPresenter* _contextualPanelEntrypointHelpPresenter;
  ToolbarAccessoryPresenter* _toolbarAccessoryPresenter;
  LensCoordinator* _lensCoordinator;
  LensOverlayCoordinator* _lensOverlayCoordinator;
  ToolbarCoordinator* _toolbarCoordinator;
  TabStripCoordinator* _tabStripCoordinator;
  TabStripLegacyCoordinator* _legacyTabStripCoordinator;
  SideSwipeMediator* _sideSwipeMediator;
  raw_ptr<FullscreenController> _fullscreenController;
  // The coordinator that shows the Send Tab To Self UI.
  SendTabToSelfCoordinator* _sendTabToSelfCoordinator;
  BookmarksCoordinator* _bookmarksCoordinator;
  std::optional<ToolbarKind> _nextToolbarToPresent;
  CredentialProviderPromoCoordinator* _credentialProviderPromoCoordinator;
  DockingPromoCoordinator* _dockingPromoCoordinator;
  // Used to display the Voice Search UI.  Nil if not visible.
  id<VoiceSearchController> _voiceSearchController;
  raw_ptr<UrlLoadingNotifierBrowserAgent> _urlLoadingNotifierBrowserAgent;
  id<LoadQueryCommands> _loadQueryCommandsHandler;
  id<OmniboxCommands> _omniboxCommandsHandler;
  LayoutGuideCenter* _layoutGuideCenter;
  raw_ptr<WebNavigationBrowserAgent> _webNavigationBrowserAgent;
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
  AddContactsCoordinator* _addContactsCoordinator;
  CountryCodePickerCoordinator* _countryCodePickerCoordinator;
  OmniboxPositionChoiceCoordinator* _omniboxPositionChoiceCoordinator;
  std::unique_ptr<WebUsageEnablerBrowserAgentObserverBridge>
      _webUsageEnablerObserver;
  ContextualSheetCoordinator* _contextualSheetCoordinator;
  RootDriveFilePickerCoordinator* _driveFilePickerCoordinator;
  SafeAreaProvider* _safeAreaProvider;
  // Number of time `showActivityOverlay` was called and its callback not
  // called.
  int _numberOfActivityOverly;
  // Callback to remove the activity overlay started by the browser coordinator
  // itself.
  base::ScopedClosureRunner _activityOverlayCallback;

  // The coordinator for the new Delete Browsing Data screen, also called Quick
  // Delete.
  QuickDeleteCoordinator* _quickDeleteCoordinator;
  LensPromoCoordinator* _lensPromoCoordinator;
  EnhancedSafeBrowsingPromoCoordinator* _enhancedSafeBrowsingPromoCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started) {
    return;
  }

  DCHECK(!self.viewController);

  _webUsageEnablerObserver =
      std::make_unique<WebUsageEnablerBrowserAgentObserverBridge>(
          WebUsageEnablerBrowserAgent::FromBrowser(self.browser), self);

  // TabLifeCycleMediator should start before createViewController because it
  // needs to register itself as a WebStateListObserver before the rest of the
  // UI in order to be able to install the tab helper delegate before the UI is
  // notified of WebStateList events.
  [self startTabLifeCycleMediator];

  [self createViewControllerDependencies];

  [self createViewController];

  [self updateViewControllerDependencies];

  // Force the view load at a specific time.
  // TODO(crbug.com/40263730): This should ideally go in createViewController,
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
  if (!self.started) {
    return;
  }

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
  _webUsageEnablerObserver.reset();
  _activityOverlayCallback.RunAndReset();
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
    _activityOverlayCallback.RunAndReset();
  } else if (!_activityOverlayCallback) {
    _activityOverlayCallback = [self showActivityOverlay];
  }

  ProfileIOS* profile = self.browser->GetProfile();
  if (profile) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForProfile(profile)
        ->SetEnabled(active);
  }
  self.webUsageEnabled = active;
  if (active) {
    // If the NTP was stopped because of a -setActive:NO call, then the NTP
    // needs to be restarted when -setActive:YES is called subsequently (i.e.
    // delete browsing data). This should not be needed for any other use case,
    // but on initial startup this is inevitably called after restoring tabs, so
    // cannot assert that it has not been started.
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    if (webState && NewTabPageTabHelper::FromWebState(webState)->IsActive() &&
        !self.NTPCoordinator.started) {
      // Avoid Voiceover focus to be stollen if the BrowserViewController is not
      // the top view.
      BOOL ntpIsTopView = !self.viewController.presentedViewController;
      self.NTPCoordinator.canfocusAccessibilityOmniboxWhenViewAppears =
          ntpIsTopView;

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
  [self hideSaveToDrive];
  [self hideDriveFilePicker];

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

  [self.passwordSuggestionBottomSheetCoordinator stop];
  self.passwordSuggestionBottomSheetCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.pageInfoCoordinator stop];

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [self.plusAddressBottomSheetCoordinator stop];
  self.plusAddressBottomSheetCoordinator = nil;

  [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  self.virtualCardEnrollmentBottomSheetCoordinator = nil;

  [self dismissAutofillErrorDialog];

  [self dismissAutofillProgressDialog];

  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.priceNotificationsViewCoordiantor stop];
  self.priceNotificationsViewCoordiantor = nil;

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;

  [self stopRepostFormCoordinator];

  [_formInputAccessoryCoordinator clearPresentedState];

  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;

  [self.viewController clearPresentedStateWithCompletion:completion
                                          dismissOmnibox:dismissOmnibox];

  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;

  [_countryCodePickerCoordinator stop];
  _countryCodePickerCoordinator = nil;

  [self dismissLensPromo];
  [self dismissEnhancedSafeBrowsingPromo];

  [self dismissAccountMenu];
}

#pragma mark - Private

// Returns whether overscroll actions should be allowed. When screeen size is
// not regular, they should be enabled.
- (BOOL)shouldAllowOverscrollActions {
  return !_toolbarAccessoryPresenter.presenting &&
         !IsRegularXRegularSizeClass(self.viewController);
}

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

// Stop the store kit coordinator.
- (void)stopStoreKitCoordinator {
  [self.storeKitCoordinator stop];
  self.storeKitCoordinator.delegate = nil;
  self.storeKitCoordinator = nil;
}

// Stops the coordinator for password manager settings.
- (void)stopPasswordSettingsCoordinator {
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;
}

// Dismisses the account menu.
- (void)dismissAccountMenu {
  if (!_NTPCoordinator) {
    return;
  }
  [_NTPCoordinator dismissAccountMenu];
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (!self.browser->GetProfile() || !self.started) {
    return;
  }
  _webUsageEnabled = webUsageEnabled;
  self.viewController.webUsageEnabled = webUsageEnabled;
}

// Displays activity overlay.
- (base::ScopedClosureRunner)showActivityOverlay {
  _numberOfActivityOverly++;
  self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.activityOverlayCoordinator start];
  return base::ScopedClosureRunner(base::BindOnce(
      [](BrowserCoordinator* strongSelf) {
        [strongSelf decreaseActivityOverlay];
      },
      self));
}

// Hides activity overlay number. Remove it if the number becomes 0..
- (void)decreaseActivityOverlay {
  _numberOfActivityOverly--;
  if (_numberOfActivityOverly == 0) {
    [self.activityOverlayCoordinator stop];
    self.activityOverlayCoordinator = nil;
  }
}

// Instantiates a BrowserViewController.
- (void)createViewController {
  DCHECK(self.browserContainerCoordinator.viewController);

  _viewController = [[BrowserViewController alloc]
      initWithBrowserContainerViewController:self.browserContainerCoordinator
                                                 .viewController
                         keyCommandsProvider:_keyCommandsProvider
                                dependencies:_viewControllerDependencies];
  _viewController.browserViewVisibilityConsumer = self;
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

  raw_ptr<TabBasedIPHBrowserAgent> tabBasedIPHBrowserAgent =
      TabBasedIPHBrowserAgent::FromBrowser(self.browser);
  if (tabBasedIPHBrowserAgent) {
    tabBasedIPHBrowserAgent->RootViewForInProductHelpWillDisappear();
  }

  // TODO(crbug.com/40256480): Remove when BVC will no longer handle commands.
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
    @protocol(AutofillCommands),
    @protocol(BrowserCoordinatorCommands),
    @protocol(ContextualPanelEntrypointIPHCommands),
    @protocol(ContextualSheetCommands),
    @protocol(DefaultBrowserPromoNonModalCommands),
    @protocol(DriveFilePickerCommands),
    @protocol(FeedCommands),
    @protocol(PromosManagerCommands),
    @protocol(FindInPageCommands),
    @protocol(NewTabPageCommands),
    @protocol(PageInfoCommands),
    @protocol(PasswordBreachCommands),
    @protocol(PasswordProtectionCommands),
    @protocol(PasswordSuggestionCommands),
    @protocol(PolicyChangeCommands),
    @protocol(PriceNotificationsCommands),
    @protocol(QuickDeleteCommands),
    @protocol(SaveToDriveCommands),
    @protocol(SaveToPhotosCommands),
    @protocol(TextZoomCommands),
    @protocol(WebContentCommands),
    @protocol(DefaultBrowserGenericPromoCommands),
    @protocol(MiniMapCommands),
    @protocol(ParcelTrackingOptInCommands),
    @protocol(UnitConversionCommands),
    @protocol(AddContactsCommands),
    @protocol(CountryCodePickerCommands),
    @protocol(WhatsNewCommands),
  ];

  for (Protocol* protocol in protocols) {
    [_dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }

  ProfileIOS* profile = self.browser->GetProfile();

  _keyCommandsProvider =
      [[KeyCommandsProvider alloc] initWithBrowser:self.browser];
  _keyCommandsProvider.applicationHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  _keyCommandsProvider.settingsHandler =
      HandlerForProtocol(_dispatcher, SettingsCommands);
  _keyCommandsProvider.findInPageHandler =
      HandlerForProtocol(_dispatcher, FindInPageCommands);
  _keyCommandsProvider.browserCoordinatorHandler =
      HandlerForProtocol(_dispatcher, BrowserCoordinatorCommands);
  _keyCommandsProvider.quickDeleteHandler =
      HandlerForProtocol(_dispatcher, QuickDeleteCommands);

  // TODO(crbug.com/40937114): This can't use HandlerForProtocol because
  // dispatch for BookmarksCommands is set up when the tab grid coordinator
  // starts, which is after this is called, so for now use static_cast until
  // that can be untangled.
  _keyCommandsProvider.bookmarksHandler =
      static_cast<id<BookmarksCommands>>(_dispatcher);

  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForProfile(profile);
  if (!profile->IsOffTheRecord()) {
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
      feature_engagement::TrackerFactory::GetForProfile(profile);

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    if (IsModernTabStripOrRaccoonEnabled()) {
      _tabStripCoordinator =
          [[TabStripCoordinator alloc] initWithBrowser:self.browser];
    } else {
      _legacyTabStripCoordinator =
          [[TabStripLegacyCoordinator alloc] initWithBrowser:self.browser];
      _legacyTabStripCoordinator.animationWaitDuration =
          kLegacyFullscreenControllerToolbarAnimationDuration.InSecondsF();
    }
  }

  _bubblePresenterCoordinator =
      [[BubblePresenterCoordinator alloc] initWithBrowser:self.browser];
  _bubblePresenterCoordinator.bubblePresenterDelegate = self;
  [_bubblePresenterCoordinator start];

  _toolbarCoordinator =
      [[ToolbarCoordinator alloc] initWithBrowser:self.browser];

  _toolbarAccessoryPresenter = [[ToolbarAccessoryPresenter alloc]
      initWithIsIncognito:profile->IsOffTheRecord()];
  _toolbarAccessoryPresenter.toolbarLayoutGuide =
      [_layoutGuideCenter makeLayoutGuideNamed:kPrimaryToolbarGuide];

  _sideSwipeMediator = [[SideSwipeMediator alloc]
      initWithFullscreenController:_fullscreenController
                      webStateList:self.browser->GetWebStateList()];
  _sideSwipeMediator.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  _sideSwipeMediator.toolbarInteractionHandler = _toolbarCoordinator;
  _sideSwipeMediator.toolbarSnapshotProvider = _toolbarCoordinator;
  _sideSwipeMediator.engagementTracker = engagementTracker;
  _sideSwipeMediator.helpHandler =
      HandlerForProtocol(_dispatcher, HelpCommands);
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      !IsModernTabStripOrRaccoonEnabled()) {
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
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinator;
  // Coordinator `start` is executed before setting it's `baseViewController`.
  // It is done intentionally, since this does not affecting the coordinator's
  // behavior but helps command handler setup below.
  [self.popupMenuCoordinator start];

  _NTPCoordinator = [[NewTabPageCoordinator alloc]
       initWithBrowser:self.browser
      componentFactory:[[NewTabPageComponentFactory alloc] init]];
  _NTPCoordinator.toolbarDelegate = _toolbarCoordinator;
  self.tabLifecycleMediator.NTPCoordinator = _NTPCoordinator;

  _lensCoordinator = [[LensCoordinator alloc] initWithBrowser:self.browser];

  _safeAreaProvider = [[SafeAreaProvider alloc] initWithBrowser:self.browser];

  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

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
  _viewControllerDependencies.isOffTheRecord = profile->IsOffTheRecord();
  _viewControllerDependencies.urlLoadingBrowserAgent = _urlLoadingBrowserAgent;
  _viewControllerDependencies.tabUsageRecorderBrowserAgent =
      TabUsageRecorderBrowserAgent::FromBrowser(self.browser);
  _viewControllerDependencies.layoutGuideCenter = _layoutGuideCenter;
  _viewControllerDependencies.webStateList =
      self.browser->GetWebStateList()->AsWeakPtr();
  _viewControllerDependencies.voiceSearchController = _voiceSearchController;
  _viewControllerDependencies.safeAreaProvider = _safeAreaProvider;
  _viewControllerDependencies.pagePlaceholderBrowserAgent =
      PagePlaceholderBrowserAgent::FromBrowser(self.browser);
}

- (void)updateViewControllerDependencies {
  _bookmarksCoordinator.baseViewController = self.viewController;

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
  _keyCommandsProvider.omniboxHandler = _omniboxCommandsHandler;
  _viewController.omniboxCommandsHandler = _omniboxCommandsHandler;

  _legacyTabStripCoordinator.baseViewController = self.viewController;
  _tabStripCoordinator.baseViewController = self.viewController;
  _NTPCoordinator.baseViewController = self.viewController;
  _bubblePresenterCoordinator.baseViewController = self.viewController;

  [_dispatcher startDispatchingToTarget:self.viewController
                            forProtocol:@protocol(BrowserCommands)];
}

// Destroys the browser view controller dependencies.
- (void)destroyViewControllerDependencies {
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
  _viewControllerDependencies.tabUsageRecorderBrowserAgent = nil;
  _viewControllerDependencies.layoutGuideCenter = nil;
  _viewControllerDependencies.voiceSearchController = nil;
  _viewControllerDependencies.safeAreaProvider = nil;
  _viewControllerDependencies.pagePlaceholderBrowserAgent = nil;

  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;

  [_bookmarksCoordinator stop];
  _bookmarksCoordinator = nil;

  [_bubblePresenterCoordinator stop];
  _bubblePresenterCoordinator = nil;

  _legacyTabStripCoordinator = nil;
  _tabStripCoordinator = nil;
  [_sideSwipeMediator disconnect];
  _sideSwipeMediator = nil;
  _toolbarCoordinator = nil;
  _loadQueryCommandsHandler = nil;
  _omniboxCommandsHandler = nil;

  _toolbarAccessoryPresenter = nil;

  [_contextualPanelEntrypointHelpPresenter dismissAnimated:NO];
  _contextualPanelEntrypointHelpPresenter = nil;

  _fullscreenController = nullptr;

  [self.popupMenuCoordinator stop];
  self.popupMenuCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [_lensCoordinator stop];
  _lensCoordinator = nil;

  [_lensOverlayCoordinator stop];
  _lensOverlayCoordinator = nil;

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
  if (!self.browser->GetProfile()->IsOffTheRecord()) {
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

  /* virtualCardEnrollmentBottomSheetCoordinator is created and started by a
   * BrowserCommand */

  /* autofillErrorDialogCoordinator is created and started by a BrowserCommand
   */

  /* autofillProgressDialogCoordinator is created and started by a
   * BrowserCommand */

  /* PriceNotificationsViewCoordinator is created and started by a
   * BrowserCommand */

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  /* RecentTabsCoordinator is created and started by a BrowserCommand */

  /* RepostFormCoordinator is created and started by a delegate method */

  /* WhatsNewCoordinator is created and started by a BrowserCommand */

  // TODO(crbug.com/40823248): Should start when the Sad Tab UI appears.
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

  // TODO(crbug.com/40228065): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.formInputAccessoryCoordinator.navigator = self;
  [self.formInputAccessoryCoordinator start];

  // TODO(crbug.com/40228065): Refactor this coordinator so it doesn't dirctly
  // access the BVC's view.
  self.infobarModalOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarModal];
  [self.infobarModalOverlayContainerCoordinator start];
  self.viewController.infobarModalOverlayContainerViewController =
      self.infobarModalOverlayContainerCoordinator.viewController;

  // TODO(crbug.com/40228065): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.infobarBannerOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarBanner];
  [self.infobarBannerOverlayContainerCoordinator start];
  self.viewController.infobarBannerOverlayContainerViewController =
      self.infobarBannerOverlayContainerCoordinator.viewController;

  _credentialProviderPromoCoordinator =
      [[CredentialProviderPromoCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  _credentialProviderPromoCoordinator.promosUIHandler =
      _promosManagerCoordinator;
  [_credentialProviderPromoCoordinator start];

  _dockingPromoCoordinator = [[DockingPromoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _dockingPromoCoordinator.promosUIHandler = _promosManagerCoordinator;
  [_dockingPromoCoordinator start];

  if (IsLensOverlayAvailable()) {
    _lensOverlayCoordinator = [[LensOverlayCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser];
    [_lensOverlayCoordinator start];
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

  [self.passwordSuggestionBottomSheetCoordinator stop];
  self.passwordSuggestionBottomSheetCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [self.cardUnmaskAuthenticationCoordinator stop];
  self.cardUnmaskAuthenticationCoordinator = nil;

  [self.plusAddressBottomSheetCoordinator stop];
  self.plusAddressBottomSheetCoordinator = nil;

  [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  self.virtualCardEnrollmentBottomSheetCoordinator = nil;

  [self dismissAutofillErrorDialog];

  [self dismissAutofillProgressDialog];

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

  // TODO(crbug.com/40823248): Should stop when the Sad Tab UI appears.
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

  [_credentialProviderPromoCoordinator stop];
  _credentialProviderPromoCoordinator = nil;

  [_dockingPromoCoordinator stop];
  _dockingPromoCoordinator = nil;

  [self.defaultBrowserGenericPromoCoordinator stop];
  self.defaultBrowserGenericPromoCoordinator = nil;

  [self.choiceCoordinator stop];
  self.choiceCoordinator = nil;

  [self.miniMapCoordinator stop];
  self.miniMapCoordinator = nil;

  [self.saveToDriveCoordinator stop];
  self.saveToDriveCoordinator = nil;

  [self.saveToPhotosCoordinator stop];
  self.saveToPhotosCoordinator = nil;

  [self.parcelTrackingOptInCoordinator stop];
  self.parcelTrackingOptInCoordinator = nil;

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;

  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;

  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;

  [self hideDriveFilePicker];
  [self hideContextualSheet];
  [self dismissEditAddressBottomSheet];
  [self dismissLensPromo];
  [self dismissEnhancedSafeBrowsingPromo];
}

// Starts independent mediators owned by this coordinator.
- (void)startIndependentMediators {
  // Cache frequently repeated property values to curb generated code bloat.

  ProfileIOS* profile = self.browser->GetProfile();
  BrowserViewController* browserViewController = self.viewController;

  DCHECK(self.browserContainerCoordinator.viewController);
  self.tabEventsMediator = [[TabEventsMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            ntpCoordinator:_NTPCoordinator
                   profile:profile
           loadingNotifier:_urlLoadingNotifierBrowserAgent];
  self.tabEventsMediator.toolbarSnapshotProvider = _toolbarCoordinator;
  self.tabEventsMediator.consumer = browserViewController;

  CHECK(self.tabLifecycleMediator);
  self.tabLifecycleMediator.NTPTabHelperDelegate = self.tabEventsMediator;

  browserViewController.reauthHandler =
      HandlerForProtocol(self.dispatcher, IncognitoReauthCommands);

  browserViewController.nonModalPromoPresentationDelegate = self;

  if (profile->IsOffTheRecord()) {
    SceneState* sceneState = self.browser->GetSceneState();
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
      PrerenderServiceFactory::GetForProfile(browser->GetProfile());
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

- (void)contextualPanelEntrypointIPHDidDismissWithConfig:
            (base::WeakPtr<ContextualPanelItemConfiguration>)config
                                         dismissalReason:
                                             (IPHDismissalReasonType)
                                                 IPHDismissalReasonType {
  ContextualPanelItemConfiguration* config_ptr = config.get();
  if (!config_ptr) {
    return;
  }

  [HandlerForProtocol(self.dispatcher, ContextualPanelEntrypointCommands)
      contextualPanelEntrypointIPHWasDismissed];

  ProfileIOS* profile = self.browser->GetProfile();
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  if (!engagementTracker || !_contextualPanelEntrypointHelpPresenter) {
    return;
  }

  engagementTracker->Dismissed(*config_ptr->iph_feature);
  _contextualPanelEntrypointHelpPresenter = nil;

  if (IPHDismissalReasonType == IPHDismissalReasonType::kTappedAnchorView ||
      IPHDismissalReasonType == IPHDismissalReasonType::kTappedIPH) {
    [self openContextualSheet];
    [self recordContextualPanelEntrypointIPHDismissed:
              ContextualPanelIPHDismissedReason::UserInteracted];
    return;
  }

  if (IPHDismissalReasonType ==
          IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView ||
      IPHDismissalReasonType == IPHDismissalReasonType::kTappedClose) {
    engagementTracker->NotifyEvent(
        config_ptr->iph_entrypoint_explicitly_dismissed);
    [self recordContextualPanelEntrypointIPHDismissed:
              ContextualPanelIPHDismissedReason::UserDismissed];
    return;
  }

  if (IPHDismissalReasonType == IPHDismissalReasonType::kTimedOut) {
    [self recordContextualPanelEntrypointIPHDismissed:
              ContextualPanelIPHDismissedReason::TimedOut];
    return;
  }

  [self recordContextualPanelEntrypointIPHDismissed:
            ContextualPanelIPHDismissedReason::Other];
}

- (void)recordContextualPanelEntrypointIPHDismissed:
    (ContextualPanelIPHDismissedReason)dismissalReason {
  base::UmaHistogramEnumeration("IOS.ContextualPanel.IPH.DismissedReason",
                                dismissalReason);
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
  // Defocus Find-In-Page before opening the share sheet. This will result in
  // closing the Find-In-Page for some OS versions.
  [self defocusFindInPage];

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

- (void)shareURLFromContextMenu:(ActivityServiceShareURLCommand*)command {
  SharingParams* params = [[SharingParams alloc]
      initWithURL:command.URL
            title:command.title
         scenario:SharingScenario::ShareInWebContextMenu];

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
  // Do not present the bottom sheet if it is already being presented.
  if (self.passwordSuggestionBottomSheetCoordinator) {
    return;
  }
  self.passwordSuggestionBottomSheetCoordinator =
      [[PasswordSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              params:params
                            delegate:self];
  self.passwordSuggestionBottomSheetCoordinator.settingsHandler =
      HandlerForProtocol(self.dispatcher, SettingsCommands);
  self.passwordSuggestionBottomSheetCoordinator
      .browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands);
  [self.passwordSuggestionBottomSheetCoordinator start];
}

- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params {
  // Do not present the bottom sheet if it is already being presented.
  if (self.paymentsSuggestionBottomSheetCoordinator) {
    return;
  }
  self.paymentsSuggestionBottomSheetCoordinator =
      [[PaymentsSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              params:params];
  self.paymentsSuggestionBottomSheetCoordinator.settingsHandler =
      HandlerForProtocol(self.dispatcher, SettingsCommands);
  self.paymentsSuggestionBottomSheetCoordinator
      .browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands);
  [self.paymentsSuggestionBottomSheetCoordinator start];
}

- (void)showCardUnmaskAuthentication {
  self.cardUnmaskAuthenticationCoordinator =
      [[CardUnmaskAuthenticationCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.cardUnmaskAuthenticationCoordinator.shouldStartWithCvcAuth = NO;

  [self.cardUnmaskAuthenticationCoordinator start];
}

- (void)continueCardUnmaskWithOtpAuth {
  // This assumes the card unmask authentication coordinator is already created
  // by the showCardUnmaskAuthentication function above. Otherwise do nothing.
  [self.cardUnmaskAuthenticationCoordinator continueWithOtpAuth];
}

- (void)continueCardUnmaskWithCvcAuth {
  if (self.cardUnmaskAuthenticationCoordinator) {
    // If the coordinator exists, it means that multiple authentication options
    // are provided and we have already presented the authentication selection
    // dialog, and the navigation controller is already created. Upon user
    // selection, we should show the CVC input dialog by pushing the view to the
    // navigation stack.
    [self.cardUnmaskAuthenticationCoordinator continueWithCvcAuth];
  } else {
    // If the coordinator does not exists, it means there is only one
    // authentication option (CVC auth) provided, and the navigation controller
    // is not yet created, so we skip the authentication selection step and
    // start directly with the CVC input dialog.
    self.cardUnmaskAuthenticationCoordinator =
        [[CardUnmaskAuthenticationCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    self.cardUnmaskAuthenticationCoordinator.shouldStartWithCvcAuth = YES;
    [self.cardUnmaskAuthenticationCoordinator start];
  }
}

- (void)showPlusAddressesBottomSheet {
  self.plusAddressBottomSheetCoordinator =
      [[PlusAddressBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.plusAddressBottomSheetCoordinator start];
}

- (void)showVirtualCardEnrollmentBottomSheet:
    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model {
  if (self.virtualCardEnrollmentBottomSheetCoordinator) {
    [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  }
  self.virtualCardEnrollmentBottomSheetCoordinator =
      [[VirtualCardEnrollmentBottomSheetCoordinator alloc]
             initWithUIModel:std::move(model)
          baseViewController:self.viewController
                     browser:self.browser];
  [self.virtualCardEnrollmentBottomSheetCoordinator start];
}

- (void)showEditAddressBottomSheet {
  self.autofillEditProfileBottomSheetCoordinator =
      [[AutofillEditProfileBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.autofillEditProfileBottomSheetCoordinator start];
}

- (void)dismissEditAddressBottomSheet {
  if (self.autofillEditProfileBottomSheetCoordinator) {
    [self.autofillEditProfileBottomSheetCoordinator stop];
  }

  self.autofillEditProfileBottomSheetCoordinator = nil;
}

- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext {
  if (self.autofillErrorDialogCoordinator) {
    [self.autofillErrorDialogCoordinator stop];
  }

  self.autofillErrorDialogCoordinator = [[AutofillErrorDialogCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                    errorContext:std::move(errorContext)];
  self.autofillErrorDialogCoordinator.autofillCommandsHandler =
      HandlerForProtocol(self.dispatcher, AutofillCommands);
  [self.autofillErrorDialogCoordinator start];
}

- (void)dismissAutofillErrorDialog {
  [self.autofillErrorDialogCoordinator stop];
  self.autofillErrorDialogCoordinator = nil;
}

- (void)showAutofillProgressDialog {
  if (self.autofillProgressDialogCoordinator) {
    [self.autofillProgressDialogCoordinator stop];
  }

  self.autofillProgressDialogCoordinator =
      [[AutofillProgressDialogCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.autofillProgressDialogCoordinator start];
}

- (void)dismissAutofillProgressDialog {
  [self.autofillProgressDialogCoordinator stop];
  self.autofillProgressDialogCoordinator = nil;
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

- (void)showDownloadsFolder {
  NSURL* URL = GetFilesAppUrl();
  if (!URL) {
    return;
  }

  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];

  base::UmaHistogramEnumeration(
      "Download.OpenDownloads.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.browser->GetProfile()));
}

- (void)showRecentTabs {
  [IntentDonationHelper donateIntent:IntentType::kOpenRecentTabs];

  // TODO(crbug.com/40568563): If BVC's clearPresentedState is ever called (such
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
  ProfileIOS* profile = self.browser->GetProfile();

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
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

  // Records the usage of Google Translate. This notifies the Tips Manager,
  // which may trigger tips or guidance related to translation features.
  if (IsSegmentationTipsManagerEnabled()) {
    TipsManagerIOS* tipsManager = TipsManagerIOSFactory::GetForProfile(profile);

    tipsManager->NotifySignal(
        segmentation_platform::tips_manager::signals::kUsedGoogleTranslation);
  }
}

- (void)showHelpPage {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  UrlLoadParams params = UrlLoadParams::InNewTab(helpUrl);
  params.append_to = OpenPosition::kCurrentTab;
  params.user_initiated = NO;
  params.in_incognito = self.browser->GetProfile()->IsOffTheRecord();
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

// TODO(crbug.com/40806293): Refactor this command away, and add a mediator to
// observe the active web state closing and push updates into the BVC for UI
// work.
- (void)closeCurrentTab {
  WebStateList* webStateList = self.browser->GetWebStateList();

  int active_index = webStateList->active_index();
  if (active_index == WebStateList::kInvalidIndex) {
    return;
  }

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

- (void)dismissPasswordSuggestions {
  [self.passwordSuggestionBottomSheetCoordinator stop];
  self.passwordSuggestionBottomSheetCoordinator = nil;
}

- (void)dismissPaymentSuggestions {
  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;
}

- (void)dismissCardUnmaskAuthentication {
  [self.cardUnmaskAuthenticationCoordinator stop];
  self.cardUnmaskAuthenticationCoordinator = nil;
}

- (void)dismissPlusAddressBottomSheet {
  [self.plusAddressBottomSheetCoordinator stop];
  self.plusAddressBottomSheetCoordinator = nil;
}

- (void)dismissVirtualCardEnrollmentBottomSheet {
  [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  self.virtualCardEnrollmentBottomSheetCoordinator = nil;
}

- (void)showOmniboxPositionChoice {
  if (!_omniboxPositionChoiceCoordinator) {
    _omniboxPositionChoiceCoordinator =
        [[OmniboxPositionChoiceCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
  } else {
    [_omniboxPositionChoiceCoordinator stop];
  }
  [_omniboxPositionChoiceCoordinator start];
}

- (void)dismissOmniboxPositionChoice {
  [_omniboxPositionChoiceCoordinator stop];
  _omniboxPositionChoiceCoordinator = nil;
}

- (void)showLensPromo {
  [_lensPromoCoordinator stop];
  _lensPromoCoordinator = [[LensPromoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [_lensPromoCoordinator start];
}

- (void)dismissLensPromo {
  [_lensPromoCoordinator stop];
  _lensPromoCoordinator = nil;
}

- (void)showEnhancedSafeBrowsingPromo {
  [_enhancedSafeBrowsingPromoCoordinator stop];
  _enhancedSafeBrowsingPromoCoordinator =
      [[EnhancedSafeBrowsingPromoCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [_enhancedSafeBrowsingPromoCoordinator start];
}

- (void)dismissEnhancedSafeBrowsingPromo {
  [_enhancedSafeBrowsingPromoCoordinator stop];
  _enhancedSafeBrowsingPromoCoordinator = nil;
}

#pragma mark - BrowserViewVisibilityConsumer

- (void)browserViewDidChangeVisibility {
  raw_ptr<TabBasedIPHBrowserAgent> tabBasedIPHBrowserAgent =
      TabBasedIPHBrowserAgent::FromBrowser(self.browser);
  if (!tabBasedIPHBrowserAgent) {
    return;
  }
  if (self.viewController.viewVisible) {
    tabBasedIPHBrowserAgent->RootViewForInProductHelpDidAppear();
  } else {
    tabBasedIPHBrowserAgent->RootViewForInProductHelpWillDisappear();
  }
}

#pragma mark - ContextualPanelEntrypointIPHCommands

- (BOOL)maybeShowContextualPanelEntrypointIPHWithConfig:
            (base::WeakPtr<ContextualPanelItemConfiguration>)config
                                            anchorPoint:(CGPoint)anchorPoint
                                        isBottomOmnibox:(BOOL)isBottomOmnibox {
  ContextualPanelItemConfiguration& config_ref = CHECK_DEREF(config.get());

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());

  if (!engagementTracker) {
    return NO;
  }

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType IPHDismissalReasonType,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf contextualPanelEntrypointIPHDidDismissWithConfig:config
                                                   dismissalReason:
                                                       IPHDismissalReasonType];
      };

  UIImage* image =
      [UIImage imageNamed:base::SysUTF8ToNSString(config_ref.iph_image_name)];

  _contextualPanelEntrypointHelpPresenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:ShouldShowRichContextualPanelEntrypointIPH()
                                ? base::SysUTF8ToNSString(config_ref.iph_text)
                                : base::SysUTF8ToNSString(config_ref.iph_title)
                      title:base::SysUTF8ToNSString(config_ref.iph_title)
                      image:image
             arrowDirection:isBottomOmnibox ? BubbleArrowDirectionDown
                                            : BubbleArrowDirectionUp
                  alignment:BubbleAlignmentTopOrLeading
                 bubbleType:ShouldShowRichContextualPanelEntrypointIPH()
                                ? BubbleViewTypeRich
                                : BubbleViewTypeDefault
          dismissalCallback:dismissalCallback];

  _contextualPanelEntrypointHelpPresenter.voiceOverAnnouncement =
      base::SysUTF8ToNSString(config_ref.iph_text);
  _contextualPanelEntrypointHelpPresenter.ignoreWebContentAreaInteractions =
      YES;
  _contextualPanelEntrypointHelpPresenter.customBubbleVisibilityDuration =
      LargeContextualPanelEntrypointDisplayedInSeconds();

  // Early return if the bubble wouldn't fit in its parent view.
  if (![_contextualPanelEntrypointHelpPresenter
          canPresentInView:self.viewController.view
               anchorPoint:anchorPoint]) {
    _contextualPanelEntrypointHelpPresenter = nil;
    return NO;
  }

  // Do this check last as the FET needs to know the IPH can be shown.
  if (!engagementTracker->ShouldTriggerHelpUI(*config_ref.iph_feature)) {
    _contextualPanelEntrypointHelpPresenter = nil;
    return NO;
  }

  [_contextualPanelEntrypointHelpPresenter
      presentInViewController:self.viewController
                  anchorPoint:anchorPoint];

  return YES;
}

- (void)dismissContextualPanelEntrypointIPHAnimated:(BOOL)animated {
  [_contextualPanelEntrypointHelpPresenter dismissAnimated:animated];
  _contextualPanelEntrypointHelpPresenter = nil;
}

#pragma mark - ContextualSheetCommands

- (void)openContextualSheet {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }

  // Close the keyboard before opening the sheet.
  UIView* view = activeWebState->GetView();
  if (view) {
    [view endEditing:YES];
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(activeWebState);
  contextualPanelTabHelper->OpenContextualPanel();

  [self showContextualSheetUIIfActive];
}

- (void)closeContextualSheet {
  web::WebState* activeWebState = self.activeWebState;
  if (activeWebState) {
    ContextualPanelTabHelper* contextualPanelTabHelper =
        ContextualPanelTabHelper::FromWebState(activeWebState);
    contextualPanelTabHelper->CloseContextualPanel();
  }

  [self hideContextualSheet];
}

- (void)showContextualSheetUIIfActive {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(activeWebState);
  if (!contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    return;
  }

  _contextualSheetCoordinator = [[ContextualSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _contextualSheetCoordinator.presenter = self.viewController;
  [_contextualSheetCoordinator start];
}

- (void)hideContextualSheet {
  [_contextualSheetCoordinator stop];
  _contextualSheetCoordinator = nil;
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  [self.defaultBrowserGenericPromoCoordinator stop];
  self.defaultBrowserGenericPromoCoordinator = nil;
}

#pragma mark - DriveFilePickerCommands

- (void)showDriveFilePicker {
  if (!base::FeatureList::IsEnabled(kIOSChooseFromDrive)) {
    return;
  }
  // If there is a coordinator, stop it before showing it again.
  [self hideDriveFilePicker];
  // Return early if the current WebState is not choosing files.
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState || activeWebState->IsBeingDestroyed()) {
    // If there is no active WebState or it is being destroyed, do nothing.
    return;
  }
  ChooseFileTabHelper* tab_helper =
      ChooseFileTabHelper::GetOrCreateForWebState(activeWebState);
  if (!tab_helper->IsChoosingFiles()) {
    return;
  }
  // Start the coordinator.
  _driveFilePickerCoordinator = [[RootDriveFilePickerCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        webState:activeWebState];
  [_driveFilePickerCoordinator start];
}

- (void)hideDriveFilePicker {
  [_driveFilePickerCoordinator stop];
  _driveFilePickerCoordinator = nil;
}

- (void)setDriveFilePickerSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  [_driveFilePickerCoordinator setSelectedIdentity:selectedIdentity];
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

  if (!self.browser->GetProfile()->IsOffTheRecord()) {
    helper->PersistSearchTerm();
  }
}

- (void)findNextStringInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  // TODO(crbug.com/40465124): Reshow find bar if necessary.
  GetConcreteFindTabHelperFromWebState(activeWebState)
      ->ContinueFinding(JavaScriptFindTabHelper::FORWARD);
}

- (void)findPreviousStringInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  // TODO(crbug.com/40465124): Reshow find bar if necessary.
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

#pragma mark - AddContactsCommands

- (void)presentAddContactsForPhoneNumber:(NSString*)phoneNumber {
  _addContactsCoordinator = [[AddContactsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                     phoneNumber:phoneNumber];
  [_addContactsCoordinator start];
}

- (void)hideAddContacts {
  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;
}

#pragma mark - CountryCodePickerCommands

- (void)presentCountryCodePickerForPhoneNumber:(NSString*)phoneNumber {
  _countryCodePickerCoordinator = [[CountryCodePickerCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _countryCodePickerCoordinator.phoneNumber = phoneNumber;
  [_countryCodePickerCoordinator start];
}

- (void)hideCountryCodePicker {
  [_countryCodePickerCoordinator stop];
  _countryCodePickerCoordinator = nil;
}

#pragma mark - PromosManagerCommands

- (void)maybeDisplayPromo {
  if (!self.promosManagerCoordinator) {
    id<CredentialProviderPromoCommands> credentialProviderPromoHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(),
                           CredentialProviderPromoCommands);
    id<DockingPromoCommands> dockingPromoHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), DockingPromoCommands);

    self.promosManagerCoordinator = [[PromosManagerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
        credentialProviderPromoHandler:credentialProviderPromoHandler
                   dockingPromoHandler:dockingPromoHandler];

    // CredentialProviderPromoCoordinator is initialized earlier than this, so
    // make sure to set its UI handler.
    _credentialProviderPromoCoordinator.promosUIHandler =
        self.promosManagerCoordinator;

    // _dockingPromoCoordinator is initialized earlier than this, so
    // make sure to set its UI handler.
    _dockingPromoCoordinator.promosUIHandler = self.promosManagerCoordinator;

    [self.promosManagerCoordinator start];
  } else {
    [self.promosManagerCoordinator displayPromoIfAvailable];
  }
}

- (void)requestAppStoreReview {
  if (IsAppStoreRatingEnabled()) {
    UIWindowScene* scene = [self.browser->GetSceneState() scene];
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

- (void)maybeDisplayDefaultBrowserPromo {
  if (self.defaultBrowserGenericPromoCoordinator) {
    // The default browser promo manager is already being displayed. Early
    // return as this is expected if a default browser promo was open and the
    // app was backgrounded.
    return;
  }

  self.defaultBrowserGenericPromoCoordinator =
      [[DefaultBrowserGenericPromoCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.defaultBrowserGenericPromoCoordinator.promosUIHandler =
      self.promosManagerCoordinator;
  self.defaultBrowserGenericPromoCoordinator.handler = self;
  [self.defaultBrowserGenericPromoCoordinator start];
}

- (void)displayDefaultBrowserPromoAfterRemindMeLater {
  self.defaultBrowserGenericPromoCoordinator =
      [[DefaultBrowserGenericPromoCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.defaultBrowserGenericPromoCoordinator.promosUIHandler =
      self.promosManagerCoordinator;
  self.defaultBrowserGenericPromoCoordinator.handler = self;
  self.defaultBrowserGenericPromoCoordinator.promoWasFromRemindMeLater = YES;
  [self.defaultBrowserGenericPromoCoordinator start];
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

#pragma mark - FormInputAccessoryCoordinatorNavigator

- (void)openPasswordManager {
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showSavedPasswordsSettingsFromViewController:self.viewController
                                  showCancelButton:YES];
}

- (void)openPasswordSettings {
  // TODO(crbug.com/40067451): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordSettingsCoordinator);

  // Use main browser to open the password settings.
  SceneState* sceneState = self.browser->GetSceneState();
  self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:sceneState.browserProviderInterface
                                     .mainBrowserProvider.browser];
  self.passwordSettingsCoordinator.delegate = self;
  [self.passwordSettingsCoordinator start];
}

- (void)openAddressSettings {
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showProfileSettingsFromViewController:self.viewController];
}

- (void)openCreditCardSettings {
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
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
  _nextToolbarToPresent = std::nullopt;

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

  AccountConsistencyBrowserAgent::CreateForBrowser(self.browser,
                                                   self.viewController);

  if (FollowBrowserAgent::FromBrowser(self.browser)) {
    CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
    FollowBrowserAgent::FromBrowser(self.browser)
        ->SetUIProviders(
            HandlerForProtocol(commandDispatcher, NewTabPageCommands),
            static_cast<id<SnackbarCommands>>(commandDispatcher),
            HandlerForProtocol(commandDispatcher, FeedCommands));
  }
}

// Installs delegates for self.browser->GetProfile()
- (void)installDelegatesForBrowserState {
  ProfileIOS* profile = self.browser->GetProfile();
  if (profile) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForProfile(profile)
        ->SetWebStateList(self.browser->GetWebStateList());
  }
}

// Uninstalls delegates for self.browser->GetProfile()
- (void)uninstallDelegatesForBrowserState {
  ProfileIOS* profile = self.browser->GetProfile();
  if (profile) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForProfile(profile)
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
          self.browser->GetProfile());
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
          self.browser->GetProfile());
  if (!shoppingService) {
    return;
  }
  if (IsUserEligibleParcelTrackingOptInPrompt(
          self.browser->GetProfile()->GetPrefs(), shoppingService)) {
    [self showParcelTrackingOptInPromptWithParcels:parcels];
  } else {
    [self maybeShowParcelTrackingInfobarWithParcels:parcels];
  }
}

- (void)showParcelTrackingInfobarWithParcels:
            (NSArray<CustomTextCheckingResult*>*)parcels
                                     forStep:(ParcelTrackingStep)step {
  web::WebState* activeWebState = self.activeWebState;
  if(!activeWebState) {
    return;
  }
  ProfileIOS* profile = self.browser->GetProfile();
  if (!commerce::ShoppingServiceFactory::GetForBrowserState(profile)
           ->IsParcelTrackingEligible()) {
    return;
  }
  if (step == ParcelTrackingStep::kNewPackageTracked) {
    feature_engagement::Tracker* engagementTracker =
        feature_engagement::TrackerFactory::GetForProfile(profile);
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
  [HandlerForProtocol(_dispatcher, HelpCommands)
      presentInProductHelpWithType:InProductHelpType::kParcelTracking];
}

#pragma mark - ParcelTrackingOptInCommands helpers

- (void)maybeShowParcelTrackingInfobarWithParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels {
  IOSParcelTrackingOptInStatus optInStatus =
      static_cast<IOSParcelTrackingOptInStatus>(
          self.browser->GetProfile()->GetPrefs()->GetInteger(
              prefs::kIosParcelTrackingOptInStatus));
  switch (optInStatus) {
    case IOSParcelTrackingOptInStatus::kAlwaysTrack: {
      web::WebState* activeWebState = self.activeWebState;
      if (!activeWebState) {
        return;
      }
      commerce::ShoppingService* shoppingService =
          commerce::ShoppingServiceFactory::GetForBrowserState(
              ProfileIOS::FromBrowserState(activeWebState->GetBrowserState()));
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
    case IOSParcelTrackingOptInStatus::kStatusNotSet:
      // Do not display infobar.
      break;
  }
}

- (void)showParcelTrackingOptInPromptWithParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels {
  [self dismissParcelTrackingOptInPrompt];
  self.parcelTrackingOptInCoordinator = [[ParcelTrackingOptInCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
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
                     proactive:(BOOL)proactive
                      webState:(web::WebState*)webState
               decisionHandler:(void (^)(BOOL accept))decisionHandler {
  // Do not present the bottom sheet if the calling web state does not match the
  // active web state in order to stop the bottom sheet from showing in a tab
  // different than the one that triggered it.
  if (webState != self.activeWebState) {
    return;
  }

  // Do not present the bottom sheet if it is already being presented.
  if (self.passwordSuggestionCoordinator) {
    return;
  }

  self.passwordSuggestionCoordinator = [[PasswordSuggestionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
              passwordSuggestion:passwordSuggestion
                 decisionHandler:decisionHandler
                       proactive:proactive];
  self.passwordSuggestionCoordinator.delegate = self;
  [self.passwordSuggestionCoordinator start];
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
  [HandlerForProtocol(_dispatcher, HelpCommands)
      presentInProductHelpWithType:InProductHelpType::
                                       kPriceNotificationsWhileBrowsing];
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
  SceneState* sceneState = self.browser->GetSceneState();
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

#pragma mark - SaveToDriveCommands

- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask {
  // If the Save to Drive coordinator is not nil, stop it.
  [self hideSaveToDrive];

  _saveToDriveCoordinator = [[SaveToDriveCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                    downloadTask:downloadTask];
  [_saveToDriveCoordinator start];
}

- (void)hideSaveToDrive {
  [_saveToDriveCoordinator stop];
  _saveToDriveCoordinator = nil;
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
  SceneState* sceneState = self.browser->GetSceneState();
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
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showSyncPassphraseSettingsFromViewController:self.viewController];
}

- (void)showGoogleServicesSettings {
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showGoogleServicesSettingsFromViewController:self.viewController];
}

- (void)showAccountSettings {
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showAccountsSettingsFromViewController:self.viewController
                        skipIfUINotAvailable:NO];
}

- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                          securityDomainID:
                                              trusted_vault::SecurityDomainId::
                                                  kChromeSync
                                                   trigger:trigger
                                               accessPoint:
                                                   signin_metrics::AccessPoint::
                                                       ACCESS_POINT_SETTINGS];
}

- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                       securityDomainID:
                                                           trusted_vault::
                                                               SecurityDomainId::
                                                                   kChromeSync
                                                                trigger:trigger
                                                            accessPoint:
                                                                signin_metrics::
                                                                    AccessPoint::
                                                                        ACCESS_POINT_SETTINGS];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
              showSignin:command
      baseViewController:self.viewController];
}

#pragma mark - SnapshotGeneratorDelegate methods
// TODO(crbug.com/40206055): Refactor SnapshotGenerator into (probably) a
// mediator with a narrowly-defined API to get UI-layer information from the
// BVC.

- (BOOL)canTakeSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState) {
    return NO;
  }
  PagePlaceholderTabHelper* pagePlaceholderTabHelper =
      PagePlaceholderTabHelper::FromWebState(webState);
  return !pagePlaceholderTabHelper->displaying_placeholder() &&
         !pagePlaceholderTabHelper->will_add_placeholder_for_next_navigation();
}

- (void)willUpdateSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState) {
    return;
  }

  if ([self isNTPActiveForCurrentWebState]) {
    [_NTPCoordinator willUpdateSnapshot];
  }
  OverscrollActionsTabHelper::FromWebState(webState)->Clear();
}

- (UIEdgeInsets)snapshotEdgeInsetsWithWebStateInfo:
    (WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState) {
    return UIEdgeInsetsZero;
  }

  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(webState);
  bool isLensOverlayAvailable =
      IsLensOverlayAvailable() && lensOverlayTabHelper;

  bool isBuildingLensOverlay =
      isLensOverlayAvailable &&
      lensOverlayTabHelper->IsCapturingLensOverlaySnapshot();
  bool isUpdatingLensOverlayTabSwitcherSnapshot =
      isLensOverlayAvailable &&
      lensOverlayTabHelper->IsUpdatingTabSwitcherSnapshot();

  if (isUpdatingLensOverlayTabSwitcherSnapshot && _safeAreaProvider) {
    return _safeAreaProvider.safeArea;
  } else if (isBuildingLensOverlay) {
    return lensOverlayTabHelper->GetSnapshotInsets();
  }

  UIEdgeInsets maxViewportInsets =
      _fullscreenController->GetMaxViewportInsets();

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, or for the incognito NTP, the NTP is laid
    // out between the toolbars, so it should not be inset while snapshotting.
    if (canShowTabStrip || self.browser->GetProfile()->IsOffTheRecord()) {
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

- (NSArray<UIView*>*)snapshotOverlaysWithWebStateInfo:
    (WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState) {
    return @[];
  }

  WebStateList* webStateList = self.browser->GetWebStateList();

  if (webStateList->GetIndexOfWebState(webState) ==
      WebStateList::kInvalidIndex) {
    return @[];
  }

  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(webState);

  BOOL webStateHasLensOverlay = IsLensOverlayAvailable() &&
                                lensOverlayTabHelper &&
                                lensOverlayTabHelper->IsLensOverlayShown();

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];

  // A lens overlay is mapped to the given web state.
  if (webStateHasLensOverlay) {
    UIView* lensOverlayView = _lensOverlayCoordinator.viewController.view;

    if (lensOverlayView) {
      [overlays addObject:lensOverlayView];
    }
  }

  // If the given web state is inactive or web usage is disabled, refrain from
  // adding any additional overlays. For inactive web states, only the lens
  // overlay is permitted to be added.
  if (!self.webUsageEnabled || webState != webStateList->GetActiveWebState()) {
    return overlays;
  }

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

- (UIView*)baseViewWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState) {
    return nil;
  }
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

- (void)handleFeedModelOfType:(FeedType)feedType
                didEndUpdates:(FeedLayoutUpdateType)updateType {
  [_NTPCoordinator handleFeedModelOfType:feedType didEndUpdates:updateType];
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

- (void)presentLensIconBubble {
  __weak NewTabPageCoordinator* weakNTPCoordinator = _NTPCoordinator;
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      prepareToPresentModal:^{
        [weakNTPCoordinator presentLensIconBubble];
      }];
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
  [_omniboxCommandsHandler cancelOmniboxEdit];
}

- (CGPoint)convertToPresentationCoordinatesForOrigin:(CGPoint)origin {
  return [self.viewController.view convertPoint:origin fromView:nil];
}

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordSettingsCoordinator, coordinator);

  [self stopPasswordSettingsCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [self stopPasswordSettingsCoordinator];
}

#pragma mark - ReadingListCoordinatorDelegate

- (void)closeReadingList {
  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;
}

#pragma mark - BubblePresenterDelegate

- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter {
  return self.viewController.viewVisible;
}

- (BOOL)isNTPActiveForBubblePresenter:(BubblePresenter*)bubblePresenter {
  return self.NTPCoordinator.isNTPActiveForCurrentWebState;
}

- (BOOL)isNTPScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter {
  return [self.NTPCoordinator isScrolledToTop];
}

- (BOOL)isOverscrollActionsSupportedForBubblePresenter:
    (BubblePresenter*)bubblePresenter {
  return [self shouldAllowOverscrollActions];
}

- (void)bubblePresenterDidPerformPullToRefreshGesture:
    (BubblePresenter*)bubblePresenter {
  if (!self.activeWebState) {
    return;
  }
  OverscrollActionsTabHelper* tabHelper =
      OverscrollActionsTabHelper::FromWebState(self.activeWebState);
  OverscrollActionsController* controller =
      tabHelper->GetOverscrollActionsController();
  [controller forceAnimatedScrollRefresh];
}

- (void)bubblePresenter:(BubblePresenter*)bubblePresenter
    didPerformSwipeToNavigateInDirection:
        (UISwipeGestureRecognizerDirection)direction {
  [_sideSwipeMediator animateSwipe:SwipeType::CHANGE_PAGE
                       inDirection:direction];
}

#pragma mark - OverscrollActionsControllerDelegate methods.

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  [applicationCommandsHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithIncognito:self.browser->GetProfile()
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
  ProfileIOS* profile = self.browser->GetProfile();
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  if (engagementTracker) {
    engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSPullToRefreshUsed);
  }

  SnapshotTabHelper::FromWebState(activeWebState)->IgnoreNextLoad();
  _webNavigationBrowserAgent->Reload();
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self shouldAllowOverscrollActions];
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
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(_dispatcher, SettingsCommands);
  [settingsHandler
      showSavedPasswordsSettingsFromViewController:self.viewController
                                  showCancelButton:YES];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(_dispatcher, SettingsCommands);
  [settingsHandler showPasswordDetailsForCredential:credential inEditMode:NO];
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

#pragma mark - WebUsageEnablerBrowserAgentObserving

- (void)webUsageEnablerValueChanged:
    (WebUsageEnablerBrowserAgent*)webUsageEnabler {
  self.active = WebUsageEnablerBrowserAgent::FromBrowser(self.browser)
                    ->IsWebUsageEnabled();
}
#pragma mark - QuickDeleteCommands

- (void)showQuickDeleteAndCanPerformTabsClosureAnimation:
    (BOOL)canPerformTabsClosureAnimation {
  CHECK(IsIosQuickDeleteEnabled());
  CHECK(!self.browser->GetProfile()->IsOffTheRecord());

  [_quickDeleteCoordinator stop];

  SceneState* sceneState = self.browser->GetSceneState();
  _quickDeleteCoordinator = [[QuickDeleteCoordinator alloc]
          initWithBaseViewController:
              top_view_controller::TopPresentedViewControllerFrom(
                  sceneState.window.rootViewController)
                             browser:self.browser
      canPerformTabsClosureAnimation:canPerformTabsClosureAnimation];
  [_quickDeleteCoordinator start];
}

- (void)stopQuickDelete {
  CHECK(IsIosQuickDeleteEnabled());
  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;
}

- (void)stopQuickDeleteForAnimationWithCompletion:(ProceduralBlock)completion {
  CHECK(IsIosQuickDeleteEnabled());

  // TODO(crbug.com/335387869): Remove NotFatalUntil and the if below when we're
  // sure this code path is infeasible. The BrowserViewController should always
  // have at least the QuickDeleteViewController on top of it.
  CHECK(self.viewController.presentedViewController, base::NotFatalUntil::M133);

  // If BrowserViewController has not presented any view controller, then
  // trigger `completion` immediately.
  if (!self.viewController.presentedViewController) {
    completion();
    [self stopQuickDelete];
    return;
  }

  // If BrowserViewController has presented a view controller, then dismiss
  // every VC on top of it.
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissalCompletion = ^{
    if (completion) {
      completion();
    }

    // Properly shutdown all coordinators started either by this coordinator or
    // by the scene controller. This should include Quick Delete, History and
    // the Privacy Settings.
    [weakSelf clearPresentedStateWithCompletion:nil dismissOmnibox:YES];
    [applicationCommandsHandler dismissModalDialogsWithCompletion:nil];
  };
  [self.viewController dismissViewControllerAnimated:YES
                                          completion:dismissalCompletion];
}

#pragma mark - WhatsNewCommands

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
  [HandlerForProtocol(_dispatcher, HelpCommands)
      presentInProductHelpWithType:InProductHelpType::kWhatsNew];
}

@end

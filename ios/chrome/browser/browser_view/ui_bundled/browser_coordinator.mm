// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"

#import <StoreKit/StoreKit.h>

#import <memory>
#import <optional>

#import "base/check_deref.h"
#import "base/check_op.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/feature_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/core/common/features.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/send_tab_to_self/features.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_prompt/enterprise_prompt_type.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_coordinator.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/infobar_autofill_edit_profile_bottom_sheet_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_coordinator.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_view_controller.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator+Testing.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_omnibox_state_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller+private.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/browser_view/ui_bundled/key_commands_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_events_mediator.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_lifecycle_mediator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_delegate.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_coordinator.h"
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
#import "ios/chrome/browser/download/coordinator/ar_quick_look_coordinator.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_coordinator.h"
#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"
#import "ios/chrome/browser/download/coordinator/download_manager_coordinator.h"
#import "ios/chrome/browser/download/coordinator/pass_kit_coordinator.h"
#import "ios/chrome/browser/download/coordinator/safari_download_coordinator.h"
#import "ios/chrome/browser/download/coordinator/vcard_coordinator.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/enterprise/data_controls/coordinator/data_controls_dialog_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_util.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_coordinator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_reason.h"
#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_coordinator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_coordinator.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_coordinator.h"
#import "ios/chrome/browser/page_info/ui_bundled/requirements/page_info_presentation.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"
#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_coordinator.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent_delegate.h"
#import "ios/chrome/browser/presenters/ui_bundled/vertical_animation_container.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/price_notifications_view_coordinator.h"
#import "ios/chrome/browser/print/coordinator/print_coordinator.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_coordinator.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/ui_bundled/notifications_opt_in_coordinator.h"
#import "ios/chrome/browser/push_notification/ui_bundled/notifications_opt_in_coordinator_delegate.h"
#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_blur_overlay_coordinator.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_coordinator.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent_delegate.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_coordinator.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_coordinator_delegate.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_coordinator.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"
#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_coordinator.h"
#import "ios/chrome/browser/safe_browsing/ui_bundled/safe_browsing_coordinator.h"
#import "ios/chrome/browser/save_to_drive/ui_bundled/save_to_drive_coordinator.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_share_url_command.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/collaboration_group_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/enhanced_calendar_commands.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_protection_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_tracked_items_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/reminder_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_command.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/commands/welcome_back_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/page_animation_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_positioner.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_coordinator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"
#import "ios/chrome/browser/spotlight_debugger/ui_bundled/spotlight_debugger_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_coordinator.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/tips_notifications/coordinator/enhanced_safe_browsing_promo_coordinator.h"
#import "ios/chrome/browser/tips_notifications/coordinator/lens_promo_coordinator.h"
#import "ios/chrome/browser/tips_notifications/coordinator/search_what_you_see_promo_coordinator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_coordinator.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/view_source/model/view_source_browser_agent.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
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
#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_coordinator.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_coordinator.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// URL to share when user selects "Share Chrome"
const char kChromeAppStoreUrl[] =
    "https://apps.apple.com/app/id535886823?pt=9008&ct=iosChromeShare&mt=8";

}  // anonymous namespace

@interface BrowserCoordinator () <
    ActivityServiceCommands,
    AddContactsCommands,
    AppLauncherTabHelperBrowserPresentationProvider,
    AutoDeletionCommands,
    AutofillAddCreditCardCoordinatorDelegate,
    BrowserCoordinatorCommands,
    BubblePresenterDelegate,
    CollaborationGroupCommands,
    ContextualPanelEntrypointIPHCommands,
    ContextualSheetCommands,
    CountryCodePickerCommands,
    DataControlsCommands,
    DefaultBrowserGenericPromoCommands,
    DefaultPromoNonModalPresentationDelegate,
    DownloadListCommands,
    DriveFilePickerCommands,
    EnhancedCalendarCommands,
    EditMenuBuilder,
    EnterprisePromptCoordinatorDelegate,
    FileUploadPanelCommands,
    FindInPageCommands,
    FormInputAccessoryCoordinatorNavigator,
    BWGCommands,
    GoogleOneCommands,
    MiniMapCommands,
    NetExportTabHelperDelegate,
    NewTabPageCommands,
    NonModalSignInPromoCommands,
    NonModalSignInPromoCoordinatorDelegate,
    NotificationsOptInCoordinatorDelegate,
    OverscrollActionsControllerDelegate,
    PageActionMenuCommands,
    PageInfoCommands,
    PageInfoPresentation,
    ParentAccessCommands,
    PasswordBreachCommands,
    PasswordControllerDelegate,
    PasswordProtectionCommands,
    PasswordProtectionCoordinatorDelegate,
    PasswordSettingsCoordinatorDelegate,
    PasswordSuggestionCommands,
    PasswordSuggestionCoordinatorDelegate,
    PriceTrackedItemsCommands,
    PromosManagerCommands,
    PolicyChangeCommands,
    SendTabToSelfCoordinatorDelegate,
    SyncedSetUpCoordinatorDelegate,
    SyncedSetUpCommands,
    PrerenderBrowserAgentDelegate,
    QuickDeleteCommands,
    ReaderModeBrowserAgentDelegate,
    ReaderModeCommands,
    ReaderModeCoordinatorDelegate,
    ReadingListCoordinatorDelegate,
    RecentTabsCoordinatorDelegate,
    ReminderNotificationsCommands,
    RepostFormCoordinatorDelegate,
    RepostFormTabHelperDelegate,
    ReSigninPresenter,
    SaveToDriveCommands,
    SaveToPhotosCommands,
    SigninPresenter,
    SnapshotGeneratorDelegate,
    StoreKitCoordinatorDelegate,
    TrustedVaultReauthenticationCoordinatorDelegate,
    UnitConversionCommands,
    URLLoadingDelegate,
    WebContentCommands,
    WebNavigationNTPDelegate,
    WebUsageEnablerBrowserAgentObserving,
    WelcomeBackPromoCommands,
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
@property(nonatomic, strong) CredentialSuggestionBottomSheetCoordinator*
    credentialSuggestionBottomSheetCoordinator;

// Coordinator in charge of the presenting autofill options in a bottom sheet.
@property(nonatomic, strong) PaymentsSuggestionBottomSheetCoordinator*
    paymentsSuggestionBottomSheetCoordinator;

// Coordinator for the authentication when unmasking card during autofill.
@property(nonatomic, strong)
    CardUnmaskAuthenticationCoordinator* cardUnmaskAuthenticationCoordinator;

@property(nonatomic, strong)
    PlusAddressBottomSheetCoordinator* plusAddressBottomSheetCoordinator;

@property(nonatomic, strong)
    AutofillEditProfileCoordinator* autofillEditProfileCoordinator;

@property(nonatomic, strong)
    SaveCardBottomSheetCoordinator* saveCardBottomSheetCoordinator;

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

// Coordinator that manages the presentation of Download List UI.
@property(nonatomic, strong) DownloadListCoordinator* downloadListCoordinator;

// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;

// Coordinator to show the Autofill error dialog.
@property(nonatomic, strong)
    AutofillErrorDialogCoordinator* autofillErrorDialogCoordinator;

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

// Coordinator to display local web approvals parent access UI in a bottom
// sheet.
@property(nonatomic, strong) ParentAccessCoordinator* parentAccessCoordinator;

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
    PriceNotificationsViewCoordinator* priceNotificationsViewCoordinator;

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

// The handler used to manage the infobar workflow for saving an address.
@property(nonatomic, strong)
    InfobarAutofillEditProfileBottomSheetHandler* editProfileBottomSheetHandler;
// The coordinator in charge of the non modal sign in promo.
@property(nonatomic, strong)
    NonModalSignInPromoCoordinator* nonModalSignInPromoCoordinator;

// Coordinator for the composebox.
@property(nonatomic, strong) ComposeboxCoordinator* composeboxCoordinator;

@end

@implementation BrowserCoordinator {
  SigninCoordinator* _signinCoordinator;
  BrowserViewControllerDependencies _viewControllerDependencies;
  KeyCommandsProvider* _keyCommandsProvider;
  BubblePresenterCoordinator* _bubblePresenterCoordinator;
  BubbleViewControllerPresenter* _contextualPanelEntrypointHelpPresenter;
  ToolbarAccessoryPresenter* _toolbarAccessoryPresenter;
  LensCoordinator* _lensCoordinator;
  LensViewFinderCoordinator* _lensViewFinderCoordinator;
  LensOverlayCoordinator* _lensOverlayCoordinator;
  ToolbarCoordinator* _toolbarCoordinator;
  BrowserOmniboxStateProvider* _browserOmniboxStateProvider;
  TabStripCoordinator* _tabStripCoordinator;
  SideSwipeCoordinator* _sideSwipeCoordinator;
  raw_ptr<FullscreenController> _fullscreenController;
  // The coordinator that shows the Send Tab To Self UI.
  SendTabToSelfCoordinator* _sendTabToSelfCoordinator;
  BookmarksCoordinator* _bookmarksCoordinator;
  CredentialProviderPromoCoordinator* _credentialProviderPromoCoordinator;
  DockingPromoCoordinator* _dockingPromoCoordinator;
  // Used to display the Voice Search UI.  Nil if not visible.
  id<VoiceSearchController> _voiceSearchController;
  raw_ptr<UrlLoadingNotifierBrowserAgent, DanglingUntriaged>
      _urlLoadingNotifierBrowserAgent;
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
  API_AVAILABLE(ios(18.4))
  FileUploadPanelCoordinator* _fileUploadPanelCoordinator;
  RootDriveFilePickerCoordinator* _driveFilePickerCoordinator;
  GoogleOneCoordinator* _googleOneCoordinator;

  // Coordinator to display a web page in Reader Mode UI.
  ReaderModeCoordinator* _readerModeCoordinator;
  ReaderModeBlurOverlayCoordinator* _readerModeBlurOverlayCoordinator;

  // Coordinator to display the "Set a reminder" screen for the user's current
  // tab.
  ReminderNotificationsCoordinator* _reminderNotificationsCoordinator;
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
  AutoDeletionCoordinator* _autoDeletionCoordinator;
  TrustedVaultReauthenticationCoordinator*
      _trustedVaultReauthenticationCoordinator;

  // The coordinator for the Enhanced Calendar feature UI (bottom sheet).
  EnhancedCalendarCoordinator* _enhancedCalendarCoordinator;

  // The coordinator for the page action menu.
  PageActionMenuCoordinator* _pageActionMenuCoordinator;

  // Coordinator that handles confirmation dialog when the last tab of a shared
  // group is closed.
  TabGroupConfirmationCoordinator* _lastTabClosingAlert;

  // The coordinator for BWG related logic.
  BWGCoordinator* _BWGCoordinator;

  // The coordinator for the Search What You See promo.
  SearchWhatYouSeePromoCoordinator* _searchWhatYouSeePromoCoordinator;

  // The coordinator for the notifications opt-in screen.
  NotificationsOptInCoordinator* _notificationsOptInCoordinator;

  // The coordinator for the Welcome Back promo.
  WelcomeBackCoordinator* _welcomeBackCoordinator;

  // The coordinator for displaying Enterprise Data Controls dialogs.
  DataControlsDialogCoordinator* _dataControlsDialogCoordinator;

  // The coordinator for managing the Synced Set Up flow.
  SyncedSetUpCoordinator* _syncedSetUpCoordinator;

  // Block to run after the Synced Set Up UI has finished dismissing.
  ProceduralBlock _runAfterSyncedSetUpDismissal;
}

#pragma mark - ReaderModeBrowserAgentDelegate

- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
           showContentAnimated:(BOOL)animated {
  if (_readerModeCoordinator) {
    return;
  }
  _readerModeCoordinator = [[ReaderModeCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:self.browser];
  _readerModeCoordinator.delegate = self;
  [_readerModeCoordinator setOverscrollDelegate:self];
  [_readerModeCoordinator startAnimated:animated];
}

- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
           hideContentAnimated:(BOOL)animated {
  if (!_readerModeCoordinator) {
    return;
  }
  [_readerModeCoordinator stopAnimated:animated];
  _readerModeCoordinator = nil;
}

#pragma mark - ReaderModeCoordinatorDelegate

- (void)readerModeCoordinatorAnimationDidComplete:
    (ReaderModeCoordinator*)coordinator {
  [self hideReaderModeBlurOverlay];
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

  CHECK_GE(_numberOfActivityOverly, 0);
  if (_numberOfActivityOverly > 0 && !self.activityOverlayCoordinator) {
    // The activity overlay was requested before the UI got ready, need to start
    // the overlay now.
    [self startActivityOverlay];
  }
}

- (void)stop {
  if (!self.started) {
    return;
  }

  self.started = NO;
  [super stop];
  self.active = NO;
  [self stopSigninCoordinator];
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

  if (self.profile) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForProfile(self.profile)
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
    if (IsVisibleURLNewTabPage(webState) && !self.NTPCoordinator.started) {
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
  if (@available(iOS 18.4, *)) {
    if (base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
      [self hideFileUploadPanel];
    }
  }
  if (IsDownloadListEnabled()) {
    [self hideDownloadList];
  }

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  [self.printCoordinator dismissAnimated:YES];

  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;

  [_readerModeCoordinator stopAnimated:YES];
  _readerModeCoordinator = nil;
  [self hideReaderModeBlurOverlay];

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self stopPasswordProtectionCoordinator];

  [self.credentialSuggestionBottomSheetCoordinator stop];
  self.credentialSuggestionBottomSheetCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self hidePageInfo];

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [self.plusAddressBottomSheetCoordinator stop];
  self.plusAddressBottomSheetCoordinator = nil;

  [self dismissSaveCardBottomSheet];

  [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  self.virtualCardEnrollmentBottomSheetCoordinator = nil;

  [self dismissAutofillErrorDialog];

  [self dismissAutofillProgressDialog];

  [self stopSendTabToSelf];

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self hidePriceTrackedItems];

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;

  [self stopRepostFormCoordinator];

  [_formInputAccessoryCoordinator clearPresentedState];

  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;

  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;

  [_countryCodePickerCoordinator stop];
  _countryCodePickerCoordinator = nil;

  [_lastTabClosingAlert stop];
  _lastTabClosingAlert = nil;

  [_dataControlsDialogCoordinator stop];
  _dataControlsDialogCoordinator = nil;

  if (IsSyncedSetUpEnabled()) {
    [self stopSyncedSetUpCoordinator];
  }

  [self hideGoogleOne];
  [self updateLensUIForBackground];

  [self dismissLensPromo];
  [self dismissEnhancedSafeBrowsingPromo];
  [self dismissAutoDeletionActionSheet];
  [self dismissSearchWhatYouSeePromo];
  [self dismissNotificationsOptIn];
  [self hideWelcomeBackPromo];

  [self cancelCollaborationFlows];
  [self.NTPCoordinator clearPresentedState];

  // The composebox replaces the omnibox.
  if (dismissOmnibox) {
    [self hideComposeboxImmediately:NO];
  }

  BOOL dismissPresentedViewController = YES;
  if (IsComposeboxIOSEnabled()) {
    dismissPresentedViewController =
        dismissOmnibox || !_composeboxCoordinator.presented;
  }

  [self.viewController
      clearPresentedStateWithCompletion:completion
                         dismissOmnibox:dismissOmnibox
         dismissPresentedViewController:dismissPresentedViewController];
}

#pragma mark - Private

- (void)stopSendTabToSelf {
  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator.delegate = nil;
  _sendTabToSelfCoordinator = nil;
}

// Stops the Synced Set Up coordinator.
- (void)stopSyncedSetUpCoordinator {
  [_syncedSetUpCoordinator stop];
  _syncedSetUpCoordinator.delegate = nil;
  _syncedSetUpCoordinator = nil;

  if (_runAfterSyncedSetUpDismissal) {
    ProceduralBlock completion = [_runAfterSyncedSetUpDismissal copy];
    _runAfterSyncedSetUpDismissal = nil;
    completion();
  }
}

- (void)signinCoordinatorCompletionWithCoordinator:
    (SigninCoordinator*)coordinator {
  CHECK(!coordinator || _signinCoordinator == coordinator,
        base::NotFatalUntil::M151);
  [self stopSigninCoordinator];
}

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)stopTrustedVaultReauthentication {
  [_trustedVaultReauthenticationCoordinator stop];
  _trustedVaultReauthenticationCoordinator.delegate = nil;
  _trustedVaultReauthenticationCoordinator = nil;
}

// The Lens UI takes the necessary steps before being backgrounded.
- (void)updateLensUIForBackground {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }

  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(activeWebState);
  bool isLensOverlayAvailable =
      IsLensOverlayAvailable(self.profile->GetPrefs()) && lensOverlayTabHelper;

  if (isLensOverlayAvailable &&
      lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive()) {
    [HandlerForProtocol(_dispatcher, LensOverlayCommands)
        prepareLensUIForBackgroundTabChange];
  }
}

// Returns whether overscroll actions should be allowed. When screeen size is
// not regular, they should be enabled.
- (BOOL)shouldAllowOverscrollActions {
  return !_toolbarAccessoryPresenter.presenting &&
         !IsRegularXRegularSizeClass(self.viewController);
}

// Display price tracking menu, optionally showing the current page the user
// is navigated to.
- (void)showPriceTrackedItems:(BOOL)showCurrentPage {
  self.priceNotificationsViewCoordinator =
      [[PriceNotificationsViewCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  self.priceNotificationsViewCoordinator.showCurrentPage = showCurrentPage;
  [self.priceNotificationsViewCoordinator start];
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

// Stops the reminder notifications coordinator.
- (void)stopReminderNotificationsCoordinator {
  [_reminderNotificationsCoordinator stop];
  _reminderNotificationsCoordinator = nil;
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

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (!self.profile || !self.started) {
    return;
  }
  _webUsageEnabled = webUsageEnabled;
  self.viewController.webUsageEnabled = webUsageEnabled;
}

// Starts the activity overlay that was requested using `-showActivityOverlay`.
- (void)startActivityOverlay {
  CHECK(self.viewController);
  self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.activityOverlayCoordinator start];
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

  id<BrowserViewVisibilityAudience> visibilityAudience =
      BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(self.browser)
          ->GetBrowserViewVisibilityAudience();
  _viewController.browserViewVisibilityAudience = visibilityAudience;
  self.tabLifecycleMediator.baseViewController = self.viewController;
  self.tabLifecycleMediator.passwordControllerDelegate = self;

  _webNavigationBrowserAgent->SetDelegate(self);

  self.contextMenuProvider = [[ContextMenuConfigurationProvider alloc]
         initWithBrowser:self.browser
      baseViewController:_viewController];
}

// Shuts down the BrowserViewController.
- (void)destroyViewController {
  _viewController.active = NO;
  _viewController.webUsageEnabled = NO;
  _viewController.browserViewVisibilityAudience = nil;

  [self.contextMenuProvider stop];
  self.contextMenuProvider = nil;

  // TODO(crbug.com/40256480): Remove when BVC will no longer handle commands.
  [self.dispatcher stopDispatchingToTarget:_viewController];
  [_viewController shutdown];
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
  Browser* browser = self.browser;
  _dispatcher = browser->GetCommandDispatcher();

  // Add commands protocols handled by this class in this array to let the
  // dispatcher know where to dispatch such commands. This must be done before
  // starting any child coordinator, otherwise they won't be able to resolve
  // handlers.
  NSArray<Protocol*>* protocols = @[
    @protocol(ActivityServiceCommands),
    @protocol(AutoDeletionCommands),
    @protocol(AutofillCommands),
    @protocol(BrowserCoordinatorCommands),
    @protocol(CollaborationGroupCommands),
    @protocol(ContextualPanelEntrypointIPHCommands),
    @protocol(ContextualSheetCommands),
    @protocol(DefaultBrowserPromoNonModalCommands),
    @protocol(DownloadListCommands),
    @protocol(DriveFilePickerCommands),
    @protocol(EnhancedCalendarCommands),
    @protocol(PromosManagerCommands),
    @protocol(FileUploadPanelCommands),
    @protocol(FindInPageCommands),
    @protocol(BWGCommands),
    @protocol(ReaderModeCommands),
    @protocol(NewTabPageCommands),
    @protocol(NonModalSignInPromoCommands),
    @protocol(PageActionMenuCommands),
    @protocol(PageInfoCommands),
    @protocol(PasswordBreachCommands),
    @protocol(PasswordProtectionCommands),
    @protocol(PasswordSuggestionCommands),
    @protocol(PolicyChangeCommands),
    @protocol(PriceTrackedItemsCommands),
    @protocol(QuickDeleteCommands),
    @protocol(SaveToDriveCommands),
    @protocol(SaveToPhotosCommands),
    @protocol(SharedTabGroupLastTabAlertCommands),
    @protocol(SyncedSetUpCommands),
    @protocol(TextZoomCommands),
    @protocol(WebContentCommands),
    @protocol(DefaultBrowserGenericPromoCommands),
    @protocol(MiniMapCommands),
    @protocol(ParentAccessCommands),
    @protocol(ReminderNotificationsCommands),
    @protocol(UnitConversionCommands),
    @protocol(AddContactsCommands),
    @protocol(CountryCodePickerCommands),
    @protocol(WhatsNewCommands),
    @protocol(GoogleOneCommands),
    @protocol(WelcomeBackPromoCommands),
    @protocol(DataControlsCommands),
  ];

  for (Protocol* protocol in protocols) {
    [_dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }

  ProfileIOS* profile = browser->GetProfile();

  _keyCommandsProvider = [[KeyCommandsProvider alloc] initWithBrowser:browser];
  _keyCommandsProvider.applicationHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
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

  if (PrerenderBrowserAgent* prerenderBrowserAgent =
          PrerenderBrowserAgent::FromBrowser(self.browser)) {
    prerenderBrowserAgent->SetDelegate(self);
  }

  _fullscreenController = FullscreenController::FromBrowser(browser);
  _layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  _webNavigationBrowserAgent = WebNavigationBrowserAgent::FromBrowser(browser);
  _urlLoadingBrowserAgent = UrlLoadingBrowserAgent::FromBrowser(browser);
  _urlLoadingNotifierBrowserAgent =
      UrlLoadingNotifierBrowserAgent::FromBrowser(browser);

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    _tabStripCoordinator =
        [[TabStripCoordinator alloc] initWithBrowser:browser];
  }

  _bubblePresenterCoordinator =
      [[BubblePresenterCoordinator alloc] initWithBrowser:browser];
  _bubblePresenterCoordinator.bubblePresenterDelegate = self;
  [_bubblePresenterCoordinator start];

  _toolbarCoordinator = [[ToolbarCoordinator alloc] initWithBrowser:browser];

  // The location bar is one of the OmniboxStateProvider because omnibox is
  // used both in browser and lens overlay.
  _browserOmniboxStateProvider = [[BrowserOmniboxStateProvider alloc] init];
  _browserOmniboxStateProvider.locationBarStateProvider = _toolbarCoordinator;
  OmniboxPositionBrowserAgent* omniboxPositionBrowserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(browser);
  if (omniboxPositionBrowserAgent) {
    omniboxPositionBrowserAgent->SetOmniboxStateProvider(
        _browserOmniboxStateProvider);
  }

  _toolbarAccessoryPresenter = [[ToolbarAccessoryPresenter alloc]
              initWithIsIncognito:profile->IsOffTheRecord()
      omniboxPositionBrowserAgent:omniboxPositionBrowserAgent];
  _toolbarAccessoryPresenter.topToolbarLayoutGuide =
      [_layoutGuideCenter makeLayoutGuideNamed:kPrimaryToolbarGuide];
  _toolbarAccessoryPresenter.bottomToolbarLayoutGuide =
      [_layoutGuideCenter makeLayoutGuideNamed:kSecondaryToolbarGuide];

  _sideSwipeCoordinator = [[SideSwipeCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:browser];

  _sideSwipeCoordinator.toolbarInteractionHandler = _toolbarCoordinator;
  _sideSwipeCoordinator.toolbarSnapshotProvider = _toolbarCoordinator;

  [_sideSwipeCoordinator start];

  _bookmarksCoordinator =
      [[BookmarksCoordinator alloc] initWithBrowser:browser];

  self.browserContainerCoordinator =
      [[BrowserContainerCoordinator alloc] initWithBaseViewController:nil
                                                              browser:browser];
  [self.browserContainerCoordinator start];

  self.downloadManagerCoordinator = [[DownloadManagerCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:browser];
  self.downloadManagerCoordinator.presenter =
      [[VerticalAnimationContainer alloc] init];
  self.tabLifecycleMediator.downloadManagerTabHelperDelegate =
      self.downloadManagerCoordinator;

  self.qrScannerCoordinator =
      [[QRScannerLegacyCoordinator alloc] initWithBrowser:browser];

  self.popupMenuCoordinator =
      [[PopupMenuCoordinator alloc] initWithBrowser:browser];
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinator;
  // Coordinator `start` is executed before setting it's `baseViewController`.
  // It is done intentionally, since this does not affecting the coordinator's
  // behavior but helps command handler setup below.
  [self.popupMenuCoordinator start];

  _NTPCoordinator = [[NewTabPageCoordinator alloc]
       initWithBrowser:browser
      componentFactory:[[NewTabPageComponentFactory alloc] init]];
  _NTPCoordinator.toolbarDelegate = _toolbarCoordinator;

  if (IsLVFUnifiedExperienceEnabled(profile->GetPrefs())) {
    _lensViewFinderCoordinator =
        [[LensViewFinderCoordinator alloc] initWithBrowser:browser];
  } else {
    _lensCoordinator = [[LensCoordinator alloc] initWithBrowser:browser];
  }

  _safeAreaProvider = [[SafeAreaProvider alloc] initWithBrowser:browser];

  _voiceSearchController = ios::provider::CreateVoiceSearchController(browser);

  _viewControllerDependencies.toolbarAccessoryPresenter =
      _toolbarAccessoryPresenter;
  _viewControllerDependencies.popupMenuCoordinator = self.popupMenuCoordinator;
  _viewControllerDependencies.ntpCoordinator = _NTPCoordinator;
  _viewControllerDependencies.toolbarCoordinator = _toolbarCoordinator;
  _viewControllerDependencies.tabStripCoordinator = _tabStripCoordinator;
  _viewControllerDependencies.sideSwipeCoordinator = _sideSwipeCoordinator;
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
      TabUsageRecorderBrowserAgent::FromBrowser(browser);
  _viewControllerDependencies.snapshotBrowserAgent =
      SnapshotBrowserAgent::FromBrowser(browser);
  _viewControllerDependencies.layoutGuideCenter = _layoutGuideCenter;
  _viewControllerDependencies.webStateList =
      browser->GetWebStateList()->AsWeakPtr();
  _viewControllerDependencies.voiceSearchController = _voiceSearchController;
  _viewControllerDependencies.safeAreaProvider = _safeAreaProvider;
}

- (void)updateViewControllerDependencies {
  BrowserViewController* viewController = self.viewController;
  _bookmarksCoordinator.baseViewController = viewController;

  _toolbarAccessoryPresenter.baseViewController = viewController;

  self.qrScannerCoordinator.baseViewController = viewController;
  [self.qrScannerCoordinator start];

  self.popupMenuCoordinator.baseViewController = viewController;

  // The Lens coordinator needs to be started before the primary toolbar
  // coordinator so that the LensCommands dispatcher is correctly registered in
  // time.
  if (IsLVFUnifiedExperienceEnabled(self.profile->GetPrefs())) {
    _lensViewFinderCoordinator.baseViewController = viewController;
    [_lensViewFinderCoordinator start];
  } else {
    _lensCoordinator.baseViewController = viewController;
    _lensCoordinator.delegate = viewController;
    [_lensCoordinator start];
  }

  _toolbarCoordinator.baseViewController = viewController;
  _toolbarCoordinator.omniboxFocusDelegate = viewController;
  _toolbarCoordinator.popupPresenterDelegate = viewController;
  _toolbarCoordinator.toolbarHeightDelegate = viewController;
  [_toolbarCoordinator start];

  _loadQueryCommandsHandler =
      HandlerForProtocol(_dispatcher, LoadQueryCommands);
  _viewController.loadQueryCommandsHandler = _loadQueryCommandsHandler;
  _voiceSearchController.dispatcher = _loadQueryCommandsHandler;
  _omniboxCommandsHandler = HandlerForProtocol(_dispatcher, OmniboxCommands);
  _keyCommandsProvider.omniboxHandler = _omniboxCommandsHandler;
  _viewController.omniboxCommandsHandler = _omniboxCommandsHandler;

  _tabStripCoordinator.baseViewController = viewController;
  _NTPCoordinator.baseViewController = viewController;
  _bubblePresenterCoordinator.baseViewController = viewController;

  [_dispatcher startDispatchingToTarget:viewController
                            forProtocol:@protocol(BrowserCommands)];
}

// Destroys the browser view controller dependencies.
- (void)destroyViewControllerDependencies {
  _viewControllerDependencies = BrowserViewControllerDependencies{};

  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;

  [_bookmarksCoordinator stop];
  _bookmarksCoordinator = nil;

  [_bubblePresenterCoordinator stop];
  _bubblePresenterCoordinator = nil;

  _tabStripCoordinator = nil;

  [_sideSwipeCoordinator stop];
  _sideSwipeCoordinator = nil;

  _toolbarCoordinator = nil;
  _loadQueryCommandsHandler = nil;
  _omniboxCommandsHandler = nil;

  [_toolbarAccessoryPresenter disconnect];
  _toolbarAccessoryPresenter = nil;

  [_contextualPanelEntrypointHelpPresenter dismissAnimated:NO];
  _contextualPanelEntrypointHelpPresenter = nil;

  _fullscreenController = nullptr;

  [self.popupMenuCoordinator stop];
  self.popupMenuCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [_lensOverlayCoordinator stop];
  _lensOverlayCoordinator = nil;

  if (IsLVFUnifiedExperienceEnabled(self.profile->GetPrefs())) {
    [_lensViewFinderCoordinator stop];
    _lensViewFinderCoordinator = nil;
  } else {
    [_lensCoordinator stop];
    _lensCoordinator = nil;
  }

  // This can be removed if the browser agent guarenteed to be detroyed before
  // profile keyed objects.
  if (AutocompleteBrowserAgent* autocompleteBrowserAgent =
          AutocompleteBrowserAgent::FromBrowser(self.browser)) {
    autocompleteBrowserAgent->RemoveServices();
  }

  [self.downloadManagerCoordinator stop];
  self.downloadManagerCoordinator = nil;

  if (IsDownloadListEnabled()) {
    [self.downloadListCoordinator stop];
    self.downloadListCoordinator = nil;
  }

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
      [[PrintCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser];
  // Updates the printControllar value inside tabLifecycleMediator.
  self.tabLifecycleMediator.printCoordinator = self.printCoordinator;

  // Help should only show in regular, non-incognito.
  if (!self.isOffTheRecord) {
    [self.popupMenuCoordinator startPopupMenuHelpCoordinator];
  }

  /* choiceCoordinator is created and started by a BrowserCommand */

  /* NetExportCoordinator is created and started by a delegate method */

  /* passwordBreachCoordinator is created and started by a BrowserCommand */

  /* passwordProtectionCoordinator is created and started by a BrowserCommand */

  /* passwordSettingsCoordinator is created and started by a delegate method */

  /* credentialSuggestionBottomSheetCoordinator is created and started by a
   * BrowserCommand */

  /* passwordSuggestionCoordinator is created and started by a BrowserCommand */

  /* paymentsSuggestionBottomSheetCoordinator is created and started by a
   * BrowserCommand */

  /* saveCardBottomSheetCoordinator is created and started by a
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

  /* `ReminderNotificationsCoordinator` is created and started by a
   * `ReminderNotificationsCommands` */

  /* RepostFormCoordinator is created and started by a delegate method */

  /* WhatsNewCoordinator is created and started by a BrowserCommand */

  /* NonModalSignInPromoCoordinator is created and started by a BrowserCommand
   */

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

  if (IsLensOverlayAvailable(self.profile->GetPrefs())) {
    _lensOverlayCoordinator = [[LensOverlayCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser];
    _lensOverlayCoordinator.presentationEnvironment = self.viewController;
    [_lensOverlayCoordinator start];
  }
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  [self.ARQuickLookCoordinator stop];
  self.ARQuickLookCoordinator = nil;

  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;

  [self.SafariDownloadCoordinator stop];
  self.SafariDownloadCoordinator = nil;

  [self.vcardCoordinator stop];
  self.vcardCoordinator = nil;

  [self hidePageInfo];

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self stopPasswordProtectionCoordinator];

  [self.credentialSuggestionBottomSheetCoordinator stop];
  self.credentialSuggestionBottomSheetCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.paymentsSuggestionBottomSheetCoordinator stop];
  self.paymentsSuggestionBottomSheetCoordinator = nil;

  [self.cardUnmaskAuthenticationCoordinator stop];
  self.cardUnmaskAuthenticationCoordinator = nil;

  [self.plusAddressBottomSheetCoordinator stop];
  self.plusAddressBottomSheetCoordinator = nil;

  [self dismissSaveCardBottomSheet];

  [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  self.virtualCardEnrollmentBottomSheetCoordinator = nil;

  [self dismissAutofillErrorDialog];

  [self dismissAutofillProgressDialog];

  [self.printCoordinator stop];
  self.printCoordinator = nil;

  [self hidePriceTrackedItems];

  [self.promosManagerCoordinator stop];
  self.promosManagerCoordinator = nil;

  [self.readingListCoordinator stop];
  self.readingListCoordinator.delegate = nil;
  self.readingListCoordinator = nil;

  [self stopRecentTabsCoordinator];

  [self stopReminderNotificationsCoordinator];

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

  [self hideTextZoomUI];

  [self stopAutofillAddCreditCardCoordinator];

  [self.infobarBannerOverlayContainerCoordinator stop];
  self.infobarBannerOverlayContainerCoordinator = nil;

  [self.infobarModalOverlayContainerCoordinator stop];
  self.infobarModalOverlayContainerCoordinator = nil;

  [self.nonModalPromoCoordinator stop];
  self.nonModalPromoCoordinator = nil;

  [self.netExportCoordinator stop];
  self.netExportCoordinator = nil;

  [self stopSendTabToSelf];

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

  [self.unitConversionCoordinator stop];
  self.unitConversionCoordinator = nil;

  [self.nonModalSignInPromoCoordinator stop];
  self.nonModalSignInPromoCoordinator = nil;

  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;

  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;

  [_enhancedCalendarCoordinator stop];
  _enhancedCalendarCoordinator = nil;

  [_lastTabClosingAlert stop];
  _lastTabClosingAlert = nil;

  [_BWGCoordinator stop];
  _BWGCoordinator = nil;

  [_dataControlsDialogCoordinator stop];
  _dataControlsDialogCoordinator = nil;

  if (IsSyncedSetUpEnabled()) {
    [self stopSyncedSetUpCoordinator];
  }

  [self hideDriveFilePicker];
  if (@available(iOS 18.4, *)) {
    if (base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
      [self hideFileUploadPanel];
    }
  }
  [self hideContextualSheet];
  [self dismissEditAddressBottomSheet];
  [self dismissLensPromo];
  [self dismissEnhancedSafeBrowsingPromo];
  [self dismissAutoDeletionActionSheet];
  [self hideGoogleOne];
  [self stopTrustedVaultReauthentication];
  [self dismissSearchWhatYouSeePromo];
  [self dismissNotificationsOptIn];
  [self hideWelcomeBackPromo];
  [self hideComposeboxImmediately:YES];
}

// Starts independent mediators owned by this coordinator.
- (void)startIndependentMediators {
  // Cache frequently repeated property values to curb generated code bloat.
  BrowserViewController* browserViewController = self.viewController;

  DCHECK(self.browserContainerCoordinator.viewController);
  self.tabEventsMediator = [[TabEventsMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            ntpCoordinator:_NTPCoordinator
                   tracker:feature_engagement::TrackerFactory::GetForProfile(
                               self.profile)
                 incognito:self.isOffTheRecord
           loadingNotifier:_urlLoadingNotifierBrowserAgent];
  self.tabEventsMediator.toolbarSnapshotProvider = _toolbarCoordinator;
  self.tabEventsMediator.consumer = browserViewController;

  CHECK(self.tabLifecycleMediator);
  self.tabLifecycleMediator.NTPTabHelperDelegate = self.tabEventsMediator;

  browserViewController.reauthHandler =
      HandlerForProtocol(self.dispatcher, IncognitoReauthCommands);

  browserViewController.nonModalPromoPresentationDelegate = self;

  if (self.isOffTheRecord) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:self.sceneState];

    self.incognitoAuthMediator =
        [[IncognitoReauthMediator alloc] initWithReauthAgent:reauthAgent];
    self.incognitoAuthMediator.consumer = browserViewController;
  }
}

- (void)startTabLifeCycleMediator {
  Browser* browser = self.browser;

  TabLifecycleMediator* tabLifecycleMediator =
      [[TabLifecycleMediator alloc] initWithBrowser:browser];

  // Set properties that are already valid.
  tabLifecycleMediator.commandDispatcher = browser->GetCommandDispatcher();
  tabLifecycleMediator.tabHelperDelegate = self;
  tabLifecycleMediator.repostFormDelegate = self;
  tabLifecycleMediator.tabInsertionBrowserAgent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  tabLifecycleMediator.snapshotGeneratorDelegate = self;
  tabLifecycleMediator.overscrollActionsDelegate = self;
  tabLifecycleMediator.appLauncherBrowserPresentationProvider = self;
  tabLifecycleMediator.editMenuBuilder = self;

  self.tabLifecycleMediator = tabLifecycleMediator;
}

- (web::WebState*)activeWebState {
  WebStateList* webStateList = self.browser->GetWebStateList();
  return webStateList ? webStateList->GetActiveWebState() : nullptr;
}

- (void)contextualPanelEntrypointIPHDidDismissWithConfig:
            (base::WeakPtr<ContextualPanelItemConfiguration>)config
                                         dismissalReason:
                                             (IPHDismissalReasonType)reason {
  ContextualPanelItemConfiguration* config_ptr = config.get();
  if (!config_ptr) {
    return;
  }

  [HandlerForProtocol(self.dispatcher, ContextualPanelEntrypointCommands)
      notifyContextualPanelEntrypointIPHDismissed];

  ProfileIOS* profile = self.profile;
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  if (!engagementTracker || !_contextualPanelEntrypointHelpPresenter) {
    return;
  }

  engagementTracker->Dismissed(*config_ptr->iph_feature);
  _contextualPanelEntrypointHelpPresenter = nil;

  if (reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH) {
    [self openContextualSheet];
    [self recordContextualPanelEntrypointIPHDismissed:
              ContextualPanelIPHDismissedReason::UserInteracted];
    return;
  }

  if (reason == IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView ||
      reason == IPHDismissalReasonType::kTappedClose) {
    engagementTracker->NotifyEvent(
        config_ptr->iph_entrypoint_explicitly_dismissed);
    [self recordContextualPanelEntrypointIPHDismissed:
              ContextualPanelIPHDismissedReason::UserDismissed];
    return;
  }

  if (reason == IPHDismissalReasonType::kTimedOut) {
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

// Cancels all the currently active collaboration flows.
- (void)cancelCollaborationFlows {
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(self.profile);
  if (collaborationService) {
    collaborationService->CancelAllFlows();
  }
}

// Resets the composebox state provider and coordinator. The
// composeboxCoordinator should have been stopped before calling this method.
- (void)resetComposebox {
  _browserOmniboxStateProvider.composeboxStateProvider = nil;
  _composeboxCoordinator = nil;
}

#pragma mark - ActivityServiceCommands

- (void)stopAndStartSharingCoordinator {
  SharingScenario scenario = IsReaderModeActiveInWebState(self.activeWebState)
                                 ? SharingScenario::ShareInReaderMode
                                 : SharingScenario::TabShareButton;
  SharingParams* params = [[SharingParams alloc] initWithScenario:scenario];

  // Exit fullscreen if needed to make sure that share button is visible.
  _fullscreenController->ExitFullscreen(FullscreenExitReason::kForcedByCode);

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

- (void)showShareSheet {
  if (!self.sharingCoordinator) {
    [self stopAndStartSharingCoordinator];
  } else {
    [self.sharingCoordinator cancelIfNecessaryAndCreateNewCoordinator];
  }
}

- (void)showShareSheetForChromeApp {
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
  _fullscreenController->ExitFullscreen(FullscreenExitReason::kForcedByCode);

  UIView* originView =
      [_layoutGuideCenter referencedViewUnderName:kToolsMenuGuide];
  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                      params:params
                                                  originView:originView];
  [self.sharingCoordinator start];
}

- (void)showShareSheetForHighlight:(ShareHighlightCommand*)command {
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

- (void)showShareSheetForURL:(ActivityServiceShareURLCommand*)command {
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

#pragma mark - AutoDeletionCommands

- (void)presentAutoDeletionActionSheetWithDownloadTask:
    (web::DownloadTask*)task {
  // Do not present the action sheet if it is already being presented or the
  // DownloadManagerCoordinator is null.
  if (_autoDeletionCoordinator || !self.downloadManagerCoordinator) {
    return;
  }

  _autoDeletionCoordinator = [[AutoDeletionCoordinator alloc]
      initWithBaseViewController:self.downloadManagerCoordinator.viewController
                         browser:self.browser
                    downloadTask:task];
  [_autoDeletionCoordinator start];
}

- (void)dismissAutoDeletionActionSheet {
  [_autoDeletionCoordinator stop];
  _autoDeletionCoordinator = nil;
}

#pragma mark - AutofillCommands

- (void)showCredentialBottomSheet:(const autofill::FormActivityParams&)params {
  // Do not present the bottom sheet if it is already being presented.
  if (self.credentialSuggestionBottomSheetCoordinator) {
    return;
  }
  // Do not present the bottom sheet when the omnibox is being used to not
  // disrupt the user.
  if (OmniboxPositionBrowserAgent::FromBrowser(self.browser)
          ->IsOmniboxFocused()) {
    return;
  }
  self.credentialSuggestionBottomSheetCoordinator =
      [[CredentialSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              params:params
                            delegate:self];
  self.credentialSuggestionBottomSheetCoordinator.settingsHandler =
      HandlerForProtocol(self.dispatcher, SettingsCommands);
  self.credentialSuggestionBottomSheetCoordinator
      .browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands);
  [self.credentialSuggestionBottomSheetCoordinator start];
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

- (void)showSaveCardBottomSheetOnOriginWebState:(web::WebState*)originWebState {
  if (self.saveCardBottomSheetCoordinator) {
    [self.saveCardBottomSheetCoordinator stop];
  }

  if (self.activeWebState != originWebState) {
    // Do not show the sheet if the current tab is not the one where the
    // bottomsheet show request was triggered from.
    return;
  }

  self.saveCardBottomSheetCoordinator = [[SaveCardBottomSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.saveCardBottomSheetCoordinator start];
}

- (void)dismissSaveCardBottomSheet {
  [self.saveCardBottomSheetCoordinator stop];
  self.saveCardBottomSheetCoordinator = nil;
}

- (void)showVirtualCardEnrollmentBottomSheet:
            (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                              originWebState:(web::WebState*)originWebState {
  if (self.virtualCardEnrollmentBottomSheetCoordinator) {
    [self.virtualCardEnrollmentBottomSheetCoordinator stop];
  }

  if (self.activeWebState != originWebState) {
    // Do not show the sheet if the current tab is not the one where the credit
    // card was originally saved.
    return;
  }

  self.virtualCardEnrollmentBottomSheetCoordinator =
      [[VirtualCardEnrollmentBottomSheetCoordinator alloc]
             initWithUIModel:std::move(model)
          baseViewController:self.viewController
                     browser:self.browser];
  [self.virtualCardEnrollmentBottomSheetCoordinator start];
}

- (void)showEditAddressBottomSheet {
  self.editProfileBottomSheetHandler =
      [[InfobarAutofillEditProfileBottomSheetHandler alloc]
          initWithWebState:self.activeWebState];

  self.autofillEditProfileCoordinator = [[AutofillEditProfileCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                         handler:self.editProfileBottomSheetHandler];
  [self.autofillEditProfileCoordinator start];
}

- (void)dismissEditAddressBottomSheet {
  if (self.autofillEditProfileCoordinator) {
    [self.autofillEditProfileCoordinator stop];
  }

  self.autofillEditProfileCoordinator = nil;
  self.editProfileBottomSheetHandler = nil;
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
  [self.printCoordinator printWebState:self.activeWebStateOrReaderMode
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
  if (IsDownloadListEnabled()) {
    [self showDownloadList];
    return;
  }
  NSURL* URL = GetFilesAppUrl();
  if (!URL) {
    return;
  }

  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];

  base::UmaHistogramEnumeration(
      "Download.OpenDownloads.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.profile));
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
  [self stopRecentTabsCoordinator];

  self.recentTabsCoordinator = [[RecentTabsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.recentTabsCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  self.recentTabsCoordinator.delegate = self;
  [self.recentTabsCoordinator start];
}

- (void)showTranslate {
  ProfileIOS* profile = self.profile;

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  engagementTracker->NotifyEvent(
      feature_engagement::events::kTriggeredTranslateInfobar);
  web::WebState* activeWebState = self.activeWebStateOrReaderMode;
  DCHECK(activeWebState);

  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(activeWebState);
  if (translateClient) {
    translate::TranslateManager* translateManager =
        translateClient->GetTranslateManager();
    DCHECK(translateManager);
    // When Reading Mode is active shows the translate infobar, and otherwise
    // shows the translate infobar and auto translates the page.
    translateManager->ShowTranslateUI(
        /*auto_translate=*/!IsReaderModeActiveInWebState(self.activeWebState),
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
  params.in_incognito = self.isOffTheRecord;
  _urlLoadingBrowserAgent->Load(params);
}

// Requests that the activity overlay is shown. The actual creation of the
// overlay might be deferred until the UI is ready.
- (base::ScopedClosureRunner)showActivityOverlay {
  ++_numberOfActivityOverly;
  if (_numberOfActivityOverly == 1) {
    // Start the overlay immediately if the UI is ready. Otherwise,
    // `startActivityOverlay` will be invoked from `[BrowserCoordinator start]`.
    if (self.viewController) {
      [self startActivityOverlay];
    }
  }
  return base::ScopedClosureRunner(base::BindOnce(
      [](BrowserCoordinator* strongSelf) {
        [strongSelf decreaseActivityOverlay];
      },
      self));
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
  _sendTabToSelfCoordinator.delegate = self;
  [_sendTabToSelfCoordinator start];
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

  TabGroupService* groupService =
      TabGroupServiceFactory::GetForProfile(self.profile);
  const TabGroup* group = webStateList->GetGroupOfWebStateAt(active_index);
  if (groupService && groupService->ShouldDisplayLastTabCloseAlert(group)) {
    web::WebState* webState = webStateList->GetWebStateAt(active_index);
    BOOL isTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
    SharedTabGroupLastTabAlertCommand* command =
        [[SharedTabGroupLastTabAlertCommand alloc]
                 initWithTabID:webState->GetUniqueIdentifier()
                       browser:self.browser
                         group:group
            baseViewController:self.viewController
                    sourceView:isTablet ? nil : self.viewController.view
                       closing:YES];

    id<SharedTabGroupLastTabAlertCommands> lastTabAlertHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(),
                           SharedTabGroupLastTabAlertCommands);
    [lastTabAlertHandler showLastTabInSharedGroupAlert:command];
    return;
  }

  BOOL canShowTabStrip = CanShowTabStrip(self.viewController);

  UIView* contentArea = self.browserContainerCoordinator.viewController.view;
  UIView* snapshotView = nil;

  if (!canShowTabStrip) {
    snapshotView = [contentArea snapshotViewAfterScreenUpdates:NO];
    snapshotView.frame = contentArea.frame;
  }

  webStateList->CloseWebStateAt(active_index,
                                WebStateList::ClosingReason::kUserAction);

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
  [self.credentialSuggestionBottomSheetCoordinator stop];
  self.credentialSuggestionBottomSheetCoordinator = nil;
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

- (void)showSearchWhatYouSeePromo {
  [_searchWhatYouSeePromoCoordinator stop];
  _searchWhatYouSeePromoCoordinator = [[SearchWhatYouSeePromoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [_searchWhatYouSeePromoCoordinator start];
}

- (void)dismissSearchWhatYouSeePromo {
  [_searchWhatYouSeePromoCoordinator stop];
  _searchWhatYouSeePromoCoordinator = nil;
}

- (void)showNotificationsOptInFromAccessPoint:
            (NotificationOptInAccessPoint)accessPoint
                           baseViewController:
                               (UIViewController*)baseViewController {
  [_notificationsOptInCoordinator stop];

  _notificationsOptInCoordinator = [[NotificationsOptInCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser];
  _notificationsOptInCoordinator.accessPoint = accessPoint;
  _notificationsOptInCoordinator.delegate = self;

  [_notificationsOptInCoordinator start];
}

- (void)dismissNotificationsOptIn {
  [_notificationsOptInCoordinator stop];
  _notificationsOptInCoordinator = nil;
}

- (void)forceFullscreenMode {
  _fullscreenController->EnterForceFullscreenMode(YES);
}

- (void)showAddAccountWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                       prefilledEmail:(NSString*)email {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:signin::GetRegularBrowser(
                                                      self.browser)
                                     contextStyle:contextStyle
                                      accessPoint:accessPoint
                                   prefilledEmail:email
                             continuationProvider:
                                 DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_signinCoordinator start];
}

- (void)performReauthToRetrieveTrustedVaultKey:
    (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger {
  [self showTrustedVaultReauthForFetchKeysWithTrigger:trigger];
}

- (void)showComposeboxFromEntrypoint:(ComposeboxEntrypoint)entrypoint
                           withQuery:(NSString*)query {
  CHECK(base::FeatureList::IsEnabled(kComposeboxIOS));
  if (_composeboxCoordinator) {
    return;
  }
  _composeboxCoordinator = [[ComposeboxCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                      entrypoint:entrypoint
                           query:query
         composeboxAnimationBase:_toolbarCoordinator];
  [_composeboxCoordinator start];
  _browserOmniboxStateProvider.composeboxStateProvider = _composeboxCoordinator;
}

- (void)hideComposeboxImmediately:(BOOL)immediately {
  if (!_composeboxCoordinator) {
    return;
  }

  if (immediately) {
    [_composeboxCoordinator stop];
    [self resetComposebox];
  } else {
    __weak __typeof(self) weakSelf = self;
    base::OnceClosure completion = base::BindOnce(^{
      [weakSelf.composeboxCoordinator stopAnimatedWithCompletion:^{
        [weakSelf resetComposebox];
      }];
    });
    // Stop the prototoype on the next run loop as this might be called while
    // the prototype's omnibox is loading a query. TODO(crbug.com/454302076):
    // Remove this workaround once the omnibox can be safely dismissed while
    // openMatch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(completion));
  }
}

#pragma mark - ContextualPanelEntrypointIPHCommands

- (BOOL)showContextualPanelEntrypointIPHWithConfig:
            (ContextualPanelItemConfiguration*)config
                                       anchorPoint:(CGPoint)anchorPoint
                                   isBottomOmnibox:(BOOL)isBottomOmnibox {
  ContextualPanelItemConfiguration& config_ref = CHECK_DEREF(config);

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);

  if (!engagementTracker) {
    return NO;
  }

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<ContextualPanelItemConfiguration> config_weak_ptr =
      config_ref.weak_ptr_factory.GetWeakPtr();
  CallbackWithIPHDismissalReasonType dismissalCallback = ^(
      IPHDismissalReasonType reason) {
    [weakSelf contextualPanelEntrypointIPHDidDismissWithConfig:config_weak_ptr
                                               dismissalReason:reason];
  };

  _contextualPanelEntrypointHelpPresenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:ShouldShowRichContextualPanelEntrypointIPH()
                                ? base::SysUTF8ToNSString(config_ref.iph_text)
                                : base::SysUTF8ToNSString(config_ref.iph_title)
                      title:base::SysUTF8ToNSString(config_ref.iph_title)
             arrowDirection:isBottomOmnibox ? BubbleArrowDirectionDown
                                            : BubbleArrowDirectionUp
                  alignment:BubbleAlignmentTopOrLeading
                 bubbleType:ShouldShowRichContextualPanelEntrypointIPH()
                                ? BubbleViewTypeRich
                                : BubbleViewTypeDefault
            pageControlPage:BubblePageControlPageNone
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

- (void)dismissContextualPanelEntrypointIPH:(BOOL)animated {
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
      ChooseFileTabHelper::FromWebState(activeWebState);
  if (!tab_helper || !tab_helper->IsChoosingFiles()) {
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

#pragma mark - EnhancedCalendarCommands

- (void)showEnhancedCalendarWithConfig:
    (EnhancedCalendarConfiguration*)enhancedCalendarConfig {
  _enhancedCalendarCoordinator = [[EnhancedCalendarCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
          enhancedCalendarConfig:enhancedCalendarConfig];
  [_enhancedCalendarCoordinator start];
}

- (void)hideEnhancedCalendarBottomSheet {
  [_enhancedCalendarCoordinator stop];
  _enhancedCalendarCoordinator = nil;
}
#pragma mark - ReaderModeCommands

- (void)showReaderModeFromAccessPoint:(ReaderModeAccessPoint)accessPoint {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }
  BOOL lensOverlayAvailable = IsLensOverlayAvailable(self.profile->GetPrefs());
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(activeWebState);
  auto activateReader =
      base::BindOnce(&ReaderModeTabHelper::ActivateReader,
                     readerModeTabHelper->GetWeakPtr(), accessPoint);

  if (lensOverlayAvailable) {
    LensOverlayTabHelper* lensOverlayTabHelper =
        LensOverlayTabHelper::FromWebState(activeWebState);
    BOOL lensOverlayVisible =
        lensOverlayTabHelper &&
        lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
    if (lensOverlayVisible) {
      id<LensOverlayCommands> lensOverlayHandler =
          HandlerForProtocol(_dispatcher, LensOverlayCommands);
      [lensOverlayHandler
          destroyLensUI:YES
                 reason:lens::LensOverlayDismissalSource::kReaderModeActivated
             completion:base::CallbackToBlock(std::move(activateReader))];
      return;
    }
  }
  std::move(activateReader).Run();
}

- (void)hideReaderMode {
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(activeWebState);
  auto deactivateReader = base::BindOnce(
      &ReaderModeTabHelper::DeactivateReader, readerModeTabHelper->GetWeakPtr(),
      ReaderModeDeactivationReason::kUserDeactivated);

  BOOL lensOverlayAvailable = IsLensOverlayAvailable(self.profile->GetPrefs());

  if (lensOverlayAvailable) {
    LensOverlayTabHelper* lensOverlayTabHelper =
        LensOverlayTabHelper::FromWebState(activeWebState);
    BOOL lensOverlayVisible =
        lensOverlayTabHelper &&
        lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
    if (lensOverlayVisible) {
      id<LensOverlayCommands> lensOverlayHandler =
          HandlerForProtocol(_dispatcher, LensOverlayCommands);
      // TODO(crbug.com/436453178): Rename lens dismissal reason to be
      // `kReaderModeInvoked`.
      [lensOverlayHandler
          destroyLensUI:YES
                 reason:lens::LensOverlayDismissalSource::kReaderModeActivated
             completion:base::CallbackToBlock(std::move(deactivateReader))];
      return;
    }
  }
  std::move(deactivateReader).Run();
}

- (void)showReaderModeBlurOverlay:(ProceduralBlock)completion {
  if (_readerModeBlurOverlayCoordinator) {
    if (completion) {
      completion();
    }
    return;
  }
  _readerModeBlurOverlayCoordinator = [[ReaderModeBlurOverlayCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:self.browser];
  [_readerModeBlurOverlayCoordinator startWithCompletion:completion];
}

- (void)hideReaderModeBlurOverlay {
  [_readerModeBlurOverlayCoordinator stop];
  _readerModeBlurOverlayCoordinator = nil;
}

#pragma mark - FileUploadPanelCommands

- (void)showFileUploadPanel API_AVAILABLE(ios(18.4)) {
  CHECK(base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu));
  ChooseFileTabHelper* tabHelper =
      ChooseFileTabHelper::FromWebState(self.activeWebState);
  if (!tabHelper || !tabHelper->IsChoosingFiles()) {
    return;
  }
  if (_fileUploadPanelCoordinator) {
    return;
  }
  _fileUploadPanelCoordinator = [[FileUploadPanelCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [_fileUploadPanelCoordinator start];
}

- (void)hideFileUploadPanel API_AVAILABLE(ios(18.4)) {
  CHECK(base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu));
  [_fileUploadPanelCoordinator stop];
  _fileUploadPanelCoordinator = nil;
}

#pragma mark - FindInPageCommands

- (void)openFindInPage {
  __weak __typeof(self) weakSelf = self;

  auto startFindInPage = ^{
    [weakSelf showSystemFindPanel];
  };

  BOOL lensOverlayAvailable = IsLensOverlayAvailable(self.profile->GetPrefs());
  web::WebState* activeWebState = self.activeWebState;
  if (lensOverlayAvailable && activeWebState) {
    LensOverlayTabHelper* lensOverlayTabHelper =
        LensOverlayTabHelper::FromWebState(activeWebState);
    BOOL lensOverlayVisible =
        lensOverlayTabHelper &&
        lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
    if (lensOverlayVisible) {
      id<LensOverlayCommands> lensOverlayHandler =
          HandlerForProtocol(_dispatcher, LensOverlayCommands);
      [lensOverlayHandler
          destroyLensUI:YES
                 reason:lens::LensOverlayDismissalSource::kFindInPageInvoked
             completion:startFindInPage];
      return;
    }
  }

  startFindInPage();
}

- (void)closeFindInPage {
  web::WebState* activeWebState = [self activeWebStateOrReaderMode];
  if (!activeWebState) {
    return;
  }

  FindTabHelper* helper = FindTabHelper::FromWebState(activeWebState);
  DCHECK(helper);
  if (helper->IsFindUIActive()) {
    helper->StopFinding();
  }
}

- (void)showFindUIIfActive {
  FindTabHelper* findHelper =
      FindTabHelper::FromWebState([self activeWebStateOrReaderMode]);
  if (!findHelper || !findHelper->IsFindUIActive()) {
    return;
  }

  [self showSystemFindPanel];
}

- (void)hideFindUI {
  web::WebState* activeWebState = [self activeWebStateOrReaderMode];
  if (!activeWebState) {
    return;
  }
  auto* helper = FindTabHelper::FromWebState(activeWebState);
  helper->DismissFindNavigator();
}

- (void)findNextStringInPage {
  web::WebState* activeWebState = self.activeWebState;
  DCHECK(activeWebState);
  // TODO(crbug.com/40465124): Reshow find bar if necessary.
  FindTabHelper::FromWebState(activeWebState)
      ->ContinueFinding(FindTabHelper::FORWARD);
}

- (void)findPreviousStringInPage {
  web::WebState* activeWebState = [self activeWebStateOrReaderMode];
  DCHECK(activeWebState);
  // TODO(crbug.com/40465124): Reshow find bar if necessary.
  FindTabHelper::FromWebState(activeWebState)
      ->ContinueFinding(FindTabHelper::REVERSE);
}

#pragma mark - FindInPageCommands Helpers

- (void)showSystemFindPanel {
  web::WebState* activeWebState = [self activeWebStateOrReaderMode];
  DCHECK(activeWebState);
  auto* helper = FindTabHelper::FromWebState(activeWebState);

  if (!helper->IsFindUIActive()) {
    // Hide the Omnibox to avoid user's confusion about which text field is
    // currently focused. The mode is force to avoid the bottom Omnibox
    // appearing above the find in page collapsed toolbar when scrolling.
    _fullscreenController->EnterForceFullscreenMode(
        /* insets_update_enabled */ true);
    helper->SetFindUIActive(true);
  }

  // If the Native Find in Page variant does not use the Chrome Find bar, it
  // is sufficient to call `StartFinding()` directly on the Find tab helper of
  // the current web state.
  helper->StartFinding(@"");
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

#pragma mark - BWGCommands

- (void)startBWGFlowWithEntryPoint:(bwg::EntryPoint)entryPoint {
  [self startBWGFlowWithImageAttachment:nil entryPoint:entryPoint];
}

- (void)startBWGFlowWithImageAttachment:(UIImage*)image
                             entryPoint:(bwg::EntryPoint)entryPoint {
  _BWGCoordinator =
      [[BWGCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser
                                          fromEntryPoint:entryPoint];
  _BWGCoordinator.imageAttachment = image;
  [_BWGCoordinator start];
}

- (void)dismissBWGFlowWithCompletion:(ProceduralBlock)completion {
  if (!_BWGCoordinator && completion) {
    completion();
    return;
  }

  [_BWGCoordinator stopWithCompletion:completion];
  _BWGCoordinator = nil;
}

- (void)showBWGPromoIfPageIsEligible {
  BwgService* BWGService = BwgServiceFactory::GetForProfile(self.profile);
  if (BWGService->IsBwgAvailableForWebState(self.activeWebState)) {
    [self startBWGFlowWithEntryPoint:bwg::EntryPoint::Promo];
  }
}

#pragma mark - PromosManagerCommands

- (void)showPromo {
  if (!self.promosManagerCoordinator) {
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    id<CredentialProviderPromoCommands> credentialProviderPromoHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(),
                           CredentialProviderPromoCommands);
    id<DockingPromoCommands> dockingPromoHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), DockingPromoCommands);

    self.promosManagerCoordinator = [[PromosManagerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
                    applicationHandler:applicationHandler
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

- (void)showAppStoreReviewPrompt {
  if (IsAppStoreRatingEnabled()) {
    [SKStoreReviewController requestReviewInScene:self.sceneState.scene];

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

- (void)showDefaultBrowserPromo {
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

  if (IsDefaultBrowserOffCyclePromoEnabled()) {
    self.defaultBrowserGenericPromoCoordinator.promoWasFromOffCycleTrigger =
        YES;
  }

  [self.defaultBrowserGenericPromoCoordinator start];
}

- (void)showDefaultBrowserPromoAfterRemindMeLater {
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

- (void)showFullscreenSigninPromo {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showFullscreenSigninPromoWithCompletion:^(SigninCoordinator* coordinator,
                                                SigninCoordinatorResult result,
                                                id<SystemIdentity>) {
        [self.promosManagerCoordinator promoWasDismissed];
      }];
}

- (void)showWelcomeBackPromo {
  _welcomeBackCoordinator = [[WelcomeBackCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  [_welcomeBackCoordinator start];
}

#pragma mark - PageActionMenuCommands

- (void)showPageActionMenu {
  _pageActionMenuCoordinator = [[PageActionMenuCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _pageActionMenuCoordinator.pageActionMenuHandler =
      HandlerForProtocol(self.dispatcher, PageActionMenuCommands);
  [_pageActionMenuCoordinator start];
}

- (void)dismissPageActionMenuWithCompletion:(ProceduralBlock)completion {
  [_pageActionMenuCoordinator stopWithCompletion:completion];
  _pageActionMenuCoordinator = nil;
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
      showSavedPasswordsSettingsFromViewController:self.viewController];
}

- (void)openPasswordSettings {
  // TODO(crbug.com/40067451): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordSettingsCoordinator);

  // Use main browser to open the password settings.
  SceneState* sceneState = self.sceneState;
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

#pragma mark - TextZoomCommands

- (void)openTextZoom {
  [self.textZoomCoordinator stop];
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
  [self hideTextZoomUI];
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
  self.textZoomCoordinator = nil;
}

- (TextZoomCoordinator*)newTextZoomCoordinator {
  TextZoomCoordinator* textZoomCoordinator = [[TextZoomCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  textZoomCoordinator.presenter = _toolbarAccessoryPresenter;

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

- (web::WebState*)activeWebStateOrReaderMode {
  if (!IsReaderModeAvailable()) {
    return self.activeWebState;
  }

  if (self.activeWebState) {
    ReaderModeTabHelper* tabHelper =
        ReaderModeTabHelper::FromWebState(self.activeWebState);
    if (tabHelper) {
      return tabHelper->GetReaderModeWebState() ?: self.activeWebState;
    }
  }

  return self.activeWebState;
}

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

  ReaderModeBrowserAgent* readerModeBrowserAgent =
      ReaderModeBrowserAgent::FromBrowser(self.browser);
  if (readerModeBrowserAgent) {
    readerModeBrowserAgent->SetDelegate(self);
  }
}

// Installs delegates for self.profile
- (void)installDelegatesForBrowserState {
  ProfileIOS* profile = self.profile;
  if (profile) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForProfile(profile)
        ->SetWebStateList(self.browser->GetWebStateList());
  }
}

// Uninstalls delegates for self.profile
- (void)uninstallDelegatesForBrowserState {
  ProfileIOS* profile = self.profile;
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

  ReaderModeBrowserAgent* readerModeBrowserAgent =
      ReaderModeBrowserAgent::FromBrowser(self.browser);
  if (readerModeBrowserAgent) {
    readerModeBrowserAgent->SetDelegate(nil);
  }
}

#pragma mark - ParentAccessCommands

- (void)
    showParentAccessBottomSheetForWebState:(web::WebState*)webState
                                 targetURL:(const GURL&)targetURL
                   filteringBehaviorReason:
                       (supervised_user::FilteringBehaviorReason)
                           filteringBehaviorReason
                                completion:
                                    (void (^)(
                                        supervised_user::LocalApprovalResult,
                                        std::optional<
                                            supervised_user::
                                                LocalWebApprovalErrorType>))
                                        completion {
  if (!supervised_user::IsLocalWebApprovalsEnabled()) {
    return;
  }

  if (self.activeWebState != webState) {
    // Do not show the sheet if the current tab is not the one where the
    // user initiated parent local web approvals.
    return;
  }
  // Close parent access local web approval if it was already opened for another
  // URL.
  if (self.parentAccessCoordinator) {
    [self.parentAccessCoordinator stop];
  }

  self.parentAccessCoordinator = [[ParentAccessCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                       targetURL:targetURL
         filteringBehaviorReason:filteringBehaviorReason
                      completion:completion];
  [self.parentAccessCoordinator start];
}

- (void)hideParentAccessBottomSheet {
  [self.parentAccessCoordinator stop];
  self.parentAccessCoordinator = nil;
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
                         frame:(base::WeakPtr<web::WebFrame>)frame
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
                           frame:frame
                 decisionHandler:decisionHandler
                       proactive:proactive];
  self.passwordSuggestionCoordinator.delegate = self;
  [self.passwordSuggestionCoordinator start];
}

#pragma mark - PriceTrackedItemsCommands

- (void)showPriceTrackedItemsWithCurrentPage {
  [self showPriceTrackedItems:YES];
}

- (void)showPriceTrackedItems {
  [self showPriceTrackedItems:NO];
}

- (void)hidePriceTrackedItems {
  [self.priceNotificationsViewCoordinator stop];
  self.priceNotificationsViewCoordinator = nil;
}

- (void)presentPriceTrackedItemsWhileBrowsingIPH {
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
  if (self.sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
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

#pragma mark - SyncedSetUpCoordinatorDelegate

- (void)syncedSetUpCoordinatorWantsToBeDismissed:
    (SyncedSetUpCoordinator*)coordinator {
  CHECK(IsSyncedSetUpEnabled());
  CHECK_EQ(_syncedSetUpCoordinator, coordinator);
  [self stopSyncedSetUpCoordinator];
}

#pragma mark - SyncedSetUpCommands

- (void)showSyncedSetUpWithDismissalCompletion:(ProceduralBlock)completion {
  CHECK(IsSyncedSetUpEnabled());
  CHECK(CanShowSyncedSetUp(self.profile->GetPrefs()));

  _runAfterSyncedSetUpDismissal = [completion copy];

  if (_syncedSetUpCoordinator) {
    // The UI is already active; the stored `completion` will run when it stops.
    return;
  }

  _syncedSetUpCoordinator = [[SyncedSetUpCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _syncedSetUpCoordinator.delegate = self;
  [_syncedSetUpCoordinator start];
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

#pragma mark - SharedTabGroupLastTabAlertCommands

- (void)showLastTabInSharedGroupAlert:
    (SharedTabGroupLastTabAlertCommand*)command {
  UIViewController* viewController = command.baseViewController
                                         ? command.baseViewController
                                         : self.viewController;
  UIView* sourceView =
      command.sourceView ? command.sourceView : self.viewController.view;

  _lastTabClosingAlert = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:self.browser
                      actionType:command.actionType
                      sourceView:sourceView];

  __weak BrowserCoordinator* weakSelf = self;
  _lastTabClosingAlert.primaryAction = ^{
    [weakSelf runLeaveOrDeleteCompletion:command.group
                          viewController:viewController];
  };
  if (command.actionType == TabGroupActionType::kCloseLastTabUnknownRole) {
    // If the user's member role is unkown (i.e. sync not complete yet),
    // cannot show option to leave/keep group when attempting to close last
    // tab. Instead, close last tab and replace with new tab after an error
    // alert is shown.
    _lastTabClosingAlert.primaryAction = ^{
      [weakSelf runKeepGroup:command.group lastTabID:command.tabID];
    };
  }
  _lastTabClosingAlert.secondaryAction = ^{
    if (command.closing) {
      [weakSelf runKeepGroup:command.group lastTabID:command.tabID];
    }
  };

  _lastTabClosingAlert.tabGroupName = command.groupTitle;
  _lastTabClosingAlert.showAsAlert = command.displayAsAlert;
  _lastTabClosingAlert.canCancel = command.canCancel;
  [_lastTabClosingAlert start];
}

#pragma mark - SharedTabGroupLastTabAlertCommands helpers

// Runs `leaveOrDeleteCompletion`. If not nil, calls it with `kSuccess`.
- (void)runLeaveOrDeleteCompletion:(const TabGroup*)group
                    viewController:(UIViewController*)viewController {
  __weak BrowserCoordinator* weakSelf = self;
  base::OnceCallback<void(
      collaboration::CollaborationControllerDelegate::ResultCallback)>
      completionCallback = base::BindOnce(
          ^(collaboration::CollaborationControllerDelegate::ResultCallback
                resultCallback) {
            BrowserCoordinator* strongSelf = weakSelf;
            if (!strongSelf) {
              std::move(resultCallback)
                  .Run(collaboration::CollaborationControllerDelegate::Outcome::
                           kCancel);
              return;
            }
            std::move(resultCallback)
                .Run(collaboration::CollaborationControllerDelegate::Outcome::
                         kSuccess);
          });

  std::unique_ptr<collaboration::IOSCollaborationControllerDelegate> delegate =
      std::make_unique<collaboration::IOSCollaborationControllerDelegate>(
          self.browser, CreateControllerDelegateParamsFromProfile(
                            self.profile, viewController,
                            collaboration::FlowType::kLeaveOrDelete));
  delegate->SetLeaveOrDeleteConfirmationCallback(std::move(completionCallback));

  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(self.profile);
  collaboration::CollaborationServiceLeaveOrDeleteEntryPoint entryPoint =
      collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown;
  collaborationService->StartLeaveOrDeleteFlow(
      std::move(delegate), group->tab_group_id(), entryPoint);
  _lastTabClosingAlert = nil;
}

// Replaces the last tab with a New Tab Page (NTP).
- (void)runKeepGroup:(const TabGroup*)group lastTabID:(web::WebStateID)tabID {
  TabGroupService* groupService =
      TabGroupServiceFactory::GetForProfile(self.profile);
  WebStateList* webStateList = self.browser->GetWebStateList();
  std::unique_ptr<web::WebState> webState =
      groupService->WebStateToAddToEmptyGroup();
  webStateList->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::Automatic().Activate().InGroup(group));

  const WebStateSearchCriteria& searchCriteria = WebStateSearchCriteria{
      .identifier = tabID,
  };

  int index = GetWebStateIndex(webStateList, searchCriteria);
  if (index != WebStateList::kInvalidIndex) {
    webStateList->CloseWebStateAt(index,
                                  WebStateList::ClosingReason::kUserAction);
  }
  _lastTabClosingAlert = nil;
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

- (void)showDefaultBrowserNonModalPromoWithReason:
    (NonModalDefaultBrowserPromoReason)promoReason {
  self.nonModalPromoCoordinator =
      [[DefaultBrowserPromoNonModalCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                         promoReason:promoReason];
  [self.nonModalPromoCoordinator start];
  [self.nonModalPromoCoordinator presentInfobarBannerAnimated:YES
                                                   completion:nil];
}

- (void)dismissDefaultBrowserNonModalPromoAnimated:(BOOL)animated {
  [self.nonModalPromoCoordinator dismissInfobarBannerAnimated:animated
                                                   completion:nil];
}

- (void)defaultBrowserNonModalPromoWasDismissed {
  [[NonModalDefaultBrowserPromoSchedulerSceneAgent
      agentFromScene:self.sceneState] logPromoWasDismissed];
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

#pragma mark - EditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  return [self.browserContainerCoordinator.editMenuBuilder
      buildEditMenuWithBuilder:builder
                    inWebState:webState];
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
}

#pragma mark - SendTabToSelfCoordinatorDelegate

- (void)sendTabToSelfCoordinatorWantsToBeStopped:
    (SendTabToSelfCoordinator*)coordinator {
  CHECK_EQ(_sendTabToSelfCoordinator, coordinator, base::NotFatalUntil::M150);
  [self stopSendTabToSelf];
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

#pragma mark - PrerenderBrowserAgentDelegate methods

- (UIView*)webViewContainer {
  return self.browserContainerCoordinator.viewController.view;
}

#pragma mark - SyncPresenter (Public)

- (void)showPrimaryAccountReauth {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kReauthInfoBar;
  SigninContextStyle style = SigninContextStyle::kDefault;
  Browser* regularBrowser = signin::GetRegularBrowser(self.browser);
  _signinCoordinator = [SigninCoordinator
      primaryAccountReauthCoordinatorWithBaseViewController:self.viewController
                                                    browser:regularBrowser
                                               contextStyle:style
                                                accessPoint:accessPoint
                                                promoAction:promoAction
                                       continuationProvider:
                                           DoNothingContinuationProvider()];

  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_signinCoordinator start];
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
    (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger {
  [self showTrustedVaultReauthWithTrigger:trigger
                                   intent:
                                       SigninTrustedVaultDialogIntentFetchKeys];
}

- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger {
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  [self showTrustedVaultReauthWithTrigger:trigger intent:intent];
}

#pragma mark - SyncPresenter helper

- (void)showTrustedVaultReauthWithTrigger:
            (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger
                                   intent:
                                       (SigninTrustedVaultDialogIntent)intent {
  if (_trustedVaultReauthenticationCoordinator) {
    // This can occur in case of double-tap.
    return;
  }
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              intent:intent
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

#pragma mark - ReSigninPresenter

- (void)showReSignin {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kResigninInfobar;
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;

  Browser* regularBrowser = signin::GetRegularBrowser(self.browser);
  _signinCoordinator = [SigninCoordinator
      signinAndSyncReauthCoordinatorWithBaseViewController:self.viewController
                                                   browser:regularBrowser
                                              contextStyle:contextStyle
                                               accessPoint:accessPoint
                                               promoAction:promoAction
                                      continuationProvider:
                                          DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_signinCoordinator start];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  _signinCoordinator = [SigninCoordinator
      signinCoordinatorWithCommand:command
                           browser:signin::GetRegularBrowser(self.browser)
                baseViewController:self.viewController];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        SigninCoordinatorCompletionCallback completion = command.completion;
        if (completion) {
          completion(coordinator, result, identity);
        }
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_signinCoordinator start];
}

#pragma mark - SnapshotGeneratorDelegate methods
// TODO(crbug.com/40206055): Refactor SnapshotGenerator into (probably) a
// mediator with a narrowly-defined API to get UI-layer information from the
// BVC.

- (BOOL)canTakeSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  DCHECK(webStateInfo);
  web::WebState* webState = webStateInfo.webState;
  if (!webState || !webState->IsRealized()) {
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

  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(webState);
  if (readerModeTabHelper && readerModeTabHelper->GetReaderModeWebState()) {
    // If Reader mode content is used in `webState` then no additional insets
    // are necessary since the Reader mode view is already constrained to fit
    // between toolbars.
    return UIEdgeInsetsZero;
  }

  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(webState);
  bool isLensOverlayAvailable =
      IsLensOverlayAvailable(self.profile->GetPrefs()) && lensOverlayTabHelper;

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

  if (IsVisibleURLNewTabPage(webState)) {
    const BOOL canShowTabStrip = CanShowTabStrip(self.viewController);
    const BOOL isSplitToolbarMode = IsSplitToolbarMode(self.viewController);
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, the toolbars are not splitted or for the
    // incognito NTP, the NTP is already laid out between the toolbars, so it
    // should not be inset while snapshotting.
    if (canShowTabStrip || !isSplitToolbarMode || self.isOffTheRecord) {
      return UIEdgeInsetsZero;
    }

    // For the regular NTP without tab strip, it sits above the bottom toolbar
    // but, since it is displayed as full-screen at the top, it requires maximum
    // viewport insets.
    maxViewportInsets.bottom = 0;
    // In this case as well, the top toolbar is also not showing, so just factor
    // in the top safe area inset.
    maxViewportInsets.top = _safeAreaProvider.safeArea.top;
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

  Browser* browser = self.browser;
  WebStateList* webStateList = browser->GetWebStateList();

  if (webStateList->GetIndexOfWebState(webState) ==
      WebStateList::kInvalidIndex) {
    return @[];
  }

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];

  PrefService* prefs = browser->GetProfile()->GetPrefs();
  if (IsLensOverlayAvailable(prefs)) {
    LensOverlayTabHelper* lensOverlayTabHelper =
        LensOverlayTabHelper::FromWebState(webState);

    if (lensOverlayTabHelper) {
      BOOL isLensOverlayCurrentlyInvoked;

      if (IsLensOverlaySameTabNavigationEnabled(prefs)) {
        isLensOverlayCurrentlyInvoked =
            lensOverlayTabHelper->IsLensOverlayInvokedOnCurrentNavigationItem();
      } else {
        isLensOverlayCurrentlyInvoked =
            lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
      }

      // A lens overlay is invoked in the given web state.
      if (isLensOverlayCurrentlyInvoked) {
        UIView* lensOverlayView = _lensOverlayCoordinator.viewController.view;

        if (lensOverlayView) {
          [overlays addObject:lensOverlayView];
        }
      }
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
  if (IsVisibleURLNewTabPage(webState)) {
    // If NTPCoordinator is not started yet, fall back to using the
    // webState's view. `_NTPCoordinator.started` should be true in most cases
    // but it can be false when the app will be terminated or the browser data
    // is removed. In particular, it can be false when this method is called as
    // a delayed task while the app is being terminated.
    if (_NTPCoordinator.started) {
      return _NTPCoordinator.viewController.view;
    }
  }
  SnapshotSourceTabHelper* snapshotSource =
      SnapshotSourceTabHelper::FromWebState(webState);
  return snapshotSource->GetView();
}

#pragma mark - NewTabPageCommands

- (void)handleFeedModelDidEndUpdates:(FeedLayoutUpdateType)updateType {
  [_NTPCoordinator handleFeedModelDidEndUpdates:updateType];
}

- (void)presentLensIconBubble {
  __weak NewTabPageCoordinator* weakNTPCoordinator = _NTPCoordinator;
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      prepareToPresentModalWithSnackbarDismissal:YES
                                      completion:^{
                                        [weakNTPCoordinator
                                            presentLensIconBubble];
                                      }];
}

- (void)presentFeedSwipeFirstRunBubble {
  if ([_NTPCoordinator isFeedVisible] &&
      GetFeedSwipeIPHVariation() == FeedSwipeIPHVariation::kStaticAfterFRE) {
    [HandlerForProtocol(_dispatcher, HelpCommands)
        presentInProductHelpWithType:InProductHelpType::kFeedSwipe];
  }
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
  return self.viewController.visibilityState ==
         BrowserViewVisibilityState::kVisible;
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
  [_sideSwipeCoordinator animatePageSideSwipeInDirection:direction];
}

#pragma mark - OverscrollActionsControllerDelegate methods.

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(_dispatcher, ApplicationCommands);
  [applicationCommandsHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithIncognito:self.isOffTheRecord]];
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  [self closeCurrentTab];
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  // Instruct the SnapshotTabHelper to ignore the next load event.
  // Attempting to snapshot while the overscroll "bounce back" animation is
  // occurring will cut the animation short.
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState) {
    return;
  }
  ProfileIOS* profile = self.profile;
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
                        fromTabId:(web::WebStateID)tabId {
  // Ignore unless the call comes from currently visible tab.
  web::WebStateID visibleTabId = self.activeWebState->GetUniqueIdentifier();
  if (tabId != visibleTabId) {
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
      showSavedPasswordsSettingsFromViewController:self.viewController];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(_dispatcher, SettingsCommands);
  [settingsHandler showPasswordDetailsForCredential:credential inEditMode:NO];
}

#pragma mark - MiniMapCommands

- (void)presentMiniMapWithIPHForText:(NSString*)text {
  self.miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                        text:text
                                                         url:nil
                                                     withIPH:YES
                                                        mode:MiniMapMode::kMap];
  [self.miniMapCoordinator start];
}

- (void)presentMiniMapForText:(NSString*)text {
  self.miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                        text:text
                                                         url:nil
                                                     withIPH:NO
                                                        mode:MiniMapMode::kMap];
  [self.miniMapCoordinator start];
}

- (void)presentMiniMapForURL:(NSURL*)URL {
  self.miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                        text:nil
                                                         url:URL
                                                     withIPH:NO
                                                        mode:MiniMapMode::kMap];
  [self.miniMapCoordinator start];
}

- (void)presentMiniMapDirectionsForText:(NSString*)text {
  self.miniMapCoordinator = [[MiniMapCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            text:text
                             url:nil
                         withIPH:NO
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

#pragma mark - ReminderNotificationsCommands

- (void)showSetTabReminderUI:(SetTabReminderEntryPoint)entryPoint {
  CHECK(
      send_tab_to_self::IsSendTabIOSPushNotificationsEnabledWithTabReminders());

  _reminderNotificationsCoordinator = [[ReminderNotificationsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  [_reminderNotificationsCoordinator start];
}

- (void)dismissSetTabReminderUI {
  CHECK(
      send_tab_to_self::IsSendTabIOSPushNotificationsEnabledWithTabReminders());

  [self stopReminderNotificationsCoordinator];
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

- (void)showQuickDeleteAndCanPerformRadialWipeAnimation:
    (BOOL)canPerformRadialWipeAnimation {
  CHECK(!self.isOffTheRecord);

  [_quickDeleteCoordinator stop];

  _quickDeleteCoordinator = [[QuickDeleteCoordinator alloc]
         initWithBaseViewController:
             top_view_controller::TopPresentedViewControllerFrom(
                 self.sceneState.window.rootViewController)
                            browser:self.browser
      canPerformRadialWipeAnimation:canPerformRadialWipeAnimation];
  [_quickDeleteCoordinator start];
}

- (void)stopQuickDelete {
  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;
}

- (void)stopQuickDeleteForAnimationWithCompletion:(ProceduralBlock)completion {
  // If BrowserViewController has not presented any view controller (i.e. QD has
  // been dismissed) and the tab grid is also not visible, then just trigger
  // `completion` immediately.
  if (!self.viewController.presentedViewController &&
      !self.sceneState.controller.isTabGridVisible) {
    if (completion) {
      completion();
    }
    [self stopQuickDelete];
    return;
  }

  // If BrowserViewController has presented a view controller, then dismiss
  // every VC on top of it.
  __weak __typeof(self) weakSelf = self;
  __weak __typeof(self.dispatcher) weakDispatcher = self.dispatcher;
  ProceduralBlock dismissalCompletion = ^{
    if (completion) {
      completion();
    }

    // Properly shutdown all coordinators started either by this coordinator or
    // by the scene controller. This should include Quick Delete, History and
    // the Privacy Settings.
    [weakSelf clearPresentedStateWithCompletion:nil dismissOmnibox:YES];
    // The protocol might not have a valid target when the shutdown of Quick
    // Delete is happening at the same time the UI is being shutdown.
    if ([weakDispatcher
            dispatchingForProtocol:@protocol(ApplicationCommands)]) {
      id<ApplicationCommands> applicationCommandsHandler =
          HandlerForProtocol(weakDispatcher, ApplicationCommands);
      [applicationCommandsHandler dismissModalDialogsWithCompletion:nil];
    }
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

#pragma mark - WelcomeBackPromoCommands

- (void)hideWelcomeBackPromo {
  [_welcomeBackCoordinator stop];
  _welcomeBackCoordinator = nil;
}

#pragma mark - NotificationsOptInCoordinatorDelegate

- (void)notificationsOptInScreenDidFinish:
    (NotificationsOptInCoordinator*)coordinator {
  CHECK_EQ(coordinator, _notificationsOptInCoordinator);
  [self dismissNotificationsOptIn];
}

#pragma mark - GoogleOneCommands

- (void)showGoogleOneForIdentity:(id<SystemIdentity>)identity
                      entryPoint:(GoogleOneEntryPoint)entryPoint
              baseViewController:(UIViewController*)baseViewController {
  UIViewController* viewController = baseViewController ?: self.viewController;
  _googleOneCoordinator =
      [[GoogleOneCoordinator alloc] initWithBaseViewController:viewController
                                                       browser:self.browser
                                                    entryPoint:entryPoint
                                                      identity:identity];
  [_googleOneCoordinator start];
}

- (void)hideGoogleOne {
  [_googleOneCoordinator stop];
  _googleOneCoordinator = nil;
}

#pragma mark - NonModalSignInPromoCommands

- (void)showNonModalSignInPromoWithType:(SignInPromoType)promoType {
  if (IsNonModalSignInPromoEnabled() && !self.nonModalSignInPromoCoordinator) {
    self.nonModalSignInPromoCoordinator =
        [[NonModalSignInPromoCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:signin::GetRegularBrowser(self.browser)
                             promoType:promoType];
    [self.nonModalSignInPromoCoordinator start];
    self.nonModalSignInPromoCoordinator.delegate = self;
  }
}

#pragma mark - NonModalSignInPromoCoordinatorDelegate

- (void)dismissNonModalSignInPromo:
    (NonModalSignInPromoCoordinator*)coordinator {
  CHECK_EQ(self.nonModalSignInPromoCoordinator, coordinator);
  [self.nonModalSignInPromoCoordinator stop];
  self.nonModalSignInPromoCoordinator.delegate = nil;
  self.nonModalSignInPromoCoordinator = nil;
}

#pragma mark - CollaborationGroupCommands

- (void)
    shareOrManageTabGroup:(const TabGroup*)tabGroup
               entryPoint:
                   (collaboration::CollaborationServiceShareOrManageEntryPoint)
                       entryPoint {
  Browser* browser = self.browser;

  std::unique_ptr<collaboration::IOSCollaborationControllerDelegate> delegate =
      std::make_unique<collaboration::IOSCollaborationControllerDelegate>(
          browser, CreateControllerDelegateParamsFromProfile(
                       self.profile, self.viewController,
                       collaboration::FlowType::kShareOrManage));
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(self.profile);
  collaborationService->StartShareOrManageFlow(
      std::move(delegate), tabGroup->tab_group_id(), entryPoint);
}

#pragma mark - TrustedVaultReauthenticationCoordinatorDelegate

- (void)trustedVaultReauthenticationCoordinatorWantsToBeStopped:
    (TrustedVaultReauthenticationCoordinator*)coordinator {
  CHECK_EQ(coordinator, _trustedVaultReauthenticationCoordinator);
  [self stopTrustedVaultReauthentication];
}

#pragma mark - DownloadListCommands

- (void)hideDownloadList {
  [self.downloadListCoordinator stop];
  self.downloadListCoordinator = nil;
}

- (void)showDownloadList {
  self.downloadListCoordinator = [[DownloadListCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.downloadListCoordinator start];
}

#pragma mark - DataControlsCommands

- (void)showDataControlsWarningDialog:
            (data_controls::DataControlsDialog::Type)dialogType
                   organizationDomain:(std::string_view)organizationDomain
                             callback:(base::OnceCallback<void(bool)>)callback {
  // If a dialog is already shown, dismiss it before showing a new one.
  if (_dataControlsDialogCoordinator) {
    [_dataControlsDialogCoordinator stop];
  }

  _dataControlsDialogCoordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:self.browser
                      dialogType:dialogType
              organizationDomain:organizationDomain
                        callback:std::move(callback)];
  [_dataControlsDialogCoordinator start];
}

@end

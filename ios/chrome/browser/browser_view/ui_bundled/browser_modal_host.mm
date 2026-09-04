// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/send_tab_to_self/features.h"
#import "components/supervised_user/core/common/features.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/autofill/authentication/coordinator/card_unmask_authentication_coordinator.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_coordinator.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_coordinator.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_coordinator.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/model/autofill_ai_error_dialog_context.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/payments/coordinator/payments_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/infobar_autofill_edit_profile_bottom_sheet_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_passwords_coordinator.h"
#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_coordinator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_constants.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"
#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"
#import "ios/chrome/browser/download/coordinator/pass_kit_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/coordinator/enterprise_dialog_coordinator.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_coordinator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_focus/omnibox_focus_browser_agent.h"
#import "ios/chrome/browser/page_info/coordinator/page_info_coordinator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/passwords/password_breach/coordinator/password_breach_coordinator.h"
#import "ios/chrome/browser/passwords/password_breach/coordinator/password_protection_coordinator.h"
#import "ios/chrome/browser/passwords/password_breach/coordinator/password_protection_coordinator_delegate.h"
#import "ios/chrome/browser/passwords/password_suggestion/coordinator/password_suggestion_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"
#import "ios/chrome/browser/picture_in_picture/coordinator/picture_in_picture_coordinator.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/price_notifications_view_coordinator.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"
#import "ios/chrome/browser/save_to_drive/ui_bundled/save_to_drive_coordinator.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator_delegate.h"
#import "ios/chrome/browser/settings/clear_browsing_data/coordinator/quick_delete_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_share_url_command.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/cobalt_commands.h"
#import "ios/chrome/browser/shared/public/commands/collaboration_group_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/enhanced_calendar_commands.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_protection_commands.h"
#import "ios/chrome/browser/shared/public/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_tracked_items_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/reminder_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_engine_choice_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_command.h"
#import "ios/chrome/browser/shared/public/commands/shared_tab_group_last_tab_closed_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/tips_passwords_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/commands/welcome_back_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/layout_guide/layout_guide_swift.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"
#import "ios/chrome/browser/synced_set_up/public/synced_set_up_utils.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_confirmation_coordinator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/webauthn/coordinator/passkey_incognito_interstitial_coordinator.h"
#import "ios/chrome/browser/webauthn/coordinator/passkey_welcome_screen_coordinator.h"
#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_coordinator.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_coordinator.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/public/provider/chrome/browser/cobalt/cobalt_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// TODO(crbug.com/544595243): Move this inside the SharingParams.
const char kChromeAppStoreUrl[] =
    "https://apps.apple.com/app/id535886823?pt=9008&ct=iosChromeShare&mt=8";

// Histogram name for the IPH dismissal reason.
const char kContextPanelDismissedHistogram[] =
    "IOS.ContextualPanel.IPH.DismissedReason";

}  // namespace

@interface BrowserModalHost () <ActivityServiceCommands,
                                ActorOverlayCommands,
                                AddContactsCommands,
                                AutofillCommands,
                                CobaltCommands,
                                CollaborationGroupCommands,
                                ContextualPanelEntrypointIPHCommands,
                                ContextualSheetCommands,
                                CountryCodePickerCommands,
                                DockingPromoCommands,
                                DownloadListCommands,
                                DriveFilePickerCommands,
                                EnhancedCalendarCommands,
                                EnterpriseCommands,
                                EnterprisePromptCoordinatorDelegate,
                                FileUploadPanelCommands,
                                GoogleOneCommands,
                                IOSPasskeyClientCommands,
                                LevelUpCommands,
                                MiniMapCommands,
                                NonModalSignInPromoCommands,
                                NonModalSignInPromoCoordinatorDelegate,
                                PageActionMenuCommands,
                                PageInfoCommands,
                                ParentAccessCommands,
                                PasskeyWelcomeScreenCoordinatorDelegate,
                                PasswordBreachCommands,
                                PasswordProtectionCommands,
                                PasswordProtectionCoordinatorDelegate,
                                PasswordSuggestionCommands,
                                PictureInPictureCommands,
                                PolicyChangeCommands,
                                PriceTrackedItemsCommands,
                                QuickDeleteCommands,
                                ReminderNotificationsCommands,
                                ReminderNotificationsCoordinatorDelegate,
                                SaveToDriveCommands,
                                SaveToPhotosCommands,
                                SearchEngineChoiceCommands,
                                SearchEngineChoiceCoordinatorDelegate,
                                SendTabToSelfCommands,
                                SendTabToSelfCoordinatorDelegate,
                                SharedTabGroupLastTabAlertCommands,
                                StoreKitCoordinatorDelegate,
                                SyncedSetUpCommands,
                                SyncedSetUpCoordinatorDelegate,
                                TabPickerCommands,
                                TipsPasswordsCommands,
                                TipsPasswordsCoordinatorDelegate,
                                UnitConversionCommands,
                                WebContentCommands,
                                WelcomeBackPromoCommands,
                                WhatsNewCommands>

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* activeWebState;
// The dispatcher.
@property(nonatomic, readonly) CommandDispatcher* dispatcher;

@end

@implementation BrowserModalHost {
  raw_ptr<Browser> _browser;
  __weak UIViewController* _baseViewController;

  // The list of sub-coordinators
  ActorOverlayCoordinator* _actorOverlayCoordinator;
  AddContactsCoordinator* _addContactsCoordinator;
  AmbientAutofillNoticeCoordinator* _ambientAutofillNoticeCoordinator;
  AutofillAIPrivateInferenceNoticeCoordinator*
      _autofillAIPrivateInferenceNoticeCoordinator;
  AutofillAiErrorDialogCoordinator* _autofillAiErrorDialogCoordinator;
  AutofillAISaveEntityCoordinator* _autofillAISaveEntityCoordinator;
  AutofillEditProfileCoordinator* _autofillEditProfileCoordinator;
  AutofillErrorDialogCoordinator* _autofillErrorDialogCoordinator;
  AutofillProgressDialogCoordinator* _autofillProgressDialogCoordinator;
  BubbleViewControllerPresenter* _contextualPanelEntrypointHelpPresenter;
  CardUnmaskAuthenticationCoordinator* _cardUnmaskAuthenticationCoordinator;
  ChromeCoordinator* _cobaltCoordinator;
  ChromeCoordinator* _cobaltAlertCoordinator;
  ChromeCoordinator* _cobaltPopupCoordinator;
  ContextualSheetCoordinator* _contextualSheetCoordinator;
  CountryCodePickerCoordinator* _countryCodePickerCoordinator;
  CredentialSuggestionBottomSheetCoordinator*
      _credentialSuggestionBottomSheetCoordinator;
  DockingPromoCoordinator* _dockingPromoCoordinator;
  DownloadListCoordinator* _downloadListCoordinator;
  RootDriveFilePickerCoordinator* _driveFilePickerCoordinator;
  InfobarAutofillEditProfileBottomSheetHandler* _editProfileBottomSheetHandler;
  EnhancedCalendarCoordinator* _enhancedCalendarCoordinator;
  EnterpriseDialogCoordinator* _enterpriseDialogCoordinator;
  EnterprisePromptCoordinator* _enterprisePromptCoordinator;
  API_AVAILABLE(ios(18.4))
  FileUploadPanelCoordinator* _fileUploadPanelCoordinator;
  GoogleOneCoordinator* _googleOneCoordinator;
  TabGroupConfirmationCoordinator* _lastTabClosingAlert;
  LevelUpCoordinator* _levelUpCoordinator;
  MiniMapCoordinator* _miniMapCoordinator;
  NonModalSignInPromoCoordinator* _nonModalSignInPromoCoordinator;
  PageActionMenuCoordinator* _pageActionMenuCoordinator;
  PageInfoCoordinator* _pageInfoCoordinator;
  ParentAccessCoordinator* _parentAccessCoordinator;
  PasskeyCreationBottomSheetCoordinator* _passkeyCreationBottomSheetCoordinator;
  PasskeyIncognitoInterstitialCoordinator* _passkeyIncognitoCoordinator;
  PasskeyWelcomeScreenCoordinator* _passkeyWelcomeScreenCoordinator;
  PassKitCoordinator* _passKitCoordinator;
  PasswordBreachCoordinator* _passwordBreachCoordinator;
  PasswordProtectionCoordinator* _passwordProtectionCoordinator;
  PasswordSuggestionCoordinator* _passwordSuggestionCoordinator;
  PaymentsScanSaveAndFillOfferBottomSheetCoordinator* _paymentsScanCoordinator;
  PaymentsSuggestionBottomSheetCoordinator*
      _paymentsSuggestionBottomSheetCoordinator;
  PictureInPictureCoordinator* _pictureInPictureCoordinator;
  PriceNotificationsViewCoordinator* _priceNotificationsViewCoordinator;
  QuickDeleteCoordinator* _quickDeleteCoordinator;
  ReminderNotificationsCoordinator* _reminderNotificationsCoordinator;
  SaveCardBottomSheetCoordinator* _saveCardBottomSheetCoordinator;
  SaveToDriveCoordinator* _saveToDriveCoordinator;
  SaveToPhotosCoordinator* _saveToPhotosCoordinator;
  SearchEngineChoiceCoordinator* _searchEngineChoiceCoordinator;
  ProceduralBlock _searchEngineChoiceClosedBlock;
  SendTabToSelfCoordinator* _sendTabToSelfCoordinator;
  SharingCoordinator* _sharingCoordinator;
  StoreKitCoordinator* _storeKitCoordinator;
  SyncedSetUpCoordinator* _syncedSetUpCoordinator;
  ProceduralBlock _runAfterSyncedSetUpDismissal;
  TabPickerCoordinator* _tabPickerCoordinator;
  TipsPasswordsCoordinator* _tipsPasswordsCoordinator;
  UnitConversionCoordinator* _unitConversionCoordinator;
  VirtualCardEnrollmentBottomSheetCoordinator*
      _virtualCardEnrollmentBottomSheetCoordinator;
  WalletReminderNoticeCoordinator* _walletReminderNoticeCoordinator;
  WelcomeBackCoordinator* _welcomeBackCoordinator;
  WhatsNewCoordinator* _whatsNewCoordinator;
}

#pragma mark - Public

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)startHostingCommandProtocols {
  [self startDispatching];
}

- (void)setBaseViewControllerForModals:(UIViewController*)baseViewController {
  _baseViewController = baseViewController;
}

- (void)stopHostingCommandProtocols {
  [self clearPresentedState];
  [self.dispatcher stopDispatchingToTarget:self];

  _browser = nullptr;
}

- (void)clearPresentedState {
  [self hideActorOverlay];
  [self hideAddContacts];
  [self dismissSaveCardBottomSheet];
  [self dismissEditAddressBottomSheet];
  [self dismissAutofillErrorDialog];
  [self dismissAutofillAiErrorDialog];
  [self dismissAutofillProgressDialog];
  [self dismissSaveEntityDialog];
  [self dismissAmbientAutofillNotice];
  [self dismissAutofillAIPrivateInferenceNotice];
  [self dismissScanCardSaveAndFillBottomSheet];
  [self dismissPaymentsBottomSheet];
  [_cardUnmaskAuthenticationCoordinator stop];
  _cardUnmaskAuthenticationCoordinator = nil;
  [_virtualCardEnrollmentBottomSheetCoordinator stop];
  _virtualCardEnrollmentBottomSheetCoordinator = nil;
  [self hideCobalt];
  [self hideCobaltAlert];
  [self hideCobaltPopup];
  [self hideContextualSheet];
  [self dismissContextualPanelEntrypointIPH:NO];
  [self hideCountryCodePicker];
  [self dismissDockingPromo];
  if (IsDownloadListEnabled()) {
    [self hideDownloadList];
  }
  [self hideDriveFilePicker];
  [self hideEnhancedCalendarBottomSheet];
  [self dismissEnterpriseWarningDialog];
  [self stopEnterprisePromptCoordinator];
  if (@available(iOS 18.4, *)) {
    [self hideFileUploadPanel];
  }
  [self hideGoogleOne];
  [_lastTabClosingAlert stop];
  _lastTabClosingAlert = nil;
  [self dismissLevelUp];
  [self stopPassKitCoordinator];
  [self dismissPasskeyCreation];
  [self dismissPasskeySuggestions];
  [self stopPasskeyWelcomeScreenCoordinator];
  [self dismissPasskeyIncognitoInterstitial];
  [self stopPasswordBreach];
  [self stopPasswordProtectionCoordinator];
  [self closePasswordSuggestion];
  [self dismissPictureInPicture];
  [self hideMiniMap];
  [self stopNonModalSignInPromoCoordinator];
  [self hidePageInfo];
  [self dismissPageActionMenuWithCompletion:nil];
  [self hideParentAccessBottomSheet];
  [self hidePriceTrackedItems];
  [self stopQuickDelete];
  [self stopReminderNotificationsCoordinator];
  [self hideSaveToDriveAnimated:NO];
  [self stopSaveToPhotos];
  [self stopSearchEngineChoiceScreen];
  [self stopSendTabToSelf];
  [self stopSharingSheet];
  [self stopStoreKitCoordinator];
  [self stopSyncedSetUpCoordinator];
  [self stopTabPickerCoordinator];
  [self dismissPasswordsTip];
  [self hideUnitConversion];
  [self dismissWalletReminderNotice];
  [self hideWelcomeBackPromo];
  [self dismissWhatsNew];
}

#pragma mark - Private properties

- (web::WebState*)activeWebState {
  WebStateList* webStateList = _browser->GetWebStateList();
  return webStateList ? webStateList->GetActiveWebState() : nullptr;
}

- (CommandDispatcher*)dispatcher {
  return _browser->GetCommandDispatcher();
}

#pragma mark - Private helpers

// Returns the active view controller from the scene UI provider, falling back
// to `_baseViewController` if unavailable.
- (UIViewController*)activeBaseViewController {
  return _browser->GetSceneState().controller.activeViewController
             ?: _baseViewController;
}

// Stops Send Tab To Self.
- (void)stopSendTabToSelf {
  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator.delegate = nil;
  _sendTabToSelfCoordinator = nil;
}

// Stops the Enterprise Prompt coordinator.
- (void)stopEnterprisePromptCoordinator {
  [_enterprisePromptCoordinator stop];
  _enterprisePromptCoordinator.delegate = nil;
  _enterprisePromptCoordinator = nil;
}

// Stops the sharing sheet.
- (void)stopSharingSheet {
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;
}

// Stops the passkey welcome screen coordinator.
- (void)stopPasskeyWelcomeScreenCoordinator {
  [_passkeyWelcomeScreenCoordinator stop];
  _passkeyWelcomeScreenCoordinator.delegate = nil;
  _passkeyWelcomeScreenCoordinator = nil;
}

// Stops the password breach coordinator.
- (void)stopPasswordBreach {
  [_passwordBreachCoordinator stop];
  _passwordBreachCoordinator = nil;
}

// Stops the password protection coordinator.
- (void)stopPasswordProtectionCoordinator {
  [_passwordProtectionCoordinator stop];
  _passwordProtectionCoordinator.delegate = nil;
  _passwordProtectionCoordinator = nil;
}

// Stops the reminder notifications coordinator.
- (void)stopReminderNotificationsCoordinator {
  [_reminderNotificationsCoordinator stop];
  _reminderNotificationsCoordinator.delegate = nil;
  _reminderNotificationsCoordinator = nil;
}

// Stops quick delete and opens the password settings after all the view
// controllers on top of BrowserViewController have been dismissed.
- (void)stopQuickDeleteAndOpenPasswordSettingsPageAfterVCDismissed {
  [self stopQuickDelete];
  [HandlerForProtocol(self.dispatcher, SettingsCommands)
      showPasswordSettingsFromViewController:self.activeBaseViewController];
}

// Exits fullscreen mode.
- (void)exitFullscreen {
  if (IsFullscreenRefactoringEnabled()) {
    [HandlerForProtocol(self.dispatcher, FullscreenCommands)
        exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                         animated:YES];
  } else {
    FullscreenController::FromBrowser(_browser)->ExitFullscreen(
        FullscreenModeTransitionTrigger::kForcedByCode);
  }
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

// Stops the tab picker coordinator.
- (void)stopTabPickerCoordinator {
  [_tabPickerCoordinator stop];
  _tabPickerCoordinator = nil;
}

// Stops the non-modal sign-in promo coordinator.
- (void)stopNonModalSignInPromoCoordinator {
  [_nonModalSignInPromoCoordinator stop];
  _nonModalSignInPromoCoordinator.delegate = nil;
  _nonModalSignInPromoCoordinator = nil;
}

// Stops the PassKit coordinator.
- (void)stopPassKitCoordinator {
  [_passKitCoordinator stop];
  _passKitCoordinator = nil;
}

// Starts the StoreKitCoordinator with the given productParameters.
- (void)startStoreKitCoordinatorWithParameters:
    (NSDictionary*)productParameters {
  _storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _storeKitCoordinator.delegate = self;
  _storeKitCoordinator.iTunesProductParameters = productParameters;
  [_storeKitCoordinator start];
}

// Stops the StoreKit coordinator.
- (void)stopStoreKitCoordinator {
  [_storeKitCoordinator stop];
  _storeKitCoordinator.delegate = nil;
  _storeKitCoordinator = nil;
}

// Handles cleanup and metric logging when the Contextual Panel Entrypoint IPH
// is dismissed.
// TODO(crbug.com/555640717): Investigate if this can be moved to an
// IPH-specific object.
- (void)contextualPanelEntrypointIPHDidDismissWithConfig:
            (base::WeakPtr<ContextualPanelItemConfiguration>)config
                                         dismissalReason:
                                             (IPHDismissalReasonType)reason {
  ContextualPanelItemConfiguration* configPointer = config.get();
  if (!configPointer) {
    return;
  }

  // TODO(crbug.com/555654175): This should be using a state object to propagate
  // the change to the different object rather than forwarding it.
  [HandlerForProtocol(self.dispatcher, ContextualPanelEntrypointCommands)
      notifyContextualPanelEntrypointIPHDismissed];

  ProfileIOS* profile = _browser->GetProfile();
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  if (!engagementTracker || !_contextualPanelEntrypointHelpPresenter) {
    return;
  }

  engagementTracker->Dismissed(*configPointer->iph_feature);
  _contextualPanelEntrypointHelpPresenter = nil;

  if (reason == IPHDismissalReasonType::kTappedAnchorView ||
      reason == IPHDismissalReasonType::kTappedIPH) {
    [HandlerForProtocol(self.dispatcher, ContextualSheetCommands)
        openContextualSheet];
    base::UmaHistogramEnumeration(
        kContextPanelDismissedHistogram,
        ContextualPanelIPHDismissedReason::UserInteracted);
    return;
  }

  if (reason == IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView ||
      reason == IPHDismissalReasonType::kTappedClose) {
    engagementTracker->NotifyEvent(
        configPointer->iph_entrypoint_explicitly_dismissed);
    base::UmaHistogramEnumeration(
        kContextPanelDismissedHistogram,
        ContextualPanelIPHDismissedReason::UserDismissed);
    return;
  }

  if (reason == IPHDismissalReasonType::kTimedOut) {
    base::UmaHistogramEnumeration(kContextPanelDismissedHistogram,
                                  ContextualPanelIPHDismissedReason::TimedOut);
    return;
  }

  base::UmaHistogramEnumeration(kContextPanelDismissedHistogram,
                                ContextualPanelIPHDismissedReason::Other);
}

// Starts dispatching to the various command protocols.
- (void)startDispatching {
  NSArray<Protocol*>* protocols = @[
    @protocol(ActivityServiceCommands),
    @protocol(ActorOverlayCommands),
    @protocol(AddContactsCommands),
    @protocol(AutofillCommands),
    @protocol(CobaltCommands),
    @protocol(CollaborationGroupCommands),
    @protocol(ContextualPanelEntrypointIPHCommands),
    @protocol(ContextualSheetCommands),
    @protocol(CountryCodePickerCommands),
    @protocol(DockingPromoCommands),
    @protocol(DownloadListCommands),
    @protocol(DriveFilePickerCommands),
    @protocol(EnhancedCalendarCommands),
    @protocol(EnterpriseCommands),
    @protocol(FileUploadPanelCommands),
    @protocol(GoogleOneCommands),
    @protocol(IOSPasskeyClientCommands),
    @protocol(LevelUpCommands),
    @protocol(MiniMapCommands),
    @protocol(NonModalSignInPromoCommands),
    @protocol(PageActionMenuCommands),
    @protocol(PageInfoCommands),
    @protocol(ParentAccessCommands),
    @protocol(PasswordBreachCommands),
    @protocol(PasswordProtectionCommands),
    @protocol(PasswordSuggestionCommands),
    @protocol(PictureInPictureCommands),
    @protocol(PolicyChangeCommands),
    @protocol(PriceTrackedItemsCommands),
    @protocol(QuickDeleteCommands),
    @protocol(ReminderNotificationsCommands),
    @protocol(SaveToDriveCommands),
    @protocol(SaveToPhotosCommands),
    @protocol(SearchEngineChoiceCommands),
    @protocol(SendTabToSelfCommands),
    @protocol(SharedTabGroupLastTabAlertCommands),
    @protocol(SyncedSetUpCommands),
    @protocol(TabPickerCommands),
    @protocol(TipsPasswordsCommands),
    @protocol(UnitConversionCommands),
    @protocol(WebContentCommands),
    @protocol(WelcomeBackPromoCommands),
    @protocol(WhatsNewCommands),
  ];

  for (Protocol* protocol in protocols) {
    [self.dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }
}

#pragma mark - ActivityServiceCommands

- (void)stopAndStartSharingCoordinatorFromView:(UIView*)shareButton {
  SharingScenario scenario = IsReaderModeActiveInWebState(self.activeWebState)
                                 ? SharingScenario::ShareInReaderMode
                                 : SharingScenario::TabShareButton;
  SharingParams* params = [[SharingParams alloc] initWithScenario:scenario];

  // Exit fullscreen if needed to make sure that share button is visible.
  [self exitFullscreen];

  if (!shareButton) {
    shareButton = [LayoutGuideCenterForBrowser(_browser)
        referencedViewUnderName:kShareButtonGuide];
  }

  [_sharingCoordinator stop];
  _sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser
                                                      params:params
                                                  sourceItem:shareButton];
  [_sharingCoordinator start];
}

- (void)showShareSheetFromShareButton:(UIView*)shareButton {
  if (_sharingCoordinator) {
    [_sharingCoordinator
        cancelIfNecessaryAndCreateNewCoordinatorFromView:shareButton];
  } else {
    [self stopAndStartSharingCoordinatorFromView:shareButton];
  }
}

- (void)showShareSheetForChromeApp {
  // TODO(crbug.com/544594466): Move this to a convenience initializer.
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
  // TODO(crbug.com/544588871): This should be part of SharingCoordinator's
  // start.
  [self exitFullscreen];

  UIView* originView = [LayoutGuideCenterForBrowser(_browser)
      referencedViewUnderName:kToolsMenuGuide];
  [_sharingCoordinator stop];
  _sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser
                                                      params:params
                                                  sourceItem:originView];
  [_sharingCoordinator start];
}

- (void)showShareSheetForHighlight:(ShareHighlightCommand*)command {
  // TODO(crbug.com/544594466): Move this to a convenience initializer.
  SharingParams* params =
      [[SharingParams alloc] initWithURL:command.URL
                                   title:command.title
                          additionalText:command.selectedText
                                scenario:SharingScenario::SharedHighlight];

  [_sharingCoordinator stop];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                          params:params
                      sourceView:command.sourceView
                      sourceRect:command.sourceRect];
  [_sharingCoordinator start];
}

- (void)showShareSheetForURL:(ActivityServiceShareURLCommand*)command {
  SharingParams* params = [[SharingParams alloc]
      initWithURL:command.URL
            title:command.title
         scenario:SharingScenario::ShareInWebContextMenu];

  [_sharingCoordinator stop];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                          params:params
                      sourceView:command.sourceView
                      sourceRect:command.sourceRect];
  [_sharingCoordinator start];
}

#pragma mark - ActorOverlayCommands

- (void)showActorOverlayForWebState:(web::WebState*)webState {
  [_actorOverlayCoordinator stop];
  _actorOverlayCoordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                        webState:webState];
  [_actorOverlayCoordinator start];
}

- (void)hideActorOverlay {
  [_actorOverlayCoordinator stop];
  _actorOverlayCoordinator = nil;
}

#pragma mark - AddContactsCommands

- (void)presentAddContactsForPhoneNumber:(NSString*)phoneNumber {
  [_addContactsCoordinator stop];
  _addContactsCoordinator = [[AddContactsCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                     phoneNumber:phoneNumber];
  [_addContactsCoordinator start];
}

- (void)hideAddContacts {
  [_addContactsCoordinator stop];
  _addContactsCoordinator = nil;
}

#pragma mark - AutofillCommands

- (void)showCredentialBottomSheet:(const autofill::FormActivityParams&)params {
  // Do not present the bottom sheet if it is already being presented.
  if (_credentialSuggestionBottomSheetCoordinator) {
    return;
  }

  // Do not present the bottom sheet when the omnibox is being used to not
  // disrupt the user.
  // TODO(crbug.com/544602750): Check if it should be removed from here or added
  // to the other methods.
  if (OmniboxFocusBrowserAgent::FromBrowser(_browser)->IsOmniboxFocused()) {
    return;
  }
  _credentialSuggestionBottomSheetCoordinator =
      [[CredentialSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                              params:params
                            delegate:self.passwordControllerDelegate];
  [_credentialSuggestionBottomSheetCoordinator start];
}

- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params {
  // Do not present the bottom sheet if it is already being presented.
  if (_paymentsSuggestionBottomSheetCoordinator) {
    return;
  }
  _paymentsSuggestionBottomSheetCoordinator =
      [[PaymentsSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                              params:params];
  // TODO(crbug.com/544597172): Don't pass the handler to the coordinator.
  _paymentsSuggestionBottomSheetCoordinator.settingsHandler =
      HandlerForProtocol(self.dispatcher, SettingsCommands);
  [_paymentsSuggestionBottomSheetCoordinator start];
}

- (void)dismissPaymentsBottomSheet {
  [_paymentsSuggestionBottomSheetCoordinator stop];
  _paymentsSuggestionBottomSheetCoordinator = nil;
}

- (void)showScanCardSaveAndFillBottomSheet:
    (const autofill::FormActivityParams&)params {
  if (_paymentsScanCoordinator) {
    return;
  }
  _paymentsScanCoordinator =
      [[PaymentsScanSaveAndFillOfferBottomSheetCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                              params:params];
  [_paymentsScanCoordinator start];
}

- (void)dismissScanCardSaveAndFillBottomSheet {
  [_paymentsScanCoordinator stop];
  _paymentsScanCoordinator = nil;
}

- (void)showCardUnmaskAuthentication {
  [_cardUnmaskAuthenticationCoordinator stop];
  _cardUnmaskAuthenticationCoordinator =
      [[CardUnmaskAuthenticationCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser];
  _cardUnmaskAuthenticationCoordinator.shouldStartWithCvcAuth = NO;

  [_cardUnmaskAuthenticationCoordinator start];
}

- (void)dismissCardUnmaskAuthentication {
  [_cardUnmaskAuthenticationCoordinator stop];
  _cardUnmaskAuthenticationCoordinator = nil;
}

- (void)continueCardUnmaskWithOtpAuth {
  // This assumes the card unmask authentication coordinator is already created
  // by the showCardUnmaskAuthentication function above. Otherwise do nothing.
  [_cardUnmaskAuthenticationCoordinator continueWithOtpAuth];
}

- (void)continueCardUnmaskWithCvcAuth {
  if (_cardUnmaskAuthenticationCoordinator) {
    // If the coordinator exists, it means that multiple authentication options
    // are provided and we have already presented the authentication selection
    // dialog, and the navigation controller is already created. Upon user
    // selection, we should show the CVC input dialog by pushing the view to the
    // navigation stack.
    [_cardUnmaskAuthenticationCoordinator continueWithCvcAuth];
  } else {
    // If the coordinator does not exist, it means there is only one
    // authentication option (CVC auth) provided, and the navigation controller
    // is not yet created, so we skip the authentication selection step and
    // start directly with the CVC input dialog.
    _cardUnmaskAuthenticationCoordinator =
        [[CardUnmaskAuthenticationCoordinator alloc]
            initWithBaseViewController:_baseViewController
                               browser:_browser];
    _cardUnmaskAuthenticationCoordinator.shouldStartWithCvcAuth = YES;
    [_cardUnmaskAuthenticationCoordinator start];
  }
}

- (void)showSaveCardBottomSheetOnOriginWebState:(web::WebState*)originWebState {
  if (_saveCardBottomSheetCoordinator) {
    [_saveCardBottomSheetCoordinator stop];
  }

  if (self.activeWebState != originWebState) {
    // Do not show the sheet if the current tab is not the one where the
    // bottomsheet show request was triggered from.
    return;
  }

  _saveCardBottomSheetCoordinator = [[SaveCardBottomSheetCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_saveCardBottomSheetCoordinator start];
}

- (void)dismissSaveCardBottomSheet {
  [_saveCardBottomSheetCoordinator stop];
  _saveCardBottomSheetCoordinator = nil;
}

- (void)showWalletReminderNoticeOnOriginWebState:(web::WebState*)originWebState
                               legalMessageLines:(autofill::LegalMessageLines)
                                                     legalMessageLines {
  [_walletReminderNoticeCoordinator stop];

  if (self.activeWebState != originWebState) {
    return;
  }

  _walletReminderNoticeCoordinator = [[WalletReminderNoticeCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
               legalMessageLines:legalMessageLines];
  [_walletReminderNoticeCoordinator start];
}

- (void)dismissWalletReminderNotice {
  [_walletReminderNoticeCoordinator stop];
  _walletReminderNoticeCoordinator = nil;
}

- (void)showVirtualCardEnrollmentBottomSheet:
            (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                              originWebState:(web::WebState*)originWebState {
  if (_virtualCardEnrollmentBottomSheetCoordinator) {
    [_virtualCardEnrollmentBottomSheetCoordinator stop];
  }

  if (self.activeWebState != originWebState) {
    // Do not show the sheet if the current tab is not the one where the credit
    // card was originally saved.
    return;
  }

  _virtualCardEnrollmentBottomSheetCoordinator =
      [[VirtualCardEnrollmentBottomSheetCoordinator alloc]
             initWithUIModel:std::move(model)
          baseViewController:_baseViewController
                     browser:_browser];
  [_virtualCardEnrollmentBottomSheetCoordinator start];
}

- (void)dismissVirtualCardEnrollmentBottomSheet {
  [_virtualCardEnrollmentBottomSheetCoordinator stop];
  _virtualCardEnrollmentBottomSheetCoordinator = nil;
}

- (void)showEditAddressBottomSheet {
  if (_autofillEditProfileCoordinator) {
    [_autofillEditProfileCoordinator stop];
  }

  // TODO(crbug.com/544600810): This object should be created and owned by the
  // AutofillEditProfileCoordinator directly.
  _editProfileBottomSheetHandler =
      [[InfobarAutofillEditProfileBottomSheetHandler alloc]
          initWithWebState:self.activeWebState];

  _autofillEditProfileCoordinator = [[AutofillEditProfileCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                         handler:_editProfileBottomSheetHandler];
  [_autofillEditProfileCoordinator start];
}

- (void)dismissEditAddressBottomSheet {
  [_autofillEditProfileCoordinator stop];
  _autofillEditProfileCoordinator = nil;
  _editProfileBottomSheetHandler = nil;
}

- (void)legacyResetAutofillSuggestionsLoadingStates {
  // TODO(crbug.com/543386292): Remove this.
  [HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands)
      resetAutofillSuggestionsLoadingStates];
}

- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext {
  [_autofillErrorDialogCoordinator stop];

  _autofillErrorDialogCoordinator = [[AutofillErrorDialogCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                    errorContext:std::move(errorContext)];
  [_autofillErrorDialogCoordinator start];
}

- (void)dismissAutofillErrorDialog {
  [_autofillErrorDialogCoordinator stop];
  _autofillErrorDialogCoordinator = nil;
}

- (void)showAutofillAiErrorDialog:
    (autofill::AutofillAiErrorDialogContext)errorContext {
  [_autofillAiErrorDialogCoordinator stop];

  _autofillAiErrorDialogCoordinator = [[AutofillAiErrorDialogCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                    errorContext:std::move(errorContext)];
  _autofillAiErrorDialogCoordinator.autofillCommandsHandler =
      HandlerForProtocol(self.dispatcher, AutofillCommands);
  [_autofillAiErrorDialogCoordinator start];
}

- (void)dismissAutofillAiErrorDialog {
  [_autofillAiErrorDialogCoordinator stop];
  _autofillAiErrorDialogCoordinator = nil;
}

- (void)showAutofillProgressDialog {
  [_autofillProgressDialogCoordinator stop];

  _autofillProgressDialogCoordinator =
      [[AutofillProgressDialogCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser];
  [_autofillProgressDialogCoordinator start];
}

- (void)dismissAutofillProgressDialog {
  [_autofillProgressDialogCoordinator stop];
  _autofillProgressDialogCoordinator = nil;
}

- (void)showSaveEntityDialog:(autofill::SaveEntityParams)params {
  [_autofillAISaveEntityCoordinator stop];

  _autofillAISaveEntityCoordinator = [[AutofillAISaveEntityCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                          params:std::move(params)];
  [_autofillAISaveEntityCoordinator start];
}

- (void)dismissSaveEntityDialog {
  [_autofillAISaveEntityCoordinator stop];
  _autofillAISaveEntityCoordinator = nil;
}

- (void)showAmbientAutofillNotice:(const autofill::FormActivityParams&)params {
  [_ambientAutofillNoticeCoordinator stop];
  _ambientAutofillNoticeCoordinator = [[AmbientAutofillNoticeCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                          params:params];
  [_ambientAutofillNoticeCoordinator start];
}

- (void)dismissAmbientAutofillNotice {
  [_ambientAutofillNoticeCoordinator markNoticeShown];
  [_ambientAutofillNoticeCoordinator stop];
  _ambientAutofillNoticeCoordinator = nil;
}

- (void)showAutofillAIPrivateInferenceNotice {
  [_autofillAIPrivateInferenceNoticeCoordinator stop];
  _autofillAIPrivateInferenceNoticeCoordinator =
      [[AutofillAIPrivateInferenceNoticeCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser];
  [_autofillAIPrivateInferenceNoticeCoordinator start];
}

- (void)dismissAutofillAIPrivateInferenceNotice {
  [_autofillAIPrivateInferenceNoticeCoordinator stop];
  _autofillAIPrivateInferenceNoticeCoordinator = nil;
}

#pragma mark - CobaltCommands

- (void)showCobalt {
  if (_cobaltCoordinator) {
    return;
  }
  _cobaltCoordinator =
      ios::provider::CreateCobaltCoordinator(_baseViewController, _browser);
  CHECK(_cobaltCoordinator);
  [_cobaltCoordinator start];
}

- (void)hideCobalt {
  [_cobaltCoordinator stop];
  _cobaltCoordinator = nil;
}

- (void)showCobaltAlertWithTitle:(NSString*)title
                         message:(NSString*)message
                      completion:(void (^)(bool))completion {
  // If `_cobaltAlertCoordinator` is present hide it first.
  if (_cobaltAlertCoordinator) {
    [self hideCobaltAlert];
  }

  // If `_cobaltCoordinator` is present hide it first.
  if (_cobaltCoordinator) {
    [self hideCobalt];
  }
  _cobaltAlertCoordinator = ios::provider::CreateCobaltAlertCoordinator(
      _baseViewController, _browser, title, message, completion);
  CHECK(_cobaltAlertCoordinator);
  [_cobaltAlertCoordinator start];
}

- (void)hideCobaltAlert {
  [_cobaltAlertCoordinator stop];
  _cobaltAlertCoordinator = nil;
}

- (void)showCobaltPopupViewController:(UIViewController*)popupViewController
                           completion:(void (^)(NSError*))completion {
  // If `_cobaltPopupCoordinator` is present hide it first.
  if (_cobaltPopupCoordinator) {
    [self hideCobaltPopup];
  }

  // If `_cobaltCoordinator` is present hide it first.
  if (_cobaltCoordinator) {
    [self hideCobalt];
  }
  _cobaltPopupCoordinator = ios::provider::CreateCobaltPopupCoordinator(
      _baseViewController, _browser, popupViewController, completion);
  CHECK(_cobaltPopupCoordinator);
  [_cobaltPopupCoordinator start];
}

- (void)hideCobaltPopup {
  [_cobaltPopupCoordinator stop];
  _cobaltPopupCoordinator = nil;
}

#pragma mark - CollaborationGroupCommands

- (void)
    shareOrManageTabGroup:(const TabGroup*)tabGroup
               entryPoint:
                   (collaboration::CollaborationServiceShareOrManageEntryPoint)
                       entryPoint {
  std::unique_ptr<collaboration::IOSCollaborationControllerDelegate> delegate =
      std::make_unique<collaboration::IOSCollaborationControllerDelegate>(
          _browser, CreateControllerDelegateParamsFromProfile(
                        _browser->GetProfile(), _baseViewController,
                        collaboration::FlowType::kShareOrManage));
  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          _browser->GetProfile());
  collaborationService->StartShareOrManageFlow(
      std::move(delegate), tabGroup->tab_group_id(), entryPoint);
}

#pragma mark - ContextualPanelEntrypointIPHCommands

// TODO(crbug.com/555650699): commands should not return a value.
- (BOOL)showContextualPanelEntrypointIPHWithConfig:
            (ContextualPanelItemConfiguration*)config
                                       anchorPoint:(CGPoint)anchorPoint
                                   isBottomOmnibox:(BOOL)isBottomOmnibox {
  ContextualPanelItemConfiguration& configRef = CHECK_DEREF(config);

  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(_browser->GetProfile());

  if (!engagementTracker) {
    return NO;
  }

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<ContextualPanelItemConfiguration> config_weak_ptr =
      configRef.weak_ptr_factory.GetWeakPtr();
  CallbackWithIPHDismissalReasonType dismissalCallback = ^(
      IPHDismissalReasonType reason) {
    [weakSelf contextualPanelEntrypointIPHDidDismissWithConfig:config_weak_ptr
                                               dismissalReason:reason];
  };

  _contextualPanelEntrypointHelpPresenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:base::SysUTF8ToNSString(configRef.iph_text)
                      title:base::SysUTF8ToNSString(configRef.iph_title)
             arrowDirection:isBottomOmnibox ? BubbleArrowDirectionDown
                                            : BubbleArrowDirectionUp
                  alignment:BubbleAlignmentTopOrLeading
                 bubbleType:BubbleViewTypeRich
            pageControlPage:BubblePageControlPageNone
          dismissalCallback:dismissalCallback];

  _contextualPanelEntrypointHelpPresenter.voiceOverAnnouncement =
      base::SysUTF8ToNSString(configRef.iph_text);
  _contextualPanelEntrypointHelpPresenter.ignoreWebContentAreaInteractions =
      YES;
  _contextualPanelEntrypointHelpPresenter.customBubbleVisibilityDuration =
      kLargeContextualPanelEntrypointDisplayDuration.InSecondsF();

  // Early return if the bubble wouldn't fit in its parent view.
  if (![_contextualPanelEntrypointHelpPresenter
          canPresentInView:self.activeBaseViewController.view
               anchorPoint:anchorPoint]) {
    _contextualPanelEntrypointHelpPresenter = nil;
    return NO;
  }

  // Do this check last as the FET needs to know the IPH can be shown.
  if (!engagementTracker->ShouldTriggerHelpUI(*configRef.iph_feature)) {
    _contextualPanelEntrypointHelpPresenter = nil;
    return NO;
  }

  [_contextualPanelEntrypointHelpPresenter
      presentInViewController:self.activeBaseViewController
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
  CHECK(activeWebState, base::NotFatalUntil::M160);
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(activeWebState);
  if (!contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    return;
  }

  _contextualSheetCoordinator = [[ContextualSheetCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _contextualSheetCoordinator.presenter =
      (id<ContextualSheetPresenter>)_baseViewController;
  [_contextualSheetCoordinator start];
}

- (void)hideContextualSheet {
  [_contextualSheetCoordinator stop];
  _contextualSheetCoordinator = nil;
}

#pragma mark - CountryCodePickerCommands

- (void)presentCountryCodePickerForPhoneNumber:(NSString*)phoneNumber {
  [_countryCodePickerCoordinator stop];
  _countryCodePickerCoordinator = [[CountryCodePickerCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _countryCodePickerCoordinator.phoneNumber = phoneNumber;
  [_countryCodePickerCoordinator start];
}

- (void)hideCountryCodePicker {
  [_countryCodePickerCoordinator stop];
  _countryCodePickerCoordinator = nil;
}

#pragma mark - DockingPromoCommands

- (void)showDockingPromoWithPromosUIHandler:
    (id<PromosManagerUIHandler>)promosUIHandler {
  [_dockingPromoCoordinator stop];
  _dockingPromoCoordinator = [[DockingPromoCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _dockingPromoCoordinator.promosUIHandler = promosUIHandler;
  [_dockingPromoCoordinator start];
}

- (void)dismissDockingPromo {
  [_dockingPromoCoordinator stop];
  _dockingPromoCoordinator = nil;
}

#pragma mark - DownloadListCommands

- (void)hideDownloadList {
  [_downloadListCoordinator stop];
  _downloadListCoordinator = nil;
}

- (void)showDownloadList {
  if (_downloadListCoordinator) {
    [self hideDownloadList];
  }
  _downloadListCoordinator = [[DownloadListCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_downloadListCoordinator start];
}

#pragma mark - DriveFilePickerCommands

- (void)showDriveFilePicker {
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
  if (!(base::FeatureList::IsEnabled(kIOSChooseFromDriveSignedOut) ||
        AuthenticationServiceFactory::GetForProfile(_browser->GetProfile())
            ->HasPrimaryIdentity())) {
    // Drive can be accessed if either:
    //   - The user has a primary identity, or
    //   - The kIOSChooseFromDriveSignedOut flag is enabled.
    // Since neither of these are true, the file picker is not presented.
    tab_helper->SetIsPresentingFilePicker(false);
    return;
  }
  // The user should not have been offered to use the drive if they are in
  // incognito.
  CHECK_EQ(_browser->type(), Browser::Type::kRegular);
  _driveFilePickerCoordinator = [[RootDriveFilePickerCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                        webState:activeWebState
                   forComposebox:NO];
  [_driveFilePickerCoordinator start];
}

- (void)hideDriveFilePicker {
  [_driveFilePickerCoordinator stop];
  _driveFilePickerCoordinator = nil;
}

- (void)setDriveFilePickerSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  CHECK(selectedIdentity);
  [_driveFilePickerCoordinator setSelectedIdentity:selectedIdentity];
}

- (void)showDriveFilePickerWithComposeboxDelegate:
            (id<ComposeboxPickerPresenterDelegate>)delegate
                               baseViewController:
                                   (UIViewController*)baseViewController
                               maxAttachmentCount:(NSUInteger)maxAttachmentCount
                                snackbarPresenter:(ComposeboxSnackbarPresenter*)
                                                      snackbarPresenter {
  // In the context of the compose box the user should not have been offered to
  // use the drive if they are not signed-in.
  CHECK(AuthenticationServiceFactory::GetForProfile(_browser->GetProfile())
            ->HasPrimaryIdentity());
  // The user should not have been offered to use the drive if they are in
  // incognito.
  CHECK_EQ(_browser->type(), Browser::Type::kRegular);

  // If there is a coordinator, stop it before showing it again.
  [self hideDriveFilePicker];
  web::WebState* activeWebState = self.activeWebState;
  if (!activeWebState || activeWebState->IsBeingDestroyed()) {
    return;
  }

  _driveFilePickerCoordinator = [[RootDriveFilePickerCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:_browser
                        webState:activeWebState
                   forComposebox:YES];
  _driveFilePickerCoordinator.composeboxDelegate = delegate;
  _driveFilePickerCoordinator.maxAttachmentCount = maxAttachmentCount;
  _driveFilePickerCoordinator.snackbarPresenter = snackbarPresenter;
  [_driveFilePickerCoordinator start];
}

#pragma mark - EnhancedCalendarCommands

- (void)showEnhancedCalendarWithConfig:
    (EnhancedCalendarConfiguration*)enhancedCalendarConfig {
  [_enhancedCalendarCoordinator stop];
  _enhancedCalendarCoordinator = [[EnhancedCalendarCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
          enhancedCalendarConfig:enhancedCalendarConfig];
  [_enhancedCalendarCoordinator start];
}

- (void)hideEnhancedCalendarBottomSheet {
  [_enhancedCalendarCoordinator stop];
  _enhancedCalendarCoordinator = nil;
}

#pragma mark - EnterpriseCommands

- (void)showEnterpriseWarningDialog:(enterprise::DialogType)dialogType
                 organizationDomain:(std::string_view)organizationDomain
                           callback:(base::OnceCallback<void(bool)>)callback {
  [_enterpriseDialogCoordinator stop];

  _enterpriseDialogCoordinator = [[EnterpriseDialogCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                      dialogType:dialogType
              organizationDomain:organizationDomain
                        callback:std::move(callback)];
  [_enterpriseDialogCoordinator start];
}

- (void)dismissEnterpriseWarningDialog {
  [_enterpriseDialogCoordinator stop];
  _enterpriseDialogCoordinator = nil;
}

#pragma mark - FileUploadPanelCommands

- (void)showFileUploadPanel API_AVAILABLE(ios(18.4)) {
  ChooseFileTabHelper* tabHelper =
      ChooseFileTabHelper::FromWebState(self.activeWebState);
  if (!tabHelper || !tabHelper->IsChoosingFiles()) {
    return;
  }
  [_fileUploadPanelCoordinator stop];
  _fileUploadPanelCoordinator = [[FileUploadPanelCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_fileUploadPanelCoordinator start];
}

- (void)hideFileUploadPanel API_AVAILABLE(ios(18.4)) {
  [_fileUploadPanelCoordinator stop];
  _fileUploadPanelCoordinator = nil;
}

#pragma mark - GoogleOneCommands

- (void)showGoogleOneForIdentity:(id<SystemIdentity>)identity
                      entryPoint:(GoogleOneEntryPoint)entryPoint
              baseViewController:(UIViewController*)baseViewController {
  [self hideGoogleOne];
  UIViewController* viewController = baseViewController ?: _baseViewController;
  _googleOneCoordinator =
      [[GoogleOneCoordinator alloc] initWithBaseViewController:viewController
                                                       browser:_browser
                                                    entryPoint:entryPoint
                                                      identity:identity];
  [_googleOneCoordinator start];
}

- (void)showGoogleOneForURL:(const GURL&)inputURL {
  [self hideGoogleOne];
  _googleOneCoordinator = [[GoogleOneCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                      entryPoint:GoogleOneEntryPoint::kDeepLink
                        inputURL:inputURL];
  [_googleOneCoordinator start];
}

- (void)hideGoogleOne {
  [_googleOneCoordinator stop];
  _googleOneCoordinator = nil;
}

#pragma mark - IOSPasskeyClientCommands

- (void)showPasskeyCreationBottomSheet:
    (webauthn::IOSPasskeyClient::RequestInfo)requestInfo {
  [_passkeyCreationBottomSheetCoordinator stop];
  _passkeyCreationBottomSheetCoordinator =
      [[PasskeyCreationBottomSheetCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                         requestInfo:std::move(requestInfo)];
  [_passkeyCreationBottomSheetCoordinator start];
}

- (void)dismissPasskeyCreation {
  [_passkeyCreationBottomSheetCoordinator stop];
  _passkeyCreationBottomSheetCoordinator = nil;
}

- (void)showPasskeySuggestionBottomSheet:
    (webauthn::IOSPasskeyClient::RequestInfo)requestInfo {
  [_credentialSuggestionBottomSheetCoordinator stop];
  _credentialSuggestionBottomSheetCoordinator =
      [[CredentialSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                         requestInfo:std::move(requestInfo)
                            delegate:self.passwordControllerDelegate];
  [_credentialSuggestionBottomSheetCoordinator start];
}

- (void)dismissPasskeySuggestions {
  [_credentialSuggestionBottomSheetCoordinator stop];
  _credentialSuggestionBottomSheetCoordinator = nil;
}

- (void)showPasskeyWelcomeScreenForPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                                completion:
                                    (webauthn::PasskeyWelcomeScreenAction)
                                        completion {
  [self stopPasskeyWelcomeScreenCoordinator];
  _passkeyWelcomeScreenCoordinator = [[PasskeyWelcomeScreenCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                         purpose:purpose
                      completion:completion];
  _passkeyWelcomeScreenCoordinator.delegate = self;
  [_passkeyWelcomeScreenCoordinator start];
}

- (void)dismissPasskeyWelcomeScreen {
  [self stopPasskeyWelcomeScreenCoordinator];
}

- (void)showPasskeyIncognitoInterstitial:
    (webauthn::IOSPasskeyClient::InterstitialCallback)callback {
  if (_passkeyIncognitoCoordinator) {
    return;
  }

  _passkeyIncognitoCoordinator =
      [[PasskeyIncognitoInterstitialCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                            callback:std::move(callback)];

  [_passkeyIncognitoCoordinator start];
}

- (void)dismissPasskeyIncognitoInterstitial {
  [_passkeyIncognitoCoordinator stop];
  _passkeyIncognitoCoordinator = nil;
}

- (void)cancelPasskeyRequest:
    (webauthn::IOSPasskeyClient::RequestInfo)requestInfo {
  if ([_passkeyCreationBottomSheetCoordinator hasPendingRequest:requestInfo]) {
    [self dismissPasskeyCreation];
    return;
  }

  if ([_credentialSuggestionBottomSheetCoordinator
          hasPendingRequest:requestInfo]) {
    [self dismissPasskeySuggestions];
    return;
  }

  if (_passkeyIncognitoCoordinator) {
    [self dismissPasskeyIncognitoInterstitial];
  }
}

- (void)showCredentialProviderPromoOnPasskeyCreated {
  id<PromosManagerCommands> promosManagerHandler =
      HandlerForProtocol(self.dispatcher, PromosManagerCommands);
  [promosManagerHandler
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SuccessfulPasskeyCreation];
}

#pragma mark - PasskeyWelcomeScreenCoordinatorDelegate

- (void)passkeyWelcomeScreenCoordinatorWantsToBeDismissed:
    (PasskeyWelcomeScreenCoordinator*)coordinator {
  // TODO(crbug.com/543366923): Remove this.
  CHECK_EQ(coordinator, _passkeyWelcomeScreenCoordinator);
  [self stopPasskeyWelcomeScreenCoordinator];
}

#pragma mark - LevelUpCommands

- (void)showLevelUp {
  [_levelUpCoordinator stop];
  _levelUpCoordinator =
      [[LevelUpCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser];
  [_levelUpCoordinator start];
}

- (void)dismissLevelUp {
  [_levelUpCoordinator stop];
  _levelUpCoordinator = nil;
}

#pragma mark - MiniMapCommands

- (void)presentMiniMapWithIPHForText:(NSString*)text {
  [_miniMapCoordinator stop];
  // TODO(crbug.com/544607135): Do the check inside the coordinator.
  MiniMapMode mode = base::FeatureList::IsEnabled(kIOSMiniMapLinkifiedAddress)
                         ? MiniMapMode::kMapNativePreviewURL
                         : MiniMapMode::kMap;
  _miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser
                                                        text:text
                                                         URL:nil
                                                     withIPH:YES
                                                        mode:mode];
  [_miniMapCoordinator start];
}

- (void)presentMiniMapForText:(NSString*)text {
  [_miniMapCoordinator stop];
  // TODO(crbug.com/544607135): Do the check inside the coordinator.
  MiniMapMode mode = base::FeatureList::IsEnabled(kIOSMiniMapLinkifiedAddress)
                         ? MiniMapMode::kMapNativePreviewURL
                         : MiniMapMode::kMap;
  _miniMapCoordinator =
      [[MiniMapCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser
                                                        text:text
                                                         URL:nil
                                                     withIPH:NO
                                                        mode:mode];
  [_miniMapCoordinator start];
}

- (void)presentMiniMapDirectionsForText:(NSString*)text {
  [_miniMapCoordinator stop];
  _miniMapCoordinator = [[MiniMapCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                            text:text
                             URL:nil
                         withIPH:NO
                            mode:MiniMapMode::kDirections];
  [_miniMapCoordinator start];
}

- (void)presentMiniMapNativePreviewForURL:(NSURL*)URL {
  [_miniMapCoordinator stop];
  _miniMapCoordinator = [[MiniMapCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                            text:nil
                             URL:URL
                         withIPH:NO
                            mode:MiniMapMode::kMapNativePreviewURL];
  [_miniMapCoordinator start];
}

- (void)hideMiniMap {
  [_miniMapCoordinator stop];
  _miniMapCoordinator = nil;
}

#pragma mark - NonModalSignInPromoCommands

- (void)showNonModalSignInPromoWithType:(NonModalSignInPromoType)promoType {
  if (_nonModalSignInPromoCoordinator) {
    return;
  }
  _nonModalSignInPromoCoordinator = [[NonModalSignInPromoCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:signin::GetRegularBrowser(_browser)
                       promoType:promoType];
  _nonModalSignInPromoCoordinator.delegate = self;
  [_nonModalSignInPromoCoordinator start];
}

#pragma mark - NonModalSignInPromoCoordinatorDelegate

- (void)dismissNonModalSignInPromo:
    (NonModalSignInPromoCoordinator*)coordinator {
  // TODO(crbug.com/555077798): Replace this by command protocol.
  CHECK_EQ(_nonModalSignInPromoCoordinator, coordinator);
  [self stopNonModalSignInPromoCoordinator];
}

#pragma mark - PageActionMenuCommands

- (void)showPageActionMenu {
  if (!self.activeWebState) {
    // The page action menu requires an active tab. Return early if there is
    // none.
    return;
  }
  // TODO(crbug.com/465505528) Propagate page action menu entry point source to
  // page action menu coordinator.
  [_pageActionMenuCoordinator stop];
  _pageActionMenuCoordinator = [[PageActionMenuCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_pageActionMenuCoordinator start];
}

- (void)dismissPageActionMenuWithCompletion:(ProceduralBlock)completion {
  [_pageActionMenuCoordinator stopWithCompletion:completion];
  _pageActionMenuCoordinator = nil;
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  PageInfoCoordinator* pageInfoCoordinator = [[PageInfoCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_pageInfoCoordinator stop];
  _pageInfoCoordinator = pageInfoCoordinator;
  [_pageInfoCoordinator start];
}

- (void)hidePageInfo {
  [_pageInfoCoordinator stop];
  _pageInfoCoordinator = nil;
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
  [_parentAccessCoordinator stop];
  _parentAccessCoordinator = [[ParentAccessCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                       targetURL:targetURL
         filteringBehaviorReason:filteringBehaviorReason
                      completion:completion];
  [_parentAccessCoordinator start];
}

- (void)hideParentAccessBottomSheet {
  [_parentAccessCoordinator stop];
  _parentAccessCoordinator = nil;
}

#pragma mark - PasswordBreachCommands

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType {
  [_passwordBreachCoordinator stop];
  _passwordBreachCoordinator = [[PasswordBreachCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                        leakType:leakType];
  [_passwordBreachCoordinator start];
}

#pragma mark - PasswordProtectionCommands

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  [self stopPasswordProtectionCoordinator];
  _passwordProtectionCoordinator = [[PasswordProtectionCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                     warningText:warningText];
  _passwordProtectionCoordinator.delegate = self;
  [_passwordProtectionCoordinator startWithCompletion:completion];
}

#pragma mark - PasswordProtectionCoordinatorDelegate

- (void)passwordProtectionCoordinatorWantsToBeStopped:
    (PasswordProtectionCoordinator*)coordinator {
  // TODO(crbug.com/543366924): Remove this.
  CHECK_EQ(_passwordProtectionCoordinator, coordinator);
  [self stopPasswordProtectionCoordinator];
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
  if (_passwordSuggestionCoordinator) {
    return;
  }

  _passwordSuggestionCoordinator = [[PasswordSuggestionCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
              passwordSuggestion:passwordSuggestion
                           frame:frame
                 decisionHandler:decisionHandler
                       proactive:proactive];
  [_passwordSuggestionCoordinator start];
}

- (void)closePasswordSuggestion {
  [_passwordSuggestionCoordinator stop];
  _passwordSuggestionCoordinator = nil;
}

#pragma mark - PictureInPictureCommands

- (void)showPictureInPictureWithConfig:(PictureInPictureConfiguration*)config {
  [_pictureInPictureCoordinator stop];

  UIViewController* baseViewController = [self activeBaseViewController];
  _pictureInPictureCoordinator = [[PictureInPictureCoordinator alloc]
      initWithConfiguration:config
         baseViewController:baseViewController
                    browser:_browser];
  [_pictureInPictureCoordinator start];
}

- (void)dismissPictureInPicture {
  [_pictureInPictureCoordinator stop];
  _pictureInPictureCoordinator = nil;
}

- (void)dismissPictureInPictureIfNotPipRestore {
  [_pictureInPictureCoordinator dismissIfNotPipRestore];
}

#pragma mark - PolicyChangeCommands

- (void)showForceSignedOutPrompt {
  // TODO(crbug.com/545540830): Check if it should have an early return or if
  // the coordinator should be stopped instead.
  if (!_enterprisePromptCoordinator) {
    _enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:_baseViewController
                           browser:_browser
                        promptType:EnterprisePromptTypeForceSignOut];
    _enterprisePromptCoordinator.delegate = self;
  }
  [_enterprisePromptCoordinator start];
}

- (void)showSyncDisabledPrompt {
  // TODO(crbug.com/545540830): Check if it should have an early return or if
  // the coordinator should be stopped instead.
  if (!_enterprisePromptCoordinator) {
    _enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:_baseViewController
                           browser:_browser
                        promptType:EnterprisePromptTypeSyncDisabled];
    _enterprisePromptCoordinator.delegate = self;
  }
  [_enterprisePromptCoordinator start];
}

- (void)showRestrictAccountSignedOutPrompt {
  SceneState* sceneState = _browser->GetSceneState();
  if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    // TODO(crbug.com/545540830): Check if it should have an early return or if
    // the coordinator should be stopped instead.
    if (!_enterprisePromptCoordinator) {
      _enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser
                          promptType:
                              EnterprisePromptTypeRestrictAccountSignedOut];
      _enterprisePromptCoordinator.delegate = self;
    }
    [_enterprisePromptCoordinator start];
  } else {
    __weak BrowserModalHost* weakSelf = self;
    // TODO(crbug.com/545541991): Don't dispatch_after here, this should be
    // triggered by the activation level observer.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf showRestrictAccountSignedOutPrompt];
                   });
  }
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  // TODO(crbug.com/545535699): Use a command instead of a delegate.
  [self stopEnterprisePromptCoordinator];
}

#pragma mark - PriceTrackedItemsCommands

- (void)showPriceTrackedItems {
  [self showPriceTrackedItems:NO];
}

- (void)showPriceTrackedItemsWithCurrentPage {
  [self showPriceTrackedItems:YES];
}

- (void)hidePriceTrackedItems {
  [_priceNotificationsViewCoordinator stop];
  _priceNotificationsViewCoordinator = nil;
}

- (void)presentPriceTrackedItemsWhileBrowsingIPH {
  [HandlerForProtocol(self.dispatcher, HelpCommands)
      presentInProductHelpWithType:InProductHelpType::
                                       kPriceNotificationsWhileBrowsing];
}

- (void)showPriceTrackedItems:(BOOL)showCurrentPage {
  [_priceNotificationsViewCoordinator stop];
  _priceNotificationsViewCoordinator =
      [[PriceNotificationsViewCoordinator alloc]
          initWithBaseViewController:_baseViewController
                             browser:_browser];
  _priceNotificationsViewCoordinator.showCurrentPage = showCurrentPage;
  [_priceNotificationsViewCoordinator start];
}

#pragma mark - QuickDeleteCommands

// TODO(crbug.com/555685925): Rename this method. Also, the parameter is almost
// always YES except in one case where it's YES only on tablet form factors.
// Ideally the child coordinator should be able to decide how to present,
// including the animation.
- (void)showQuickDeleteAndCanPerformRadialWipeAnimation:
    (BOOL)canPerformRadialWipeAnimation {
  CHECK(!_browser->GetProfile()->IsOffTheRecord());

  [_quickDeleteCoordinator stop];

  _quickDeleteCoordinator = [[QuickDeleteCoordinator alloc]
         initWithBaseViewController:
             top_view_controller::TopPresentedViewControllerFrom(
                 _browser->GetSceneState().window.rootViewController)
                            browser:_browser
      canPerformRadialWipeAnimation:canPerformRadialWipeAnimation];
  [_quickDeleteCoordinator start];
}

- (void)stopQuickDelete {
  [_quickDeleteCoordinator stop];
  _quickDeleteCoordinator = nil;
}

- (void)stopQuickDeleteAndOpenPasswordSettingsPage {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissalCompletion = ^{
    [weakSelf stopQuickDeleteAndOpenPasswordSettingsPageAfterVCDismissed];
  };
  [_baseViewController dismissViewControllerAnimated:YES
                                          completion:dismissalCompletion];
}

- (void)stopQuickDeleteForAnimationWithCompletion:(ProceduralBlock)completion {
  // TODO(crbug.com/555682992): Investigate why QuickDelete is doing so much UI
  // management.

  // If BrowserViewController has not presented any view controller (i.e. QD has
  // been dismissed) and the tab grid is also not visible, then just trigger
  // `completion` immediately.
  if (!_baseViewController.presentedViewController &&
      !_browser->GetSceneState().controller.isTabGridVisible) {
    if (completion) {
      completion();
    }
    [self stopQuickDelete];
    return;
  }

  // If BrowserViewController has presented a view controller, then dismiss
  // every VC on top of it.
  __weak __typeof(self.dispatcher) weakDispatcher = self.dispatcher;

  // TODO(crbug.com/555685927): This block is too long. Also, why is it
  // introducing another way of dismissing all UI by calling the existing
  // commands?
  ProceduralBlock dismissalCompletion = ^{
    if (completion) {
      completion();
    }

    // Properly shutdown all coordinators started either by this coordinator or
    // by the scene controller. This should include Quick Delete, History and
    // the Privacy Settings.
    [HandlerForProtocol(weakDispatcher, BrowserCoordinatorCommands)
        clearPresentedStateWithCompletion:nil
                           dismissOmnibox:YES];
    // The protocol might not have a valid target when the shutdown of Quick
    // Delete is happening at the same time the UI is being shutdown.
    if ([weakDispatcher dispatchingForProtocol:@protocol(SceneCommands)]) {
      id<SceneCommands> sceneHandler =
          HandlerForProtocol(weakDispatcher, SceneCommands);
      [sceneHandler dismissModalDialogsWithCompletion:nil];
    }
  };
  [_baseViewController dismissViewControllerAnimated:YES
                                          completion:dismissalCompletion];
}

#pragma mark - ReminderNotificationsCommands

- (void)showSetTabReminderUI:(SetTabReminderEntryPoint)entryPoint {
  CHECK(send_tab_to_self::AreIOSTabRemindersEnabled());

  [self stopReminderNotificationsCoordinator];
  _reminderNotificationsCoordinator = [[ReminderNotificationsCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _reminderNotificationsCoordinator.delegate = self;
  [_reminderNotificationsCoordinator start];
}

#pragma mark - ReminderNotificationsCoordinatorDelegate

- (void)reminderNotificationsCoordinatorWantsToBeDismissed:
    (ReminderNotificationsCoordinator*)coordinator {
  CHECK_EQ(coordinator, _reminderNotificationsCoordinator);
  [self stopReminderNotificationsCoordinator];
}

#pragma mark - SaveToDriveCommands

- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask {
  // If the Save to Drive coordinator is not nil, stop it.
  [self hideSaveToDriveAnimated:NO];

  _saveToDriveCoordinator = [[SaveToDriveCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                    downloadTask:downloadTask];
  [_saveToDriveCoordinator start];
}

- (void)hideSaveToDriveAnimated:(BOOL)animated {
  [_saveToDriveCoordinator stopAnimated:animated];
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

  _saveToPhotosCoordinator = [[SaveToPhotosCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                        imageURL:command.imageURL
                        referrer:command.referrer
                        webState:command.webState.get()
                         frameID:command.frameID
                     frameOrigin:command.frameOrigin];
  [_saveToPhotosCoordinator start];
}

- (void)stopSaveToPhotos {
  [_saveToPhotosCoordinator stop];
  _saveToPhotosCoordinator = nil;
}

#pragma mark - SearchEngineChoiceCommands

- (void)showSearchEngineChoiceScreenWithCompletion:(ProceduralBlock)completion {
  [self stopSearchEngineChoiceScreen];

  _searchEngineChoiceClosedBlock = completion;
  _searchEngineChoiceCoordinator = [[SearchEngineChoiceCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _searchEngineChoiceCoordinator.delegate = self;
  [_searchEngineChoiceCoordinator start];
}

- (void)stopSearchEngineChoiceScreen {
  [_searchEngineChoiceCoordinator stop];
  _searchEngineChoiceCoordinator = nil;
  _searchEngineChoiceClosedBlock = nil;
}

#pragma mark - SearchEngineChoiceCoordinatorDelegate

- (void)choiceScreenWasDismissed:(SearchEngineChoiceCoordinator*)coordinator {
  if (_searchEngineChoiceCoordinator == coordinator) {
    if (ProceduralBlock block =
            std::exchange(_searchEngineChoiceClosedBlock, nil)) {
      block();
    }
  }
}

#pragma mark - SendTabToSelfCommands

- (void)showSendTabToSelfUI:(const GURL&)url
                      title:(NSString*)title
                 entryPoint:(send_tab_to_self::ShareEntryPoint)entryPoint {
  [self sendTabToSelfToDeviceWithURL:url
                               title:title
                            deviceID:nil
                          deviceName:nil
                          entryPoint:entryPoint];
}

- (void)sendTabToSelfToDeviceWithURL:(const GURL&)url
                               title:(NSString*)title
                            deviceID:(NSString*)deviceID
                          deviceName:(NSString*)deviceName
                          entryPoint:
                              (send_tab_to_self::ShareEntryPoint)entryPoint {
  [self stopSendTabToSelf];
  UIViewController* baseViewController = [self activeBaseViewController];
  _sendTabToSelfCoordinator = [[SendTabToSelfCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:_browser
                             url:url
                           title:title
           targetDeviceCacheGUID:deviceID
                targetDeviceName:deviceName
                      entryPoint:entryPoint];
  _sendTabToSelfCoordinator.delegate = self;

  __weak SendTabToSelfCoordinator* weakSendTabToSelfCoordinator =
      _sendTabToSelfCoordinator;
  ExecuteWhenTransitionsComplete(
      ^{
        [weakSendTabToSelfCoordinator start];
      },
      baseViewController);
}

#pragma mark - SendTabToSelfCoordinatorDelegate

- (void)sendTabToSelfCoordinatorWantsToBeStopped:
    (SendTabToSelfCoordinator*)coordinator {
  // TODO(crbug.com/545567389): Use a command instead of a delegate here.
  CHECK_EQ(_sendTabToSelfCoordinator, coordinator, base::NotFatalUntil::M150);
  [self stopSendTabToSelf];
}

#pragma mark - SharedTabGroupLastTabAlertCommands

- (void)showLastTabInSharedGroupAlert:
    (SharedTabGroupLastTabAlertCommand*)command {
  UIViewController* viewController = command.baseViewController
                                         ? command.baseViewController
                                         : _baseViewController;
  UIView* sourceView =
      command.sourceView ? command.sourceView : _baseViewController.view;

  _lastTabClosingAlert = [[TabGroupConfirmationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:_browser
                      actionType:command.actionType
                      sourceView:sourceView];

  // TODO(crbug.com/545566706): This should be done in
  // TabGroupConfirmationCoordinator if possible.
  __weak BrowserModalHost* weakSelf = self;
  _lastTabClosingAlert.primaryAction = ^{
    [weakSelf runLeaveOrDeleteCompletion:command.group
                          viewController:viewController];
  };
  if (command.actionType == TabGroupActionType::kCloseLastTabUnknownRole) {
    // If the user's member role is unknown (i.e. sync not complete yet),
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
// TODO(crbug.com/545566706): This should be done in
// TabGroupConfirmationCoordinator if possible.
- (void)runLeaveOrDeleteCompletion:(const TabGroup*)group
                    viewController:(UIViewController*)viewController {
  __weak BrowserModalHost* weakSelf = self;
  base::OnceCallback<void(
      collaboration::CollaborationControllerDelegate::ResultCallback)>
      completionCallback = base::BindOnce(
          ^(collaboration::CollaborationControllerDelegate::ResultCallback
                resultCallback) {
            BrowserModalHost* strongSelf = weakSelf;
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
          _browser, CreateControllerDelegateParamsFromProfile(
                        _browser->GetProfile(), viewController,
                        collaboration::FlowType::kLeaveOrDelete));
  delegate->SetLeaveOrDeleteConfirmationCallback(std::move(completionCallback));

  collaboration::CollaborationService* collaborationService =
      collaboration::CollaborationServiceFactory::GetForProfile(
          _browser->GetProfile());
  collaboration::CollaborationServiceLeaveOrDeleteEntryPoint entryPoint =
      collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown;
  collaborationService->StartLeaveOrDeleteFlow(
      std::move(delegate), group->tab_group_id(), entryPoint);
  _lastTabClosingAlert = nil;
}

// Replaces the last tab with a New Tab Page (NTP).
// TODO(crbug.com/545566706): This should be done in
// TabGroupConfirmationCoordinator if possible.
- (void)runKeepGroup:(const TabGroup*)group lastTabID:(web::WebStateID)tabID {
  TabGroupService* groupService =
      TabGroupServiceFactory::GetForProfile(_browser->GetProfile());
  WebStateList* webStateList = _browser->GetWebStateList();
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

#pragma mark - SyncedSetUpCommands

- (void)showSyncedSetUpWithDismissalCompletion:(ProceduralBlock)completion {
  CHECK(CanShowSyncedSetUp(_browser->GetProfile()->GetPrefs()));

  _runAfterSyncedSetUpDismissal = [completion copy];

  if (_syncedSetUpCoordinator) {
    // The UI is already active; the stored `completion` will run when it stops.
    return;
  }

  _syncedSetUpCoordinator = [[SyncedSetUpCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _syncedSetUpCoordinator.delegate = self;
  [_syncedSetUpCoordinator start];
}

#pragma mark - SyncedSetUpCoordinatorDelegate

- (void)syncedSetUpCoordinatorDidFinish:(SyncedSetUpCoordinator*)coordinator {
  CHECK_EQ(_syncedSetUpCoordinator, coordinator);
  [self stopSyncedSetUpCoordinator];
}

#pragma mark - StoreKitCoordinatorDelegate

- (void)storeKitCoordinatorWantsToStop:(StoreKitCoordinator*)coordinator {
  // TODO(crbug.com/555077798): Replace this by command protocol.
  CHECK_EQ(_storeKitCoordinator, coordinator);
  [self stopStoreKitCoordinator];
}

#pragma mark - TabPickerCommands

- (void)showTabPickerWithParams:(TabPickerParams*)params
                     completion:(TabPickerCompletionBlock)completion {
  if (_tabPickerCoordinator) {
    return;
  }

  UIViewController* baseViewController = params.baseViewController
                                             ? params.baseViewController
                                             : _baseViewController;

  _tabPickerCoordinator = [[TabPickerCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:_browser];
  _tabPickerCoordinator.params = params;
  _tabPickerCoordinator.tabPickerCompletionBlock = completion;
  _tabPickerCoordinator.tabPickerHandler = self;
  [_tabPickerCoordinator start];
}

- (void)hideTabPicker {
  [self stopTabPickerCoordinator];
}

#pragma mark - TipsPasswordsCommands

- (void)showPasswordsTipForIdentifier:
    (segmentation_platform::TipIdentifier)identifier {
  [_tipsPasswordsCoordinator stop];
  _tipsPasswordsCoordinator = [[TipsPasswordsCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                      identifier:identifier];
  _tipsPasswordsCoordinator.delegate = self;
  [_tipsPasswordsCoordinator start];
}

- (void)dismissPasswordsTip {
  [_tipsPasswordsCoordinator stop];
  _tipsPasswordsCoordinator = nil;
}

#pragma mark - TipsPasswordsCoordinatorDelegate

- (void)tipsPasswordsCoordinatorDidFinish:
    (TipsPasswordsCoordinator*)coordinator {
  // TODO(crbug.com/543335936): Remove this.
  CHECK_EQ(coordinator, _tipsPasswordsCoordinator);
  [self dismissPasswordsTip];
}

#pragma mark - UnitConversionCommands

- (void)presentUnitConversionForSourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location {
  [_unitConversionCoordinator stop];
  _unitConversionCoordinator = [[UnitConversionCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                      sourceUnit:sourceUnit
                 sourceUnitValue:sourceUnitValue
                        location:location];
  [_unitConversionCoordinator start];
}

- (void)hideUnitConversion {
  [_unitConversionCoordinator stop];
  _unitConversionCoordinator = nil;
}

#pragma mark - WebContentCommands

- (void)showAppStoreWithParameters:(NSDictionary*)productParameters {
  __weak __typeof(self) weakSelf = self;
  // Properly start the StoreKitCoordinator in a clean presented state.
  [HandlerForProtocol(self.dispatcher, BrowserCoordinatorCommands)
      clearPresentedStateWithCompletion:^{
        [weakSelf startStoreKitCoordinatorWithParameters:productParameters];
      }
                         dismissOmnibox:YES];
}

- (void)showDialogForPassKitPasses:(NSArray<PKPass*>*)passes {
  if (_passKitCoordinator.passes) {
    // Another pass is being displayed -- early return (this is unexpected).
    return;
  }

  _passKitCoordinator =
      [[PassKitCoordinator alloc] initWithBaseViewController:_baseViewController
                                                     browser:_browser];
  _passKitCoordinator.passes = passes;
  [_passKitCoordinator start];
}

#pragma mark - WelcomeBackPromoCommands

- (void)showWelcomeBackPromoWithPromosUIHandler:
    (id<PromosManagerUIHandler>)promosUIHandler {
  [_welcomeBackCoordinator stop];
  _welcomeBackCoordinator = [[WelcomeBackCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
                 promosUIHandler:promosUIHandler];
  [_welcomeBackCoordinator start];
}

- (void)hideWelcomeBackPromo {
  [_welcomeBackCoordinator stop];
  _welcomeBackCoordinator = nil;
}

#pragma mark - WhatsNewCommands

- (void)showWhatsNew {
  [_whatsNewCoordinator stop];
  _whatsNewCoordinator = [[WhatsNewCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_whatsNewCoordinator start];
}

- (void)showWhatsNewWithPromosUIHandler:
    (id<PromosManagerUIHandler>)promosUIHandler {
  [self showWhatsNew];
  [_whatsNewCoordinator setShouldShowPromoOnDismissWithHandler:promosUIHandler];
}

- (void)dismissWhatsNew {
  [_whatsNewCoordinator stop];
  _whatsNewCoordinator = nil;
}

@end

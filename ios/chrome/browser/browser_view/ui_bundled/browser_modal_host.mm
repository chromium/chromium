// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_passwords_coordinator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/root_drive_file_picker_coordinator.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"
#import "ios/chrome/browser/page_info/coordinator/page_info_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"
#import "ios/chrome/browser/save_to_drive/ui_bundled/save_to_drive_coordinator.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/enhanced_calendar_commands.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_engine_choice_commands.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/tips_passwords_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_coordinator.h"

@interface BrowserModalHost () <ActorOverlayCommands,
                                AddContactsCommands,
                                CountryCodePickerCommands,
                                DriveFilePickerCommands,
                                EnhancedCalendarCommands,
                                FileUploadPanelCommands,
                                LevelUpCommands,
                                PageActionMenuCommands,
                                PageInfoCommands,
                                ParentAccessCommands,
                                SaveToDriveCommands,
                                SaveToPhotosCommands,
                                SearchEngineChoiceCommands,
                                SearchEngineChoiceCoordinatorDelegate,
                                SyncedSetUpCommands,
                                SyncedSetUpCoordinatorDelegate,
                                TipsPasswordsCommands,
                                TipsPasswordsCoordinatorDelegate,
                                UnitConversionCommands,
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
  CountryCodePickerCoordinator* _countryCodePickerCoordinator;
  RootDriveFilePickerCoordinator* _driveFilePickerCoordinator;
  EnhancedCalendarCoordinator* _enhancedCalendarCoordinator;
  API_AVAILABLE(ios(18.4))
  FileUploadPanelCoordinator* _fileUploadPanelCoordinator;
  LevelUpCoordinator* _levelUpCoordinator;
  PageActionMenuCoordinator* _pageActionMenuCoordinator;
  PageInfoCoordinator* _pageInfoCoordinator;
  ParentAccessCoordinator* _parentAccessCoordinator;
  SaveToDriveCoordinator* _saveToDriveCoordinator;
  SaveToPhotosCoordinator* _saveToPhotosCoordinator;
  SearchEngineChoiceCoordinator* _searchEngineChoiceCoordinator;
  ProceduralBlock _searchEngineChoiceClosedBlock;
  SyncedSetUpCoordinator* _syncedSetUpCoordinator;
  ProceduralBlock _runAfterSyncedSetUpDismissal;
  TipsPasswordsCoordinator* _tipsPasswordsCoordinator;
  UnitConversionCoordinator* _unitConversionCoordinator;
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
  [self hideCountryCodePicker];
  [self hideDriveFilePicker];
  [self hideEnhancedCalendarBottomSheet];
  if (@available(iOS 18.4, *)) {
    [self hideFileUploadPanel];
  }
  [self dismissLevelUp];
  [self hidePageInfo];
  [self dismissPageActionMenuWithCompletion:nil];
  [self hideParentAccessBottomSheet];
  [self hideSaveToDrive];
  [self stopSaveToPhotos];
  [self stopSearchEngineChoiceScreen];
  [self stopSyncedSetUpCoordinator];
  [self dismissPasswordsTip];
  [self hideUnitConversion];
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

// Starts dispatching to the various command protocols.
- (void)startDispatching {
  NSArray<Protocol*>* protocols = @[
    @protocol(ActorOverlayCommands),
    @protocol(AddContactsCommands),
    @protocol(CountryCodePickerCommands),
    @protocol(DriveFilePickerCommands),
    @protocol(EnhancedCalendarCommands),
    @protocol(FileUploadPanelCommands),
    @protocol(LevelUpCommands),
    @protocol(PageActionMenuCommands),
    @protocol(PageInfoCommands),
    @protocol(ParentAccessCommands),
    @protocol(SaveToDriveCommands),
    @protocol(SaveToPhotosCommands),
    @protocol(SearchEngineChoiceCommands),
    @protocol(SyncedSetUpCommands),
    @protocol(TipsPasswordsCommands),
    @protocol(UnitConversionCommands),
    @protocol(WhatsNewCommands),
  ];

  for (Protocol* protocol in protocols) {
    [self.dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }
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
                                   (UIViewController*)baseViewController {
  // In the context of the compose box the user should not have been offered to
  // use the drive if they are not signed-in.
  CHECK(AuthenticationServiceFactory::GetForProfile(_browser->GetProfile())
            ->HasPrimaryIdentity());
  // The user should not have been offered to use the drive if they are in
  // incognito.
  CHECK_EQ(_browser->type(), Browser::Type::kRegular);

  if (!base::FeatureList::IsEnabled(kIOSChooseFromDrive)) {
    return;
  }
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

#pragma mark - SaveToDriveCommands

- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask {
  // If the Save to Drive coordinator is not nil, stop it.
  [self hideSaveToDrive];

  _saveToDriveCoordinator = [[SaveToDriveCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser
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

- (void)syncedSetUpCoordinatorWantsToBeDismissed:
    (SyncedSetUpCoordinator*)coordinator {
  CHECK_EQ(_syncedSetUpCoordinator, coordinator);
  [self stopSyncedSetUpCoordinator];
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

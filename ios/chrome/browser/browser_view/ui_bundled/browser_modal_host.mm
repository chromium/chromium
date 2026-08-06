// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_passwords_coordinator.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_coordinator.h"
#import "ios/chrome/browser/page_info/coordinator/page_info_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/enhanced_calendar_commands.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/tips_passwords_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_coordinator.h"

@interface BrowserModalHost () <ActorOverlayCommands,
                                AddContactsCommands,
                                CountryCodePickerCommands,
                                EnhancedCalendarCommands,
                                FileUploadPanelCommands,
                                LevelUpCommands,
                                PageActionMenuCommands,
                                PageInfoCommands,
                                ParentAccessCommands,
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
  EnhancedCalendarCoordinator* _enhancedCalendarCoordinator;
  API_AVAILABLE(ios(18.4))
  FileUploadPanelCoordinator* _fileUploadPanelCoordinator;
  LevelUpCoordinator* _levelUpCoordinator;
  PageActionMenuCoordinator* _pageActionMenuCoordinator;
  PageInfoCoordinator* _pageInfoCoordinator;
  ParentAccessCoordinator* _parentAccessCoordinator;
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
  [self hideEnhancedCalendarBottomSheet];
  if (@available(iOS 18.4, *)) {
    [self hideFileUploadPanel];
  }
  [self dismissLevelUp];
  [self hidePageInfo];
  [self dismissPageActionMenuWithCompletion:nil];
  [self hideParentAccessBottomSheet];
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

// Starts dispatching to the various command protocols.
- (void)startDispatching {
  NSArray<Protocol*>* protocols = @[
    @protocol(ActorOverlayCommands),
    @protocol(AddContactsCommands),
    @protocol(CountryCodePickerCommands),
    @protocol(EnhancedCalendarCommands),
    @protocol(FileUploadPanelCommands),
    @protocol(LevelUpCommands),
    @protocol(PageActionMenuCommands),
    @protocol(PageInfoCommands),
    @protocol(ParentAccessCommands),
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

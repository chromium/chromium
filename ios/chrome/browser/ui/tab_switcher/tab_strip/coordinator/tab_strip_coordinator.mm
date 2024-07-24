// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/context_menu/tab_strip_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface TabStripCoordinator () <CreateOrEditTabGroupCoordinatorDelegate,
                                   TabStripCommands>

// Mediator for updating the TabStrip when the WebStateList changes.
@property(nonatomic, strong) TabStripMediator* mediator;
// Helper providing context menu for tab strip items.
@property(nonatomic, strong) TabStripContextMenuHelper* contextMenuHelper;

@property TabStripViewController* tabStripViewController;

@end

@implementation TabStripCoordinator {
  SharingCoordinator* _sharingCoordinator;
  CreateTabGroupCoordinator* _createTabGroupCoordinator;
  AlertCoordinator* _alertCoordinator;
}

@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.tabStripViewController)
    return;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabStripCommands)];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  CHECK(browserState);
  self.tabStripViewController = [[TabStripViewController alloc] init];
  self.tabStripViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.tabStripViewController.overrideUserInterfaceStyle =
      browserState->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                     : UIUserInterfaceStyleUnspecified;
  self.tabStripViewController.isIncognito = browserState->IsOffTheRecord();

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController
                                     browserList:browserList];
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.browserState = browserState;
  self.mediator.browser = self.browser;
  self.mediator.tabStripHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);

  self.contextMenuHelper = [[TabStripContextMenuHelper alloc]
      initWithBrowserList:browserList
             webStateList:self.browser->GetWebStateList()];
  self.contextMenuHelper.incognito = browserState->IsOffTheRecord();
  self.contextMenuHelper.mutator = self.mediator;
  self.contextMenuHelper.handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabStripCommands);

  self.tabStripViewController.mutator = self.mediator;
  self.tabStripViewController.dragDropHandler = self.mediator;
  self.tabStripViewController.contextMenuProvider = self.contextMenuHelper;
}

- (void)stop {
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;
  [self.contextMenuHelper disconnect];
  self.contextMenuHelper = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(TabStripCommands)];
  self.tabStripViewController = nil;
}

#pragma mark - TabStripCommands

- (void)setNewTabButtonOnTabStripIPHHighlighted:(BOOL)IPHHighlighted {
  [self.tabStripViewController
      setNewTabButtonOnTabStripIPHHighlighted:IPHHighlighted];
}

- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  [self hideTabStripGroupCreation];
  _createTabGroupCoordinator = [[CreateTabGroupCoordinator alloc]
      initTabGroupCreationWithBaseViewController:self.baseViewController
                                         browser:self.browser
                                    selectedTabs:identifiers];
  _createTabGroupCoordinator.delegate = self;
  [_createTabGroupCoordinator start];
}

- (void)showTabStripGroupEditionForGroup:(const TabGroup*)tabGroup {
  [self hideTabStripGroupCreation];
  _createTabGroupCoordinator = [[CreateTabGroupCoordinator alloc]
      initTabGroupEditionWithBaseViewController:self.baseViewController
                                        browser:self.browser
                                       tabGroup:tabGroup];
  _createTabGroupCoordinator.delegate = self;
  [_createTabGroupCoordinator start];
}

- (void)hideTabStripGroupCreation {
  _createTabGroupCoordinator.delegate = nil;
  [_createTabGroupCoordinator stop];
  _createTabGroupCoordinator = nil;
}

- (void)shareItem:(TabSwitcherItem*)item originView:(UIView*)originView {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:item.URL
                                   title:item.title
                                scenario:SharingScenario::TabStripItem];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                          params:params
                      originView:originView];
  [_sharingCoordinator start];
}

- (void)showTabGroupDeletionAlertForTab:(web::WebStateID)tabID
                          originBrowser:(Browser*)browser
                            originIndex:(int)index
                            originGroup:(const TabGroup*)group {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState->IsOffTheRecord()) {
    return;
  }
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  base::WeakPtr<Browser> weakBrowser = browser->AsWeakPtr();
  __weak __typeof(self) weakSelf = self;
  tab_groups::TabGroupVisualData visualDataCopy = group->visual_data();

  NSString* title = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_CONFIRMATION_LAST_TAB_MOVE_TITLE);
  NSString* message;
  if (identity) {
    message = l10n_util::GetNSStringF(
        IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITH_EMAIL,
        base::SysNSStringToUTF16(identity.userEmail));
  } else {
    message = l10n_util::GetNSString(
        IDS_IOS_TAB_GROUP_CONFIRMATION_DELETE_MESSAGE_WITHOUT_EMAIL);
  }
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(
                                          IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)
                               action:^(void) {
                                 [weakSelf dimissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDestructive];
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:^(void) {
                                 if (!weakBrowser) {
                                   return;
                                 }
                                 [weakSelf cancelMoveForTab:tabID
                                              originBrowser:weakBrowser.get()
                                                originIndex:index
                                                 visualData:visualDataCopy];
                                 [weakSelf dimissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator start];
}

- (void)cancelMoveForTab:(web::WebStateID)tabID
           originBrowser:(Browser*)originBrowser
             originIndex:(int)originIndex
              visualData:(const tab_groups::TabGroupVisualData&)visualData {
  [_mediator cancelMoveForTab:tabID
                originBrowser:originBrowser
                  originIndex:originIndex
                   visualData:visualData];
}

#pragma mark - CreateOrEditTabGroupCoordinatorDelegate

- (void)createOrEditTabGroupCoordinatorDidDismiss:
            (CreateTabGroupCoordinator*)coordinator
                                         animated:(BOOL)animated {
  CHECK(coordinator == _createTabGroupCoordinator);
  _createTabGroupCoordinator.animatedDismissal = animated;
  _createTabGroupCoordinator.delegate = nil;
  [_createTabGroupCoordinator stop];
  _createTabGroupCoordinator = nil;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.tabStripViewController;
}

#pragma mark - Public

- (void)hideTabStrip:(BOOL)hidden {
  self.tabStripViewController.view.hidden = hidden;
}

#pragma mark - Private

// Dismisses the alert coordinator.
- (void)dimissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end

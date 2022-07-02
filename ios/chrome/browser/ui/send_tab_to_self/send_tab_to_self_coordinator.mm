// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#include "base/check.h"
#import "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/signin/public/base/consent_level.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/send_tab_to_self/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Snackbar category for activity services.
NSString* const kActivityServicesSnackbarCategory =
    @"ActivityServicesSnackbarCategory";

void ShowSendingMessage(CommandDispatcher* dispatcher, NSString* deviceName) {
  if (!dispatcher) {
    return;
  }

  [HandlerForProtocol(dispatcher, ToolbarCommands)
      triggerToolsMenuButtonAnimation];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  NSString* text =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(deviceName));
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.accessibilityLabel = text;
  message.duration = 2.0;
  message.category = kActivityServicesSnackbarCategory;
  [HandlerForProtocol(dispatcher, SnackbarCommands)
      showSnackbarMessage:message];
}

void OpenManageDevicesTab(CommandDispatcher* dispatcher) {
  if (!dispatcher) {
    return;
  }

  id<ApplicationCommands> handler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  [handler openURLInNewTab:[OpenNewTabCommand
                               commandWithURLFromChrome:
                                   GURL(kGoogleMyAccountDeviceActivityURL)]];
}

}  // namespace

@interface SendTabToSelfCoordinator () <UIViewControllerTransitioningDelegate,
                                        InfobarModalPositioner,
                                        SendTabToSelfModalDelegate>

@property(nonatomic, assign, readonly) GURL url;
@property(nonatomic, copy, readonly) NSString* title;

// The TableViewController that shows the Send Tab To Self UI. This is NOT the
// presented controller, it is wrapped in a UINavigationController.
@property(nonatomic, strong)
    SendTabToSelfTableViewController* sendTabToSelfViewController;
// If non-null, this is called when iOS finishes the animated dismissal of the
// view controllers. This is called after this object is destroyed so it must
// NOT rely on self. Instead the block should retain its dependencies.
@property(nonatomic, copy) ProceduralBlock dismissedCompletion;

@end

@implementation SendTabToSelfCoordinator

#pragma mark - Public

- (id)initWithBaseViewController:(UIViewController*)baseViewController
                         browser:(Browser*)browser
                             url:(const GURL&)url
                           title:(NSString*)title {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (!self) {
    return nil;
  }

  _url = url;
  _title = title;
  return self;
}

#pragma mark - ChromeCoordinator Methods

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  send_tab_to_self::SendTabToSelfSyncService* syncService =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browserState);
  // This modal should not be launched in incognito mode where syncService is
  // undefined.
  DCHECK(syncService);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  DCHECK(accountManagerService);
  ChromeIdentity* account =
      AuthenticationServiceFactory::GetForBrowserState(browserState)
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  DCHECK(account) << "The user must be signed in to share a tab";
  self.sendTabToSelfViewController = [[SendTabToSelfTableViewController alloc]
      initWithDeviceList:syncService->GetSendTabToSelfModel()
                             ->GetTargetDeviceInfoSortedList()
                delegate:self
           accountAvatar:accountManagerService->GetIdentityAvatarWithIdentity(
                             account, IdentityAvatarSize::TableViewIcon)
            accountEmail:account.userEmail];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.sendTabToSelfViewController];

  navigationController.transitioningDelegate = self;
  navigationController.modalPresentationStyle = UIModalPresentationCustom;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Do not call directly, use the hideSendTabToSelfUI() command instead!
- (void)stop {
  DCHECK(self.sendTabToSelfViewController) << "Already stopped";
  [self.baseViewController
      dismissViewControllerAnimated:YES
                         completion:self.dismissedCompletion];
  // Embedders currently don't wait for the dismissal to finish, so might as
  // well reset fields immediately.
  self.sendTabToSelfViewController = nil;
  self.dismissedCompletion = nil;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  SendTabToSelfModalPresentationController* presentationController =
      [[SendTabToSelfModalPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  presentationController.modalPositioner = self;
  return presentationController;
}

#pragma mark - InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  UIView* view = self.sendTabToSelfViewController.view;
  CGSize contentSize = CGSizeZero;
  if (UIScrollView* scrollView = base::mac::ObjCCast<UIScrollView>(view)) {
    CGRect layoutFrame = self.baseViewController.view.bounds;
    layoutFrame.size.width = width;
    scrollView.frame = layoutFrame;
    [scrollView setNeedsLayout];
    [scrollView layoutIfNeeded];
    contentSize = scrollView.contentSize;
  } else {
    contentSize = [view sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)];
  }

  // Since the TableView is contained in a NavigationController get the
  // navigation bar height.
  CGFloat navigationBarHeight =
      self.sendTabToSelfViewController.navigationController.navigationBar.frame
          .size.height;

  return contentSize.height + navigationBarHeight;
}

#pragma mark - SendTabToSelfModalDelegate

- (void)dismissViewControllerAnimated {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) hideSendTabToSelfUI];
}

- (void)sendTabToTargetDeviceCacheGUID:(NSString*)cacheGUID
                      targetDeviceName:(NSString*)deviceName {
  send_tab_to_self::RecordDeviceClicked(
      send_tab_to_self::ShareEntryPoint::kShareMenu);

  SendTabToSelfSyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->GetSendTabToSelfModel()
      ->AddEntry(self.url, base::SysNSStringToUTF8(self.title),
                 base::SysNSStringToUTF8(cacheGUID));

  // ShowSendingMessage() opens UI, so wait for the dialog to be dismissed.
  __weak CommandDispatcher* weakDispatcher =
      self.browser->GetCommandDispatcher();
  self.dismissedCompletion = ^{
    ShowSendingMessage(weakDispatcher, deviceName);
  };
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) hideSendTabToSelfUI];
}

- (void)openManageDevicesTab {
  // OpenManageDevicesTab() opens UI, so wait for the dialog to be dismissed.
  __weak CommandDispatcher* weakDispatcher =
      self.browser->GetCommandDispatcher();
  self.dismissedCompletion = ^{
    OpenManageDevicesTab(weakDispatcher);
  };
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) hideSendTabToSelfUI];
}

@end

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"

#import <memory>
#import <optional>
#import <string_view>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/send_tab_to_self/target_device_list_waiter.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_send_tab.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/infobars/ui_bundled/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_table_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

void OpenManageDevicesTab(CommandDispatcher* dispatcher) {
  if (!dispatcher) {
    return;
  }

  id<SceneCommands> handler = HandlerForProtocol(dispatcher, SceneCommands);
  [handler openURLInNewTab:[OpenNewTabCommand
                               commandWithURLFromChrome:
                                   GURL(kGoogleMyAccountDeviceActivityURL)]];
}

}  // namespace

@interface SendTabToSelfCoordinator () <InfobarModalPositioner,
                                        SendTabToSelfMediatorDelegate,
                                        SendTabToSelfModalDelegate,
                                        UIViewControllerTransitioningDelegate>

@property(nonatomic, assign, readonly) GURL url;
@property(nonatomic, copy, readonly) NSString* title;

// Returns YES if the coordinator is running in direct-send mode, sending the
// tab directly to a specific target device without presenting any picker UI.
@property(nonatomic, assign, readonly) BOOL isDirectSend;

// The TableViewController that shows the Send Tab To Self UI. This is NOT the
// presented controller, it is wrapped in a UINavigationController.
@property(nonatomic, strong) UIViewController* sendTabToSelfViewController;
// If non-null, this is called when iOS finishes the animated dismissal of the
// view controllers. This is called after this object is destroyed so it must
// NOT rely on self. Instead the block should retain its dependencies.
@property(nonatomic, copy) ProceduralBlock dismissedCompletion;
@property(nonatomic, assign) BOOL stopped;

@end

@implementation SendTabToSelfCoordinator {
  id<BrowserCoordinatorCommands> __weak _browserCoordinatorHandler;
  SigninCoordinator* _signinCoordinator;
  // The navigation controller displaying the send tab to self.
  UINavigationController* _navigationController;
  // The mediator of this coordinator.
  SendTabToSelfMediator* _mediator;
  std::unique_ptr<send_tab_to_self::TargetDeviceListWaiter>
      _targetDeviceListWaiter;
  send_tab_to_self::ShareEntryPoint _entryPoint;
  // Non-nil only when the coordinator is initialized in direct-send mode,
  // representing the target device's cache GUID where the tab should be sent.
  NSString* _targetDeviceCacheGUID;
  NSString* _targetDeviceName;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                       url:(const GURL&)url
                                     title:(NSString*)title
                     targetDeviceCacheGUID:(NSString*)targetDeviceCacheGUID
                          targetDeviceName:(NSString*)targetDeviceName
                                entryPoint:(send_tab_to_self::ShareEntryPoint)
                                               entryPoint {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (!self) {
    return nil;
  }

  CHECK(!targetDeviceCacheGUID || targetDeviceName);
  _url = url;
  _title = title;
  _targetDeviceCacheGUID = targetDeviceCacheGUID;
  _targetDeviceName = targetDeviceName;
  _entryPoint = entryPoint;
  _browserCoordinatorHandler = HandlerForProtocol(
      browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                       url:(const GURL&)url
                                     title:(NSString*)title
                                entryPoint:(send_tab_to_self::ShareEntryPoint)
                                               entryPoint {
  return [self initWithBaseViewController:baseViewController
                                  browser:browser
                                      url:url
                                    title:title
                    targetDeviceCacheGUID:nil
                         targetDeviceName:nil
                               entryPoint:entryPoint];
}

#pragma mark - ChromeCoordinator Methods

- (void)start {
  send_tab_to_self::RecordEntryPointInvoked(_entryPoint);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  if (!authService->SigninEnabled()) {
    // Sign-in was disabled after the list of action was opened. Let’s abort.
    // Don’t call anything after this, as `self` is not retained anymore.
    [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
    return;
  }

  // If initialized in direct-send mode, send the tab immediately without
  // presenting the target device picker UI.
  if (self.isDirectSend) {
    [self sendTabToTargetDeviceCacheGUID:_targetDeviceCacheGUID
                        targetDeviceName:_targetDeviceName];
    return;
  }

  _mediator = [[SendTabToSelfMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        self.profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        self.profile)];
  _mediator.delegate = self;
  [self waitAndShow];
}

// Do not call directly, use `[self.delegate
// sendTabToSelfCoordinatorWantsToBeStopped:self]` instead!
- (void)stop {
  DCHECK(!self.stopped) << "Already stopped";
  self.stopped = YES;
  // Abort the waiting if it's still ongoing.
  _targetDeviceListWaiter.reset();
  [self stopSigninCoordinator];
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;
  _browserCoordinatorHandler = nil;
  _title = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:self.dismissedCompletion];
  // Embedders currently don't wait for the dismissal to finish, so might as
  // well reset fields immediately.
  _navigationController = nil;
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

#pragma mark - SendTabToSelfMediatorDelegate

- (void)mediatorWantsToBeStopped:(SendTabToSelfMediator*)mediator {
  CHECK_EQ(mediator, _mediator, base::NotFatalUntil::M150);
  [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
}

- (void)mediatorWantsToRefreshView:(SendTabToSelfMediator*)mediator {
  CHECK_EQ(mediator, _mediator, base::NotFatalUntil::M150);
  if (_signinCoordinator) {
    // Nothing to refresh in case of sign-in. The signin coordinator will deal
    // with the update itself.
    return;
  }
  [self show];
}

#pragma mark - InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  UIView* view = self.sendTabToSelfViewController.view;
  CGSize contentSize = CGSizeZero;
  if (UIScrollView* scrollView = base::apple::ObjCCast<UIScrollView>(view)) {
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
  [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
}

- (void)sendTabToTargetDeviceCacheGUID:(NSString*)cacheGUID
                      targetDeviceName:(NSString*)targetDeviceName {
  SendTabToSelfBrowserAgent* browserAgent =
      SendTabToSelfBrowserAgent::FromBrowser(self.browser);
  if (!browserAgent) {
    [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
    return;
  }

  browserAgent->SendTabToTargetDevice(
      self.url, base::SysNSStringToUTF8(self.title),
      base::SysNSStringToUTF8(cacheGUID),
      base::SysNSStringToUTF8(targetDeviceName), _entryPoint,
      base::BindOnce(^(send_tab_to_self::SendTabToSelfResult result) {
        [self handleTabSentWithResult:result];
      }));
}

- (void)openManageDevicesTab {
  // OpenManageDevicesTab() opens UI, so wait for the dialog to be dismissed.
  __weak CommandDispatcher* weakDispatcher =
      self.browser->GetCommandDispatcher();
  self.dismissedCompletion = ^{
    OpenManageDevicesTab(weakDispatcher);
  };
  [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
}

#pragma mark - Private

// Handles the result when a tab has finished sending to the target device.
- (void)handleTabSentWithResult:(send_tab_to_self::SendTabToSelfResult)result {
  if (self.stopped) {
    return;
  }
  [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
}

// Returns YES if the coordinator is in direct-send mode, which bypasses the
// device picker UI and sends the tab directly to a specific target device.
- (BOOL)isDirectSend {
  return _targetDeviceCacheGUID != nil;
}

// Stops the signin-coordiantor
- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

// Closes the current tab in preparation for changing the profile.
- (void)prepareForChangeProfile {
  [_browserCoordinatorHandler closeCurrentTab];
}

// Wait until a UI can be shown to send tab to self, then shows it.
- (void)waitAndShow {
  std::optional<send_tab_to_self::EntryPointDisplayReason> displayReason =
      [self displayReason];
  if (displayReason == std::nullopt) {
    [self waitForDeviceList];
  } else {
    [self show];
  }
}

- (void)show {
  std::optional<send_tab_to_self::EntryPointDisplayReason> displayReason =
      [self displayReason];
  if (displayReason == std::nullopt) {
    // The coordinator should only be started if the view can be shown, so this
    // case should not occur.
    [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
    return;
  }

  size_t deviceCount = 0;
  if (*displayReason ==
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature) {
    send_tab_to_self::SendTabToSelfSyncService* syncService =
        SendTabToSelfSyncServiceFactory::GetForProfile(self.profile);
    if (syncService && syncService->GetSendTabToSelfModel()) {
      deviceCount = syncService->GetSendTabToSelfModel()
                        ->GetTargetDeviceInfoSortedList()
                        .size();
    }
  }
  send_tab_to_self::RecordTargetDeviceCount(_entryPoint, *displayReason,
                                            deviceCount);

  switch (*displayReason) {
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      [self showSendTabToSelf];
      break;
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn:
    case send_tab_to_self::EntryPointDisplayReason::kOfferReauth:
      [self showSigninPromo];
      break;
  }
}

// Shows the send-tab-to-self sheet, either asking the user to pick a target
// device, or informing them that there are no target devices.
- (void)showSendTabToSelf {
  ProfileIOS* profile = self.profile;
  send_tab_to_self::SendTabToSelfSyncService* syncService =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  // This modal should not be launched in incognito mode where syncService
  // is undefined.
  DCHECK(syncService);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  DCHECK(accountManagerService);
  id<SystemIdentity> account =
      AuthenticationServiceFactory::GetForProfile(profile)
          ->GetPrimaryIdentity();
  DCHECK(account) << "The user must be signed in to share a tab";

  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfEnhancedBottomsheet)) {
    SendTabToSelfBottomSheetViewController* bottomSheet =
        [[SendTabToSelfBottomSheetViewController alloc]
            initWithDeviceList:syncService->GetSendTabToSelfModel()
                                   ->GetTargetDeviceInfoSortedList()
                  accountEmail:account.userEmail
                      delegate:self];
    bottomSheet.parentViewControllerHeight =
        self.baseViewController.view.frame.size.height;
    self.sendTabToSelfViewController = bottomSheet;
    _navigationController = [[UINavigationController alloc]
        initWithRootViewController:self.sendTabToSelfViewController];
    _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* sheet =
        _navigationController.sheetPresentationController;
    if (sheet) {
      sheet.prefersGrabberVisible = YES;
    }
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  } else {
    self.sendTabToSelfViewController = [[SendTabToSelfTableViewController alloc]
        initWithDeviceList:syncService->GetSendTabToSelfModel()
                               ->GetTargetDeviceInfoSortedList()
                  delegate:self
             accountAvatar:GetApplicationContext()
                               ->GetIdentityAvatarProvider()
                               ->GetIdentityAvatar(
                                   account, IdentityAvatarSize::TableViewIcon)
              accountEmail:account.userEmail];
    _navigationController = [[UINavigationController alloc]
        initWithRootViewController:self.sendTabToSelfViewController];
    _navigationController.transitioningDelegate = self;
    _navigationController.modalPresentationStyle = UIModalPresentationCustom;
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

// Shows a signin promo, for the case where the user is not signed in yet and
// thus can't use send-tab-to-self until they sign in.
- (void)showSigninPromo {
  __weak __typeof(self) weakSelf = self;
  SigninCoordinatorCompletionCallback completion = ^(
      SigninCoordinator* coordinator, SigninCoordinatorResult result,
      id<SystemIdentity> completionIdentity) {
    BOOL succeeded = result == SigninCoordinatorResultSuccess;
    [weakSelf onSigninCompleteWithCoordinator:coordinator succeeded:succeeded];
  };
  ChangeProfileContinuationProvider provider = base::BindRepeating(
      &CreateChangeProfileSendTabToOtherDevice, _url, self.title, _entryPoint);
  void (^prepareChangeProfile)() = ^() {
    [weakSelf prepareForChangeProfile];
  };

  SigninContextStyle style = SigninContextStyle::kDefault;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kSendTabToSelfPromo;
  _signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:
          self.baseViewController
                                                      browser:self.browser
                                                 contextStyle:style
                                                  accessPoint:accessPoint
                                         confirmChangeProfile:nil
                                         prepareChangeProfile:
                                             prepareChangeProfile
                                         continuationProvider:provider];
  _signinCoordinator.signinCompletion = completion;
  [_signinCoordinator start];
}

// Called when the sign-in flow is complete.
- (void)onSigninCompleteWithCoordinator:(SigninCoordinator*)coordinator
                              succeeded:(BOOL)succeeded {
  CHECK_EQ(_signinCoordinator, coordinator, base::NotFatalUntil::M151);
  [self stopSigninCoordinator];
  if (!succeeded) {
    [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
    return;
  }
  [self waitForDeviceList];
}

// Waits for the device list to be available and shows it.
- (void)waitForDeviceList {
  __weak __typeof(self) weakSelf = self;
  _targetDeviceListWaiter =
      std::make_unique<send_tab_to_self::TargetDeviceListWaiter>(
          SyncServiceFactory::GetForProfile(self.profile),
          SendTabToSelfSyncServiceFactory::GetForProfile(self.profile), _url,
          base::BindOnce(^{
            [weakSelf onTargetDeviceListReady];
          }));
}

// Called when the list of target devices is ready.
- (void)onTargetDeviceListReady {
  _targetDeviceListWaiter.reset();
  [self show];
}

// Returns the reason for displaying the Send Tab To Self entry point.
- (std::optional<send_tab_to_self::EntryPointDisplayReason>)displayReason {
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(self.profile);
  return service ? service->GetEntryPointDisplayReason(_url) : std::nullopt;
}

@end

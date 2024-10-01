// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <memory>
#import <optional>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Snackbar category for activity services.
NSString* const kActivityServicesSnackbarCategory =
    @"ActivityServicesSnackbarCategory";

class TargetDeviceListWaiter : public syncer::SyncServiceObserver {
 public:
  using GetDisplayReasonCallback = base::RepeatingCallback<
      std::optional<send_tab_to_self::EntryPointDisplayReason>()>;

  // Queries `get_display_reason_callback` until it indicates the device list is
  // known (i.e. until it returns kOfferFeature or kInformNoTargetDevice), then
  // calls `on_list_known_callback`. Destroying the object aborts the waiting.
  TargetDeviceListWaiter(
      syncer::SyncService* sync_service,
      const GetDisplayReasonCallback& get_display_reason_callback,
      base::OnceClosure on_list_known_callback)
      : sync_service_(sync_service),
        get_display_reason_callback_(get_display_reason_callback),
        on_list_known_callback_(std::move(on_list_known_callback)) {
    sync_service_->AddObserver(this);
    OnStateChanged(sync_service_);
  }

  TargetDeviceListWaiter(const TargetDeviceListWaiter&) = delete;
  TargetDeviceListWaiter& operator=(const TargetDeviceListWaiter&) = delete;

  ~TargetDeviceListWaiter() override { sync_service_->RemoveObserver(this); }

  void OnStateChanged(syncer::SyncService*) override {
    std::optional<send_tab_to_self::EntryPointDisplayReason> display_reason =
        get_display_reason_callback_.Run();
    if (!display_reason) {
      // Model starting up, keep waiting.
      return;
    }
    switch (*display_reason) {
      case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn:
        break;
      case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
        sync_service_->RemoveObserver(this);
        std::move(on_list_known_callback_).Run();
        break;
    }
  }

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
  const GetDisplayReasonCallback get_display_reason_callback_;
  base::OnceClosure on_list_known_callback_;
};

void ShowSendingMessage(CommandDispatcher* dispatcher, NSString* deviceName) {
  if (!dispatcher) {
    return;
  }

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  NSString* text =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(deviceName));
  MDCSnackbarMessage* message = CreateSnackbarMessage(text);
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
                                        SendTabToSelfModalDelegate> {
  std::unique_ptr<TargetDeviceListWaiter> _targetDeviceListWaiter;
}

@property(nonatomic, weak, readonly) id<SigninPresenter> signinPresenter;
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
@property(nonatomic, assign) BOOL stopped;

@end

@implementation SendTabToSelfCoordinator

#pragma mark - Public

- (id)initWithBaseViewController:(UIViewController*)baseViewController
                         browser:(Browser*)browser
                 signinPresenter:(id<SigninPresenter>)signinPresenter
                             url:(const GURL&)url
                           title:(NSString*)title {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (!self) {
    return nil;
  }

  _signinPresenter = signinPresenter;
  _url = url;
  _title = title;
  return self;
}

#pragma mark - ChromeCoordinator Methods

- (void)start {
  [self show];
}

// Do not call directly, use the hideSendTabToSelfUI() command instead!
- (void)stop {
  DCHECK(!self.stopped) << "Already stopped";
  self.stopped = YES;
  // Abort the waiting if it's still ongoing.
  _targetDeviceListWaiter.reset();
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
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) hideSendTabToSelfUI];
}

- (void)sendTabToTargetDeviceCacheGUID:(NSString*)cacheGUID
                      targetDeviceName:(NSString*)deviceName {
  SendTabToSelfSyncServiceFactory::GetForProfile(self.browser->GetProfile())
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

#pragma mark - Private

- (void)show {
  std::optional<send_tab_to_self::EntryPointDisplayReason> displayReason =
      [self displayReason];
  DCHECK(displayReason);

  switch (*displayReason) {
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature: {
      ProfileIOS* profile = self.browser->GetProfile();
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
              ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      DCHECK(account) << "The user must be signed in to share a tab";
      self.sendTabToSelfViewController =
          [[SendTabToSelfTableViewController alloc]
              initWithDeviceList:syncService->GetSendTabToSelfModel()
                                     ->GetTargetDeviceInfoSortedList()
                        delegate:self
                   accountAvatar:accountManagerService
                                     ->GetIdentityAvatarWithIdentity(
                                         account,
                                         IdentityAvatarSize::TableViewIcon)
                    accountEmail:account.userEmail];
      UINavigationController* navigationController =
          [[UINavigationController alloc]
              initWithRootViewController:self.sendTabToSelfViewController];

      navigationController.transitioningDelegate = self;
      navigationController.modalPresentationStyle = UIModalPresentationCustom;
      [self.baseViewController presentViewController:navigationController
                                            animated:YES
                                          completion:nil];
      break;
    }
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn: {
      __weak __typeof(self) weakSelf = self;
      ShowSigninCommandCompletionCallback callback =
          ^(SigninCoordinatorResult result,
            SigninCompletionInfo* completionInfo) {
            BOOL succeeded = result == SigninCoordinatorResultSuccess;
            [weakSelf onSigninComplete:succeeded];
          };
      ShowSigninCommand* command = [[ShowSigninCommand alloc]
          initWithOperation:AuthenticationOperation::kSigninOnly
                   identity:nil
                accessPoint:signin_metrics::AccessPoint::
                                ACCESS_POINT_SEND_TAB_TO_SELF_PROMO
                promoAction:signin_metrics::PromoAction::
                                PROMO_ACTION_NO_SIGNIN_PROMO
                   callback:callback];
      [self.signinPresenter showSignin:command];
      break;
    }
  }
}

- (void)onSigninComplete:(BOOL)succeeded {
  if (!succeeded) {
    [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                        BrowserCoordinatorCommands) hideSendTabToSelfUI];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _targetDeviceListWaiter = std::make_unique<TargetDeviceListWaiter>(
      SyncServiceFactory::GetForProfile(self.browser->GetProfile()),
      base::BindRepeating(
          [](__typeof(self) strongSelf) { return [strongSelf displayReason]; },
          weakSelf),
      base::BindOnce(
          [](__typeof(self) strongSelf) {
            [strongSelf onTargetDeviceListReady];
          },
          weakSelf));
}

- (void)onTargetDeviceListReady {
  _targetDeviceListWaiter.reset();
  [self show];
}

- (std::optional<send_tab_to_self::EntryPointDisplayReason>)displayReason {
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(
          self.browser->GetProfile());
  return service ? service->GetEntryPointDisplayReason(_url) : std::nullopt;
}

@end

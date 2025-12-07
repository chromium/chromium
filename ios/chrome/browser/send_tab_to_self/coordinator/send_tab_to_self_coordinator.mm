// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
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
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_send_tab.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/infobars/ui_bundled/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_mediator_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_table_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class TargetDeviceListWaiter : public syncer::SyncServiceObserver {
 public:
  using GetDisplayReasonCallback = base::RepeatingCallback<
      std::optional<send_tab_to_self::EntryPointDisplayReason>()>;

  // Queries `get_display_reason_callback` until it indicates the device list
  // is known (i.e. until it returns kOfferFeature or kInformNoTargetDevice),
  // then calls `on_list_known_callback`. Destroying the object aborts the
  // waiting.
  TargetDeviceListWaiter(
      syncer::SyncService* sync_service,
      const GetDisplayReasonCallback& get_display_reason_callback,
      base::OnceClosure on_list_known_callback)
      : get_display_reason_callback_(get_display_reason_callback),
        on_list_known_callback_(std::move(on_list_known_callback)) {
    sync_observation_.Observe(sync_service);
    OnStateChanged(sync_observation_.GetSource());
  }

  TargetDeviceListWaiter(const TargetDeviceListWaiter&) = delete;
  TargetDeviceListWaiter& operator=(const TargetDeviceListWaiter&) = delete;

  ~TargetDeviceListWaiter() override = default;

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
        sync_observation_.Reset();
        std::move(on_list_known_callback_).Run();
        break;
    }
  }

  void OnSyncShutdown(syncer::SyncService*) override {
    sync_observation_.Reset();
  }

 private:
  base::ScopedObservation<syncer::SyncService, TargetDeviceListWaiter>
      sync_observation_{this};
  const GetDisplayReasonCallback get_display_reason_callback_;
  base::OnceClosure on_list_known_callback_;
};

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

@interface SendTabToSelfCoordinator () <InfobarModalPositioner,
                                        SendTabToSelfMediatorDelegate,
                                        SendTabToSelfModalDelegate,
                                        UIViewControllerTransitioningDelegate> {
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

@implementation SendTabToSelfCoordinator {
  id<BrowserCoordinatorCommands> __weak _browserCoordinatorHandler;
  SigninCoordinator* _signinCoordinator;
  // The navigation controller displaying the send tab to self.
  UINavigationController* _navigationController;
  // The mediator of this coordinator.
  SendTabToSelfMediator* _mediator;
}

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
  _browserCoordinatorHandler = HandlerForProtocol(
      browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  return self;
}

#pragma mark - ChromeCoordinator Methods

- (void)start {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  if (!authService->SigninEnabled()) {
    // Sign-in was disabled after the list of action was opened. Let’s abort.
    // Don’t call anything after this, as `self` is not retained anymore.
    [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
    return;
  }
  _mediator = [[SendTabToSelfMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        self.profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        self.profile)];
  _mediator.delegate = self;
  [self show];
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
                      targetDeviceName:(NSString*)deviceName {
  SendTabToSelfSyncServiceFactory::GetForProfile(self.profile)
      ->GetSendTabToSelfModel()
      ->AddEntry(self.url, base::SysNSStringToUTF8(self.title),
                 base::SysNSStringToUTF8(cacheGUID));

  // ShowSendingMessage() opens UI, so wait for the dialog to be dismissed.
  __weak __typeof(self) weakSelf = self;
  self.dismissedCompletion = ^{
    [weakSelf showSnackbarMessageWithDeviceName:deviceName];
  };
  [self.delegate sendTabToSelfCoordinatorWantsToBeStopped:self];
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

// Stops the signin-coordiantor
- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

// Shows a snackbar message confirming that the tab was sent to `deviceName`.
- (void)showSnackbarMessageWithDeviceName:(NSString*)deviceName {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  if (!dispatcher) {
    return;
  }

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  NSString* text =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(deviceName));
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
  id<SnackbarCommands> handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [handler showSnackbarMessage:message];
}

// Closes the current tab in preparation for changing the profile.
- (void)prepareForChangeProfile {
  [_browserCoordinatorHandler closeCurrentTab];
}

// Shows the Send Tab To Self UI, either the device list or the sign-in promo.
- (void)show {
  std::optional<send_tab_to_self::EntryPointDisplayReason> displayReason =
      [self displayReason];
  DCHECK(displayReason);

  switch (*displayReason) {
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature: {
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
              ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      DCHECK(account) << "The user must be signed in to share a tab";
      self.sendTabToSelfViewController =
          [[SendTabToSelfTableViewController alloc]
              initWithDeviceList:syncService->GetSendTabToSelfModel()
                                     ->GetTargetDeviceInfoSortedList()
                        delegate:self
                   accountAvatar:GetApplicationContext()
                                     ->GetIdentityAvatarProvider()
                                     ->GetIdentityAvatar(
                                         account,
                                         IdentityAvatarSize::TableViewIcon)
                    accountEmail:account.userEmail];
      _navigationController = [[UINavigationController alloc]
          initWithRootViewController:self.sendTabToSelfViewController];

      _navigationController.transitioningDelegate = self;
      _navigationController.modalPresentationStyle = UIModalPresentationCustom;
      [self.baseViewController presentViewController:_navigationController
                                            animated:YES
                                          completion:nil];
      break;
    }
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn: {
      __weak __typeof(self) weakSelf = self;
      SigninCoordinatorCompletionCallback completion =
          ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
            id<SystemIdentity> completionIdentity) {
            BOOL succeeded = result == SigninCoordinatorResultSuccess;
            [weakSelf onSigninCompleteWithCoordinator:coordinator
                                            succeeded:succeeded];
          };
      ChangeProfileContinuationProvider provider = base::BindRepeating(
          &CreateChangeProfileSendTabToOtherDevice, _url, self.title);
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
                                             prepareChangeProfile:
                                                 prepareChangeProfile
                                             continuationProvider:provider];
      _signinCoordinator.signinCompletion = completion;
      [_signinCoordinator start];
      break;
    }
  }
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
  __weak __typeof(self) weakSelf = self;
  _targetDeviceListWaiter = std::make_unique<TargetDeviceListWaiter>(
      SyncServiceFactory::GetForProfile(self.profile),
      base::BindRepeating(
          [](__typeof(self) strongSelf) { return [strongSelf displayReason]; },
          weakSelf),
      base::BindOnce(
          [](__typeof(self) strongSelf) {
            [strongSelf onTargetDeviceListReady];
          },
          weakSelf));
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

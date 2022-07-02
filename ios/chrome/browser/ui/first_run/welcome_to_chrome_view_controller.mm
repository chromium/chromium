// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#include "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#include "ios/chrome/browser/ui/first_run/static_file_view_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/terms_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeToChromeViewController () <WelcomeToChromeViewDelegate>

// The animation which occurs at launch has run.
@property(nonatomic, assign) BOOL ranLaunchAnimation;

// The TOS link was tapped.
@property(nonatomic, assign) BOOL didTapTOSLink;

// Presenter for showing sync-related UI.
@property(nonatomic, readonly, weak) id<SyncPresenter> presenter;

@property(nonatomic, readonly, weak)
    id<ApplicationCommands, BrowsingDataCommands>
        dispatcher;

// The coordinator used to control sign-in UI flows.
@property(nonatomic, strong) SigninCoordinator* coordinator;

// Holds the state of the first run flow.
@property(nonatomic, strong) FirstRunConfiguration* firstRunConfig;

// Stores the interrupt completion block to be invoked once the first run is
// dismissed.
@property(nonatomic, copy) void (^interruptCompletion)(void);

// The browser of the interface that is presenting the Welcome to Chrome view.
@property(nonatomic, readonly) Browser* mainBrowser;

// The main browser that can be used for authentication.
@property(nonatomic, readonly) Browser* browser;

@end

@implementation WelcomeToChromeViewController

@synthesize didTapTOSLink = _didTapTOSLink;
@synthesize ranLaunchAnimation = _ranLaunchAnimation;
@synthesize presenter = _presenter;
@synthesize dispatcher = _dispatcher;

+ (BOOL)defaultStatsCheckboxValue {
  return kChromeFirstRunUIDidFinishNotification;
}

- (instancetype)initWithBrowser:(Browser*)browser
                    mainBrowser:(Browser*)mainBrowser
                      presenter:(id<SyncPresenter>)presenter
                     dispatcher:(id<ApplicationCommands, BrowsingDataCommands>)
                                    dispatcher {
  DCHECK(browser);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browser = browser;
    _mainBrowser = mainBrowser;
    _presenter = presenter;
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)interruptSigninCoordinatorWithCompletion:(void (^)(void))completion {
  // The first run can only be dismissed on the sign-in view.
  DCHECK(self.coordinator);
  // The sign-in coordinator is part of the navigation controller, so the
  // sign-in coordinator didn't present itself. Therefore the interrupt action
  // must be SigninCoordinatorInterruptActionNoDismiss.
  // `completion` has to be stored in order to be invoked when
  // `firstRunDismissedWithPresentingViewController:signinAction:` is
  // called.
  self.interruptCompletion = completion;
  [self.coordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:nil];
}

- (void)loadView {
  RecordMetricsReportingDefaultState();
  WelcomeToChromeView* welcomeToChromeView =
      [[WelcomeToChromeView alloc] initWithFrame:CGRectZero];
  [welcomeToChromeView setDelegate:self];
  [welcomeToChromeView
      setCheckBoxSelected:[[self class] defaultStatsCheckboxValue]];
  self.view = welcomeToChromeView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.navigationController setNavigationBarHidden:YES];
  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kStart);
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.ranLaunchAnimation)
    return;
  WelcomeToChromeView* view =
      base::mac::ObjCCastStrict<WelcomeToChromeView>(self.view);
  [view runLaunchAnimation];
  self.ranLaunchAnimation = YES;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (NSURL*)newTermsOfServiceUrl {
  std::string tos = GetTermsOfServicePath();
  NSString* path = [[base::mac::FrameworkBundle() bundlePath]
      stringByAppendingPathComponent:base::SysUTF8ToNSString(tos)];
  NSURLComponents* components = [[NSURLComponents alloc] init];
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  return [components URL];
}

// Displays the file at the given URL in a StaticFileViewController.
- (void)openStaticFileWithURL:(NSURL*)url title:(NSString*)title {
  StaticFileViewController* staticViewController =
      [[StaticFileViewController alloc]
          initWithBrowserState:self.mainBrowser->GetBrowserState()
                           URL:url];
  [staticViewController setTitle:title];
  [self.navigationController pushViewController:staticViewController
                                       animated:YES];
}

#pragma mark - WelcomeToChromeViewDelegate

- (void)welcomeToChromeViewDidTapTOSLink {
  self.didTapTOSLink = YES;
  NSString* title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_TERMS_TITLE);
  NSURL* tosUrl = [self newTermsOfServiceUrl];
  [self openStaticFileWithURL:tosUrl title:title];
}

- (void)welcomeToChromeViewDidTapOKButton:(WelcomeToChromeView*)view {
  PrefService* prefs = GetApplicationContext()->GetLocalState();

  prefs->SetBoolean(metrics::prefs::kMetricsReportingEnabled,
                    view.checkBoxSelected);

  // Sets a LocalState pref marking the TOS EULA as accepted.
  if (!prefs->GetBoolean(prefs::kEulaAccepted)) {
    prefs->SetBoolean(prefs::kEulaAccepted, true);
    prefs->CommitPendingWrite();
  }

  if (view.checkBoxSelected) {
    if (self.didTapTOSLink)
      base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }

  self.firstRunConfig = [[FirstRunConfiguration alloc] init];
  self.firstRunConfig.signInAttemptStatus =
      first_run::SignInAttemptStatus::NOT_ATTEMPTED;
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.mainBrowser->GetBrowserState());
  self.firstRunConfig.hasSSOAccount = accountManagerService->HasIdentities();

  syncer::SyncService* syncService = SyncServiceFactory::GetForBrowserState(
      self.mainBrowser->GetBrowserState());
  BOOL shouldSkipSignInFlow =
      syncService->GetDisableReasons().Has(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      !signin::IsSigninAllowedByPolicy();

  if (shouldSkipSignInFlow) {
    // Sign-in or sync is disabled by policy. Skip the sign-in flow.
    self.firstRunConfig.signInAttemptStatus =
        first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
    SigninCompletionInfo* completionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    [self completeFirstRunWithSigninCompletionInfo:completionInfo];
    return;
  }

  self.coordinator = [SigninCoordinator
      firstRunCoordinatorWithBaseNavigationController:self.navigationController
                                              browser:self.mainBrowser];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(markSigninAttempted:)
             name:kUserSigninAttemptedNotification
           object:self.coordinator];
  __weak WelcomeToChromeViewController* weakSelf = self;
  self.coordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf signinCompleteResult:signinResult
                        completionInfo:signinCompletionInfo];
      };

  [self.coordinator start];
  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kSyncScreenStart);
}

// Handles the sign-in completion and proceeds to complete the first run
// operation depending on the `signinResult` state.
- (void)signinCompleteResult:(SigninCoordinatorResult)signinResult
              completionInfo:(SigninCompletionInfo*)signinCompletionInfo {
  [self.coordinator stop];
  self.coordinator = nil;

  switch (signinResult) {
    case SigninCoordinatorResultCanceledByUser:
    case SigninCoordinatorResultInterrupted: {
      base::UmaHistogramEnumeration(
          "FirstRun.Stage", first_run::kSyncScreenCompletionWithoutSync);
      break;
    }
    case SigninCoordinatorResultSuccess: {
      base::UmaHistogramEnumeration("FirstRun.Stage",
                                    first_run::kSyncScreenCompletionWithSync);
      break;
    }
  }

  [self completeFirstRunWithSigninCompletionInfo:signinCompletionInfo];
}

// Completes the first run operation by either showing advanced settings
// sign-in, showing the location permission prompt, or simply dismissing the
// welcome page.
- (void)completeFirstRunWithSigninCompletionInfo:
    (SigninCompletionInfo*)completionInfo {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  FinishFirstRun(self.mainBrowser->GetBrowserState(), currentWebState,
                 self.firstRunConfig, self.presenter);

  __weak __typeof(self) weakSelf = self;
  UIViewController* presentingViewController =
      self.navigationController.presentingViewController;
  void (^completion)(void) = ^{
    [weakSelf
        firstRunDismissedWithPresentingViewController:presentingViewController
                                 signinCompletionInfo:completionInfo];
  };
  [presentingViewController dismissViewControllerAnimated:YES
                                               completion:completion];
}

// Triggers all the events after the first run is dismissed.
- (void)firstRunDismissedWithPresentingViewController:
            (UIViewController*)presentingViewController
                                 signinCompletionInfo:
                                     (SigninCompletionInfo*)completionInfo {
  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kComplete);
  FirstRunDismissed();
  switch (completionInfo.signinCompletionAction) {
    case SigninCompletionActionNone:
    case SigninCompletionActionShowManagedLearnMore:
      if (self.interruptCompletion) {
        self.interruptCompletion();
      }
      if (completionInfo.signinCompletionAction ==
          SigninCompletionActionShowManagedLearnMore) {
        OpenNewTabCommand* command = [OpenNewTabCommand
            commandWithURLFromChrome:GURL(kChromeUIManagementURL)];
        command.userInitiated = YES;
        [self.dispatcher openURLInNewTab:command];
      }
      break;
  }
}

#pragma mark - Notifications

// Marks the sign-in attempted field in first run config.
- (void)markSigninAttempted:(NSNotification*)notification {
  self.firstRunConfig.signInAttemptStatus =
      first_run::SignInAttemptStatus::ATTEMPTED;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kUserSigninAttemptedNotification
              object:self.coordinator];
}

@end

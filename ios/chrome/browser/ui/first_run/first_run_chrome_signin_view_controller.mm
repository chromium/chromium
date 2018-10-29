// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_chrome_signin_view_controller.h"

#import "base/ios/block_types.h"
#include "base/metrics/user_metrics.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/first_run/first_run_configuration.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/promos/signin_promo_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSignInButtonAccessibilityIdentifier =
    @"SignInButtonAccessibilityIdentifier";
NSString* const kSignInSkipButtonAccessibilityIdentifier =
    @"SkipButtonAccessibilityIdentifier";

@interface FirstRunChromeSigninViewController ()<
    ChromeSigninViewControllerDelegate> {
  __weak TabModel* _tabModel;
  FirstRunConfiguration* _firstRunConfig;
  __weak ChromeIdentity* _identity;
  BOOL _hasRecordedSigninStarted;
}

// Presenter for showing sync-related UI.
@property(nonatomic, readonly, weak) id<SyncPresenter> presenter;

@end

@implementation FirstRunChromeSigninViewController
@synthesize presenter = _presenter;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            tabModel:(TabModel*)tabModel
                      firstRunConfig:(FirstRunConfiguration*)firstRunConfig
                      signInIdentity:(ChromeIdentity*)identity
                           presenter:(id<SyncPresenter>)presenter
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super
       initWithBrowserState:browserState
                accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE
                promoAction:signin_metrics::PromoAction::
                                PROMO_ACTION_NO_SIGNIN_PROMO
             signInIdentity:identity
                 dispatcher:dispatcher];
  if (self) {
    _tabModel = tabModel;
    _firstRunConfig = firstRunConfig;
    _identity = identity;
    _presenter = presenter;
    self.delegate = self;
  }
  return self;
}

- (void)dealloc {
  self.delegate = nil;
  _tabModel = nil;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.navigationController setNavigationBarHidden:YES];
  self.primaryButton.accessibilityIdentifier =
      kSignInButtonAccessibilityIdentifier;
  self.secondaryButton.accessibilityIdentifier =
      kSignInSkipButtonAccessibilityIdentifier;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if (!_hasRecordedSigninStarted) {
    _hasRecordedSigninStarted = YES;
    signin_metrics::LogSigninAccessPointStarted(
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    signin_metrics::RecordSigninUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  }

  // Save the version number to prevent showing the SSO Recall promo on the next
  // cold start.
  [SigninPromoViewController recordVersionSeen];
}

- (BOOL)shouldAutorotate {
  return IsIPadIdiom() ? [super shouldAutorotate] : NO;
}

- (void)finishFirstRunAndDismissWithCompletion:(ProceduralBlock)completion {
  DCHECK(self.presentingViewController);
  FinishFirstRun(self.browserState, [_tabModel currentTab], _firstRunConfig,
                 self.presenter);
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:^{
                                                      FirstRunDismissed();
                                                      if (completion)
                                                        completion();
                                                    }];
}

#pragma mark ChromeSigninViewControllerDelegate

- (void)willStartSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  controller.shouldClearData = SHOULD_CLEAR_DATA_MERGE_DATA;
  [_firstRunConfig setSignInAttempted:YES];
}

- (void)willStartAddAccount:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  [_firstRunConfig setSignInAttempted:YES];
}

- (void)didSkipSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  // User is done with First Run after explicit skip.
  [self finishFirstRunAndDismissWithCompletion:nil];
}

- (void)didFailSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
}

- (void)didSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);

  // User is considered done with First Run only after successful sign-in.
  WriteFirstRunSentinelAndRecordMetrics(self.browserState, YES,
                                        [_firstRunConfig hasSSOAccount]);
}

- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity {
  DCHECK_EQ(self, controller);

  if ([_identity isEqual:identity]) {
    _identity = nil;
    // This is best effort. If the operation fails, the account will be left on
    // the device. The user will not be warned either as this call is
    // asynchronous (but undo is not), the application might be in an unknown
    // state when the forget identity operation finishes.
    ios::GetChromeBrowserProvider()->GetChromeIdentityService()->ForgetIdentity(
        identity, nil);
    [self.navigationController popViewControllerAnimated:YES];
  }
}

- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
    showAccountsSettings:(BOOL)showAccountsSettings {
  DCHECK_EQ(self, controller);

  // User is done with First Run after explicit sign-in accept.
  // Save a reference to the presentingViewController since this view controller
  // will be dismissed.
  __weak UIViewController* baseViewController = self.presentingViewController;
  __weak id<ApplicationCommands> weakDispatcher = self.dispatcher;
  [self finishFirstRunAndDismissWithCompletion:^{
    if (showAccountsSettings) {
      [weakDispatcher
          showAccountsSettingsFromViewController:baseViewController];
    }
  }];
}

#pragma mark ChromeSigninViewController

- (NSString*)skipSigninButtonTitle {
  return l10n_util::GetNSString(
      IDS_IOS_FIRSTRUN_ACCOUNT_CONSISTENCY_SKIP_BUTTON);
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"

#import "base/check.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ReauthenticationViewController {
  id<ReauthenticationProtocol> _reauthModule;
}

- (instancetype)initWithReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _reauthModule = reauthenticationModule;
    self.navigationItem.hidesBackButton = YES;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpTitle];
}

#pragma mark - Private

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  CHECK(self.delegate);

  [self requestAuthentication];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Wait until the view is in the hierarchy to present the alert, otherwise it
  // won't be shown.
  [self showSetUpPasscodeDialogIfNeeded];
}

#pragma mark - Private

// Forwards reauthentication result to the delegate.
- (void)handleReauthenticationResult:(ReauthenticationResult)result {
  // Reauth can't be skipped for this surface.
  CHECK(result != ReauthenticationResult::kSkipped);

  BOOL success = result == ReauthenticationResult::kSuccess;

  [self recordAuthenticationEvent:success ? ReauthenticationEvent::kSuccess
                                          : ReauthenticationEvent::kFailure];

  [self.delegate reauthenticationDidFinishWithSuccess:success];
}

// Sets a custom title view with the Password Manager logo.
- (void)setUpTitle {
  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  self.navigationItem.titleView =
      password_manager::CreatePasswordManagerTitleView(/*title=*/self.title);
}

// Requests the delegate to show passcode request alert if no Local
// Authentication is available.
- (void)showSetUpPasscodeDialogIfNeeded {
  if (![_reauthModule canAttemptReauth]) {
    [self recordAuthenticationEvent:ReauthenticationEvent::kMissingPasscode];
    [self.delegate showSetUpPasscodeDialog];
  }
}

// Triggers Local Authentication.
- (void)requestAuthentication {
  [self recordAuthenticationEvent:ReauthenticationEvent::kAttempt];

  if ([_reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    [_reauthModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW)
                    canReusePreviousAuth:NO
                                 handler:^(ReauthenticationResult result) {
                                   [weakSelf
                                       handleReauthenticationResult:result];
                                 }];
  }
}

// Records reauthentication event metrics.
- (void)recordAuthenticationEvent:(ReauthenticationEvent)event {
  UMA_HISTOGRAM_ENUMERATION(password_manager::kReauthenticationUIEventHistogram,
                            event);
}

@end

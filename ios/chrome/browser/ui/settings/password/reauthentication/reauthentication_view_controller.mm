// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"

#import "base/check.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ReauthenticationViewController {
  id<ReauthenticationProtocol> _reauthModule;
  BOOL _reauthUponPresentation;
}

- (instancetype)initWithReauthenticationModule:
                    (id<ReauthenticationProtocol>)reauthenticationModule
                        reauthUponPresentation:(BOOL)reauthUponPresentation {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _reauthModule = reauthenticationModule;
    _reauthUponPresentation = reauthUponPresentation;
    self.navigationItem.hidesBackButton = YES;

    // This view does not support large titles as it uses a custom title view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier =
      password_manager::kReauthenticationViewControllerAccessibilityIdentifier;

  // Set background color matching the one used in the settings UI.
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self setUpTitle];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Restore navigation bar background color to its default value.
  // The view controller under self in the stack could have changed it.
  self.navigationController.navigationBar.backgroundColor = nil;

  if (_reauthUponPresentation) {
    [self recordAuthenticationEvent:ReauthenticationEvent::kAttempt];
    if (@available(iOS 18, *)) {
      // TODO(crbug.com/347330366): Mock reauth will make
      // -triggerLocalAuthentication return immediately, which means a VC is
      // pushed and popped, following by pushing another VC. When doing this on
      // iOS18, the top accessory views are not visible. This is either unsafe
      // in UIKit and needs to be changed, or is a bug in iOS18, and may still
      // require a workaround. During this investigation, simply defer calling
      // -triggerLocalAuthentication to the next runloop on iOS18. This may be
      // unsafe and is a short-term only solution to greening a large number of
      // tests in one place.
      dispatch_async(dispatch_get_main_queue(), ^{
        [self triggerLocalAuthentication];
      });
    } else {
      [self triggerLocalAuthentication];
    }
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Wait until the view is in the hierarchy to present the alert, otherwise it
  // won't be shown.
  if (_reauthUponPresentation) {
    _reauthUponPresentation = NO;
    [self showSetUpPasscodeDialogIfNeeded];
  }
}

#pragma mark - ReauthenticationViewController

- (void)requestAuthentication {
  [self recordAuthenticationEvent:ReauthenticationEvent::kAttempt];

  if ([_reauthModule canAttemptReauth]) {
    [self triggerLocalAuthentication];
  } else {
    [self showSetUpPasscodeDialogIfNeeded];
  }
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
  if ([_reauthModule canAttemptReauth]) {
    return;
  }

  [self recordAuthenticationEvent:ReauthenticationEvent::kMissingPasscode];
  [self.delegate showSetUpPasscodeDialog];
}

// Records reauthentication event metrics.
- (void)recordAuthenticationEvent:(ReauthenticationEvent)event {
  UMA_HISTOGRAM_ENUMERATION(password_manager::kReauthenticationUIEventHistogram,
                            event);
}

// Starts the native UI for Local Authentication.
- (void)triggerLocalAuthentication {
  if (![_reauthModule canAttemptReauth]) {
    return;
  }

  // Hide keyboard otherwise the first responder can get focused after getting
  // the authentication result.
  [GetFirstResponder() resignFirstResponder];

  __weak __typeof(self) weakSelf = self;
  [_reauthModule
      attemptReauthWithLocalizedReason:
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW)
                  canReusePreviousAuth:NO
                               handler:^(ReauthenticationResult result) {
                                 [weakSelf handleReauthenticationResult:result];
                               }];
}

@end

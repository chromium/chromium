// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_consumer.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_presenter.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::SysUTF16ToNSString;
using password_manager::CreateDialogTraits;
using password_manager::CredentialLeakType;
using password_manager::LeakDialogTraits;
using password_manager::ShouldCheckPasswords;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogMetricsRecorder;
using password_manager::metrics_util::LeakDialogType;

@interface PasswordBreachMediator ()

// Credential leak type of the dialog.
@property(nonatomic, assign) CredentialLeakType credentialLeakType;

// Dismiss reason, used for metrics.
@property(nonatomic, assign) LeakDialogDismissalReason dismissReason;

// The presenter of the feature.
@property(nonatomic, weak) id<PasswordBreachPresenter> presenter;

@end

@implementation PasswordBreachMediator {
  // Metrics recorder for UMA and UKM
  std::unique_ptr<LeakDialogMetricsRecorder> recorder;

  // Service used to get the user's signed-in status.
  raw_ptr<AuthenticationService> _authenticationService;
}

- (instancetype)initWithConsumer:(id<PasswordBreachConsumer>)consumer
                       presenter:(id<PasswordBreachPresenter>)presenter
                metrics_recorder:
                    (std::unique_ptr<LeakDialogMetricsRecorder>)metrics_recorder
                        leakType:(CredentialLeakType)leakType
           authenticationService:(AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _presenter = presenter;
    _credentialLeakType = leakType;
    _dismissReason = LeakDialogDismissalReason::kNoDirectInteraction;
    _authenticationService = authenticationService;

    recorder = std::move(metrics_recorder);

    std::unique_ptr<LeakDialogTraits> traits = CreateDialogTraits(leakType);

    NSString* primaryActionString =
        traits->ShouldCheckPasswords()
            ? SysUTF16ToNSString(traits->GetAcceptButtonLabel())
            : l10n_util::GetNSString(IDS_IOS_PASSWORD_LEAK_CHANGE_CREDENTIALS);

    NSString* secondaryActionString = traits->ShouldCheckPasswords()
                                          ? l10n_util::GetNSString(IDS_NOT_NOW)
                                          : nil;

    [consumer setTitleString:SysUTF16ToNSString(traits->GetTitle())
               subtitleString:SysUTF16ToNSString(traits->GetDescription())
          primaryActionString:primaryActionString
        secondaryActionString:secondaryActionString];
  }
  return self;
}

- (void)disconnect {
  recorder->LogLeakDialogTypeAndDismissalReason(self.dismissReason);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (ShouldCheckPasswords(self.credentialLeakType)) {
    self.dismissReason = LeakDialogDismissalReason::kClickedCheckPasswords;

    // Opening the Password Checkup homepage or the Password Manager will stop
    // the presentation in the presenter. No need to send `stop`.
    if (_authenticationService->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      // The Password Checkup homepage is not made to be visited when signed
      // out.
      [self.presenter openPasswordCheckup];
    } else {
      [self.presenter openPasswordManager];
    }
  } else {
    [self dismissSheet];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self dismissSheet];
}

#pragma mark - Private

- (void)dismissSheet {
  self.dismissReason = LeakDialogDismissalReason::kClickedOk;
  [self.presenter stop];
}

@end

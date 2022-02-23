// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/passwords/password_breach_consumer.h"
#import "ios/chrome/browser/ui/passwords/password_breach_presenter.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysUTF16ToNSString;
using password_manager::CredentialLeakType;
using password_manager::GetAcceptButtonLabel;
using password_manager::GetCancelButtonLabel;
using password_manager::GetDescription;
using password_manager::GetLeakDialogType;
using password_manager::GetTitle;
using password_manager::GetPasswordCheckupURL;
using password_manager::ShouldCheckPasswords;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogType;
using password_manager::metrics_util::LogLeakDialogTypeAndDismissalReason;

@interface PasswordBreachMediator ()

// Leak type of the dialog.
@property(nonatomic, assign) LeakDialogType leakType;

// Credential leak type of the dialog.
@property(nonatomic, assign) CredentialLeakType credentialLeakType;

// Dismiss reason, used for metrics.
@property(nonatomic, assign) LeakDialogDismissalReason dismissReason;

// The presenter of the feature.
@property(nonatomic, weak) id<PasswordBreachPresenter> presenter;

@end

@implementation PasswordBreachMediator

- (instancetype)initWithConsumer:(id<PasswordBreachConsumer>)consumer
                       presenter:(id<PasswordBreachPresenter>)presenter
                        leakType:(CredentialLeakType)leakType {
  self = [super init];
  if (self) {
    _presenter = presenter;
    _credentialLeakType = leakType;
    _leakType = GetLeakDialogType(leakType);
    _dismissReason = LeakDialogDismissalReason::kNoDirectInteraction;

    if (base::FeatureList::IsEnabled(
            password_manager::features::
                kIOSEnablePasswordManagerBrandingUpdate)) {
      NSString* subtitle = SysUTF16ToNSString(GetDescription(leakType));
      NSString* primaryActionString =
          ShouldCheckPasswords(leakType)
              ? SysUTF16ToNSString(GetAcceptButtonLabel(leakType))
              : l10n_util::GetNSString(
                    IDS_IOS_PASSWORD_LEAK_CHANGE_CREDENTIALS);

      NSString* secondaryActionString =
          ShouldCheckPasswords(leakType) ? l10n_util::GetNSString(IDS_NOT_NOW)
                                         : nil;

      [consumer setTitleString:SysUTF16ToNSString(GetTitle(leakType))
                 subtitleString:subtitle
            primaryActionString:primaryActionString
          secondaryActionString:secondaryActionString];
    } else {
      NSString* subtitle = SysUTF16ToNSString(GetDescription(leakType));
      NSString* primaryActionString =
          ShouldCheckPasswords(leakType)
              ? SysUTF16ToNSString(GetAcceptButtonLabel(leakType))
              : nil;
      [consumer setTitleString:SysUTF16ToNSString(GetTitle(leakType))
                 subtitleString:subtitle
            primaryActionString:primaryActionString
          secondaryActionString:nil];
    }
  }
  return self;
}

- (void)disconnect {
  LogLeakDialogTypeAndDismissalReason(self.leakType, self.dismissReason);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  self.dismissReason = LeakDialogDismissalReason::kClickedOk;
  [self.presenter stop];
}

- (void)confirmationAlertPrimaryAction {
  if (ShouldCheckPasswords(self.credentialLeakType)) {
    self.dismissReason = LeakDialogDismissalReason::kClickedCheckPasswords;
    // Opening Password page will stop the presentation in the presenter.
    // No need to send |stop|.
    [self.presenter startPasswordCheck];
  } else {
    [self confirmationAlertDismissAction];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self confirmationAlertDismissAction];
}

- (void)confirmationAlertLearnMoreAction {
  [self.presenter presentLearnMore];
}

@end

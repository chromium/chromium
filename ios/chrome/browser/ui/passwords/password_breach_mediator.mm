// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/passwords/password_breach_consumer.h"

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

// Dismiss reason, used for metrics.
@property(nonatomic, assign) LeakDialogDismissalReason dismissReason;

// The presenter of the feature.
@property(nonatomic, weak) id<PasswordBreachPresenter> presenter;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

@implementation PasswordBreachMediator

- (instancetype)initWithConsumer:(id<PasswordBreachConsumer>)consumer
                       presenter:(id<PasswordBreachPresenter>)presenter
                      dispatcher:(id<ApplicationCommands>)dispatcher
                             URL:(const GURL&)URL
                        leakType:(CredentialLeakType)leakType {
  self = [super init];
  if (self) {
    _presenter = presenter;
    _dispatcher = dispatcher;
    _leakType = GetLeakDialogType(leakType);
    _dismissReason = LeakDialogDismissalReason::kNoDirectInteraction;

    NSString* subtitle = SysUTF16ToNSString(GetDescription(leakType, URL));
    NSString* primaryActionString =
        SysUTF16ToNSString(GetAcceptButtonLabel(leakType));
    [consumer setTitleString:SysUTF16ToNSString(GetTitle(leakType))
                subtitleString:subtitle
           primaryActionString:primaryActionString
        primaryActionAvailable:ShouldCheckPasswords(leakType)];
  }
  return self;
}

- (void)disconnect {
  LogLeakDialogTypeAndDismissalReason(self.leakType, self.dismissReason);
}

#pragma mark - PasswordBreachConsumerDelegate

- (void)passwordBreachDone {
  self.dismissReason = LeakDialogDismissalReason::kClickedOk;
  [self.presenter stop];
}

- (void)passwordBreachPrimaryAction {
  // Opening a new tab already stops the presentation in the presenter.
  // No need to send |stop|.
  self.dismissReason = LeakDialogDismissalReason::kClickedCheckPasswords;
  OpenNewTabCommand* newTabCommand =
      [OpenNewTabCommand commandWithURLFromChrome:GetPasswordCheckupURL()];
  [self.dispatcher openURLInNewTab:newTabCommand];
}

@end

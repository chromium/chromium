// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

@implementation ReauthenticationHandler {
  // Module containing the reauthentication mechanism used accessing passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
}

- (instancetype)initWithReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  DCHECK(reauthenticationModule);
  self = [super init];
  if (self) {
    _weakReauthenticationModule = reauthenticationModule;
  }
  return self;
}

- (void)verifyUserWithCompletionHandler:
            (void (^)(ReauthenticationResult))completionHandler
        presentReminderOnViewController:(UIViewController*)viewController {
  if ([_weakReauthenticationModule canAttemptReauth]) {
    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_SCREENLOCK_REASON",
                              @"Access Passwords...")
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self showSetPasscodeDialogOnViewController:viewController
                              completionHandler:completionHandler];
  }
}

- (void)showSetPasscodeDialogOnViewController:(UIViewController*)viewController
                            completionHandler:(void (^)(ReauthenticationResult))
                                                  completionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_SCREENLOCK_TITLE",
              @"Set A Passcode")
                       message:NSLocalizedString(
                                   @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_"
                                   @"SCREENLOCK_CONTENT",
                                   @"To use passwords, you must first set a "
                                   @"passcode on your device.")
                preferredStyle:UIAlertControllerStyleAlert];

  __weak UIResponder* opener = [self openerFromViewController:viewController];
  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:
          NSLocalizedString(
              @"IDS_IOS_CREDENTIAL_PROVIDER_SET_UP_SCREENLOCK_LEARN_HOW",
              @"Learn How")
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [self openAppWithURL:[NSURL
                                         URLWithString:base::SysUTF8ToNSString(
                                                           kPasscodeArticleURL)]
                              opener:opener];
                completionHandler(ReauthenticationResult::kFailure);
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_OK",
                                        @"OK")
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                completionHandler(ReauthenticationResult::kFailure);
              }];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [viewController presentViewController:alertController
                               animated:YES
                             completion:nil];
}

#pragma mark - Private

// Returns first responder up the chain that can open a URL.
- (UIResponder*)openerFromViewController:(UIViewController*)viewController {
  UIResponder* responder = viewController;
  while (responder) {
    if ([responder respondsToSelector:@selector(openURL:)]) {
      return responder;
    }
    responder = responder.nextResponder;
  }
  return nil;
}

// Open URL through app group commands.
- (void)openAppWithURL:(NSURL*)URL opener:(UIResponder*)opener {
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceCredentialsExtension
         URLOpenerBlock:^(NSURL* openURL) {
           if ([opener respondsToSelector:@selector(openURL:)]) {
             [opener performSelector:@selector(openURL:)
                          withObject:openURL
                          afterDelay:0];
           }
         }];

  [command prepareToOpenURL:URL];
  [command executeInApp];
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_view_test_app_interface.h"

#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"

@implementation SnackbarViewTestAppInterface

static UITextField* gDummyTextField = nil;

+ (id<SnackbarCommands>)snackbarCommandsHandler {
  CommandDispatcher* dispatcher = ChromeCoordinatorAppInterface.dispatcher;
  return HandlerForProtocol(dispatcher, SnackbarCommands);
}

+ (void)showSnackbarWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
            secondarySubtitle:(NSString*)secondarySubtitle
                   buttonText:(NSString*)buttonText
          hasLeadingAccessory:(BOOL)hasLeadingAccessory
         hasTrailingAccessory:(BOOL)hasTrailingAccessory {
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  message.subtitle = subtitle;
  message.secondarySubtitle = secondarySubtitle;

  if (buttonText) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.title = buttonText;
    action.handler = ^{
      // Do nothing.
    };

    message.action = action;
  }

  if (hasLeadingAccessory) {
    message.leadingAccessoryImage = [[UIImage alloc] init];
  }

  if (hasTrailingAccessory) {
    message.trailingAccessoryImage = [[UIImage alloc] init];
  }

  [[self snackbarCommandsHandler] showSnackbarMessage:message];
}

+ (void)makeTextFieldFirstResponder {
  if (!gDummyTextField) {
    gDummyTextField =
        [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 30)];
    // Add the text field to a view hierarchy to make it first responder.
    UIWindow* keyWindow = nil;

    for (UIScene* scene in [UIApplication sharedApplication].connectedScenes) {
      if (scene.activationState == UISceneActivationStateForegroundActive &&
          [scene isKindOfClass:[UIWindowScene class]]) {
        keyWindow = [(UIWindowScene*)scene keyWindow];
        break;
      }
    }

    [keyWindow addSubview:gDummyTextField];
    gDummyTextField.accessibilityIdentifier = @"dummyTextField";
  }
  [gDummyTextField becomeFirstResponder];
}

+ (void)removeDummyTextField {
  [gDummyTextField resignFirstResponder];
  [gDummyTextField removeFromSuperview];
  gDummyTextField = nil;
}

+ (UITextField*)dummyTextField {
  return gDummyTextField;
}

+ (void)showSnackbarMessageAfterDismissingKeyboardWithTitle:(NSString*)title {
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  [[self snackbarCommandsHandler]
      showSnackbarMessageAfterDismissingKeyboard:message];
}

@end

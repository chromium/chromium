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

  CommandDispatcher* dispatcher = ChromeCoordinatorAppInterface.dispatcher;
  id<SnackbarCommands> handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [handler showSnackbarMessage:message];
}

@end

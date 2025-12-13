// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_agent_delegate.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

@implementation AutofillAgentDelegate {
  __weak id<SnackbarCommands> _commandHandler;
}

- (instancetype)initWithCommandHandler:(id<SnackbarCommands>)commandHandler {
  if ((self = [super init])) {
    _commandHandler = commandHandler;
    DCHECK(_commandHandler);
  }
  return self;
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  DCHECK(_commandHandler);
  [_commandHandler showSnackbarWithMessage:messageText
                                buttonText:buttonText
                             messageAction:messageAction
                          completionAction:completionAction];
}

@end

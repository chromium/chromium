// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_agent_delegate.h"

#import "base/check.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

@implementation AutofillAgentDelegate {
  __weak id<SnackbarCommands> _snackbarHandler;
  __weak id<AtMemoryCommands> _atMemoryHandler;
}

- (instancetype)initWithSnackbarHandler:(id<SnackbarCommands>)snackbarHandler
                        atMemoryHandler:(id<AtMemoryCommands>)atMemoryHandler {
  if ((self = [super init])) {
    _snackbarHandler = snackbarHandler;
    _atMemoryHandler = atMemoryHandler;
    DCHECK(_snackbarHandler);
  }
  return self;
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  DCHECK(_snackbarHandler);
  [_snackbarHandler showSnackbarWithMessage:messageText
                                 buttonText:buttonText
                              messageAction:messageAction
                           completionAction:completionAction];
}

- (void)showAtMemory {
  [_atMemoryHandler showAtMemory];
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_snackbar_presenter.h"

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation ComposeboxSnackbarPresenter {
  raw_ptr<Browser> _browser;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)showAttachmentLimitSnackbar {
  [self showAttachmentLimitSnackbarWithBottomOffset:0];
}

- (void)showAttachmentLimitSnackbarWithBottomOffset:(CGFloat)bottomOffset {
  NSString* title = l10n_util::GetPluralNSStringF(
      IDS_IOS_COMPOSEBOX_MAXIMUM_ATTACHMENTS_REACHED, kAttachmentLimit);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];

  CommandDispatcher* dispatcher = _browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarHandler showSnackbarMessage:message bottomOffset:bottomOffset];
}

- (void)showUnableToAddAttachmentSnackbarWithBottomOffset:
    (CGFloat)bottomOffset {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_UNABLE_TO_ADD_ATTACHMENT);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];

  CommandDispatcher* dispatcher = _browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarHandler showSnackbarMessage:message bottomOffset:bottomOffset];
}

- (void)showCannotReloadTabError {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANNOT_RELOAD_TAB_ERROR);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];

  CommandDispatcher* dispatcher = _browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarHandler showSnackbarMessage:message bottomOffset:0];
}

@end

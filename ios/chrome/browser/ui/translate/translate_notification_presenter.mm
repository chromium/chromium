// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_notification_presenter.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/translate/translate_notification_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSnackbarActionAccessibilityIdentifier =
    @"SnackbarActionAccessibilityIdentifier";

NSString* const kTranslateNotificationSnackbarCategory =
    @"TranslateNotificationSnackbarCategory";

}  // namespace

@interface TranslateNotificationPresenter ()

// The dispatcher used by this Object.
@property(nonatomic, weak) id<SnackbarCommands> dispatcher;

@end

@implementation TranslateNotificationPresenter

- (instancetype)initWithDispatcher:(id<SnackbarCommands>)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - TranslateNotificationHandler

- (void)showTranslateNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                             notificationType:(TranslateNotificationType)type {
  NSString* text = nil;
  switch (type) {
    case TranslateNotificationTypeAlwaysTranslate:
    case TranslateNotificationTypeAutoAlwaysTranslate:
      text = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_NOTIFICATION_ALWAYS_TRANSLATE,
          base::SysNSStringToUTF16(delegate.sourceLanguage),
          base::SysNSStringToUTF16(delegate.targetLanguage)));
      break;
    case TranslateNotificationTypeNeverTranslate:
    case TranslateNotificationTypeAutoNeverTranslate:
      text = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_NOTIFICATION_LANGUAGE_NEVER,
          base::SysNSStringToUTF16(delegate.sourceLanguage)));
      break;
    case TranslateNotificationTypeNeverTranslateSite:
      text = l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_SITE_NEVER);
      break;
    case TranslateNotificationTypeError:
      text = l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_ERROR);
  }
  DCHECK(text);

  __weak id<TranslateNotificationDelegate> weakDelegate = delegate;
  __weak TranslateNotificationPresenter* weakSelf = self;
  __block BOOL action_block_executed = NO;
  auto action = ^() {
    action_block_executed = YES;
  };
  auto completion = ^(BOOL userInitiated) {
    // Inform the delegate of the dismissal or the user tapping "Undo".
    if (action_block_executed) {
      [weakDelegate translateNotificationHandlerDidUndo:weakSelf
                                       notificationType:type];
    } else {
      [weakDelegate translateNotificationHandlerDidDismiss:weakSelf
                                          notificationType:type];
    }
  };

  if (type == TranslateNotificationTypeError) {
    [self showSnackbarWithText:text];
  } else {
    [self showSnackbarWithText:text
                 actionHandler:action
             completionHandler:completion];
  }
}

- (void)dismissNotification {
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          kTranslateNotificationSnackbarCategory];
}

#pragma mark - Private

- (void)showSnackbarWithText:(NSString*)text {
  [self showSnackbarWithText:text actionHandler:nil completionHandler:nil];
}

- (void)showSnackbarWithText:(NSString*)text
               actionHandler:(void (^)())actionHandler
           completionHandler:(void (^)(BOOL))completionHandler {
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  if (actionHandler) {
    // A MDCSnackbarMessageAction is displayed as a button on the Snackbar. If
    // no action is set no button will appear on the Snackbar.
    MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
    action.title = l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_UNDO);
    action.accessibilityIdentifier = kSnackbarActionAccessibilityIdentifier;
    action.handler = actionHandler;
    message.action = action;
  }
  message.completionHandler = completionHandler;
  message.category = kTranslateNotificationSnackbarCategory;
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [self.dispatcher showSnackbarMessage:message];
}

@end

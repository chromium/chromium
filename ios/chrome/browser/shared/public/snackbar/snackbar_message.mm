// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"

namespace {

// Default snackbar visibility duration.
const NSTimeInterval kDefaultDuration = 4;

}  // namespace

@implementation SnackbarMessage

- (instancetype)initWithTitle:(NSString*)title {
  self = [super init];
  if (self) {
    _title = [title copy];
    _duration = kDefaultDuration;
  }
  return self;
}

- (instancetype)initWithMDCSnackbarMessage:(MDCSnackbarMessage*)message {
  self = [self initWithTitle:message.text];
  if (self) {
    self.duration = message.duration;
    self.completionHandler = message.completionHandler;
    if (message.action) {
      _action = [[SnackbarMessageAction alloc] init];
      _action.title = message.action.title;
      _action.handler = message.action.handler;
      _action.accessibilityLabel = message.action.accessibilityLabel;
      _action.accessibilityHint = message.action.accessibilityHint;
    }
  }
  return self;
}

@end

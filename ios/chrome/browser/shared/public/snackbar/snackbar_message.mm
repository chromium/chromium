// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"

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

@end

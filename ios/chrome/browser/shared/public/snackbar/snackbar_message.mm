// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"

#import "base/time/time.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"

@implementation SnackbarMessage

- (instancetype)initWithTitle:(NSString*)title {
  self = [super init];
  if (self) {
    _title = [title copy];
    _duration = tests_hook::GetSnackbarMessageDuration().InSeconds();
  }
  return self;
}

@end

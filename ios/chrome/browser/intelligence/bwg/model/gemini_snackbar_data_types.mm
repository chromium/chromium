// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_snackbar_data_types.h"

@implementation GeminiSnackbar

- (instancetype)initWithType:(GeminiSnackbarType)type
                     message:(NSString*)message
                 actionTitle:(NSString*)actionTitle
               actionHandler:(void (^)(void))actionHandler {
  self = [super init];
  if (self) {
    _type = type;
    _message = [message copy];
    _actionTitle = [actionTitle copy];
    _actionHandler = [actionHandler copy];
  }
  return self;
}

- (instancetype)initWithType:(GeminiSnackbarType)type
                     message:(NSString*)message {
  return [self initWithType:type
                    message:message
                actionTitle:nil
              actionHandler:nil];
}

@end

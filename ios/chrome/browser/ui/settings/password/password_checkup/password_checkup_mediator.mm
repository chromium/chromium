// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordCheckupMediator

- (void)setConsumer:(id<PasswordCheckupConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
}

@end

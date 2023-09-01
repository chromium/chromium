// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_mediator.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_consumer.h"

@interface PasswordPickerMediator () {
  std::vector<password_manager::CredentialUIEntry> _credentials;
}

@end

@implementation PasswordPickerMediator

- (instancetype)initWithCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  self = [super init];
  if (self) {
    _credentials = credentials;
  }
  return self;
}

- (void)setConsumer:(id<PasswordPickerConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setCredentials:_credentials];
}

@end

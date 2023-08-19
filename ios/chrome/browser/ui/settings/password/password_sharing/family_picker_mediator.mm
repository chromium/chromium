// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_mediator.h"

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"

@interface FamilyPickerMediator () {
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;
}

@end

@implementation FamilyPickerMediator

- (instancetype)initWithRecipients:
    (NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  self = [super init];
  if (self) {
    _recipients = recipients;
  }
  return self;
}

- (void)setConsumer:(id<FamilyPickerConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setRecipients:_recipients];
}

@end

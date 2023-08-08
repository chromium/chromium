// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"

@implementation RecipientInfo

- (instancetype)initWithRecipientInfo:
    (const password_manager::RecipientInfo&)recipient {
  self = [super init];
  if (self) {
    _fullName = base::SysUTF8ToNSString(recipient.user_name);
    _email = base::SysUTF8ToNSString(recipient.email);
  }
  return self;
}

@end

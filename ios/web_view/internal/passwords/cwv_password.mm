// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_internal.h"

#import <objc/runtime.h>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVPassword {
  autofill::PasswordForm _passwordForm;
}

- (instancetype)initWithPasswordForm:
    (const autofill::PasswordForm&)passwordForm {
  self = [super init];
  if (self) {
    _passwordForm = passwordForm;
    auto name_and_link =
        password_manager::GetShownOriginAndLinkUrl(_passwordForm);
    _title = base::SysUTF8ToNSString(name_and_link.first);
    _site = base::SysUTF8ToNSString(name_and_link.second.spec());
  }
  return self;
}

#pragma mark - Public

- (NSString*)username {
  if (self.blacklisted) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.username_value);
}

- (NSString*)password {
  if (self.blacklisted) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.password_value);
}

- (BOOL)isBlacklisted {
  return _passwordForm.blacklisted_by_user;
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

#pragma mark - Internal

- (autofill::PasswordForm*)internalPasswordForm {
  return &_passwordForm;
}

@end

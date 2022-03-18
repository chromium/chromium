// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issue_with_form.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/chrome/browser/net/crurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordIssueWithForm

@synthesize website = _website;
@synthesize username = _username;
@synthesize URL = _URL;

- (instancetype)initWithPasswordForm:(password_manager::PasswordForm)form {
  self = [super init];
  if (self) {
    _form = form;
    _website = base::SysUTF8ToNSString(
        password_manager::GetShownOriginAndLinkUrl(form).first);
    _username = base::SysUTF16ToNSString(form.username_value);
    _URL = [[CrURL alloc] initWithGURL:GURL(form.url)];
  }
  return self;
}

@end

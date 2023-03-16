// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/net/crurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordIssue

- (instancetype)initWithCredential:
    (password_manager::CredentialUIEntry)credential {
  self = [super init];
  if (self) {
    _credential = credential;
    _website =
        base::SysUTF8ToNSString(password_manager::GetShownOrigin(credential));
    _username = base::SysUTF16ToNSString(credential.username);
    _URL = [[CrURL alloc] initWithGURL:credential.GetURL()];
  }
  return self;
}

@end

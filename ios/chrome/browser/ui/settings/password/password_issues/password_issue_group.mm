// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue_group.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordIssueGroup

- (instancetype)initWithHeaderText:(NSString*)headerText
                    passwordIssues:(NSArray<PasswordIssue*>*)passwordIssues {
  DCHECK(passwordIssues.count);

  self = [super init];
  if (self) {
    _headerText = headerText;
    _passwordIssues = passwordIssues;
  }
  return self;
}

@end

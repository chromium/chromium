// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue_content_item.h"

@implementation PasswordIssueContentItem

- (void)setPassword:(PasswordIssue*)password {
  if (_password == password) {
    return;
  }
  _password = password;
  self.title = password.website;
  self.detailText = password.username;
  self.URL = password.URL;
  self.thirdRowText = password.compromisedDescription;
}

@end

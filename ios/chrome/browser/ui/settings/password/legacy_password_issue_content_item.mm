// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/legacy_password_issue_content_item.h"

#include "components/password_manager/core/common/password_manager_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LegacyPasswordIssueContentItem

- (void)setPassword:(PasswordIssue*)password {
  DCHECK(!base::FeatureList::IsEnabled(
      password_manager::features::kEnableFaviconForPasswords));
  if (_password == password)
    return;
  _password = password;
  self.text = password.website;
  self.detailText = password.username;
}

@end

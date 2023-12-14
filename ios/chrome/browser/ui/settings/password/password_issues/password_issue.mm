// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PasswordIssue () {
  // Whether the description for compromised credentials should be displayed.
  BOOL _compromisedDescriptionEnabled;
}
@end

@implementation PasswordIssue

- (instancetype)initWithCredential:
                    (password_manager::CredentialUIEntry)credential
      enableCompromisedDescription:(BOOL)enableCompromisedDescription {
  self = [super init];
  if (self) {
    _credential = credential;
    _website =
        base::SysUTF8ToNSString(password_manager::GetShownOrigin(credential));
    _username = base::SysUTF16ToNSString(credential.username);
    _URL = [[CrURL alloc] initWithGURL:credential.GetURL()];
    _compromisedDescriptionEnabled = enableCompromisedDescription;
    std::optional<GURL> changePasswordURL = credential.GetChangePasswordURL();
    if (changePasswordURL.has_value()) {
      _changePasswordURL =
          [[CrURL alloc] initWithGURL:changePasswordURL.value()];
    }
  }
  return self;
}

- (NSString*)compromisedDescription {
  if (_compromisedDescriptionEnabled) {
    if (_credential.IsLeaked()) {
      return _credential.IsPhished()
                 ? l10n_util::GetNSString(
                       IDS_IOS_COMPROMISED_PASSWORD_ISSUES_PHISHED_AND_LEAKED_DESCRIPTION)
                 : l10n_util::GetNSString(
                       IDS_IOS_COMPROMISED_PASSWORD_ISSUES_LEAKED_DESCRIPTION);
    }

    if (_credential.IsPhished()) {
      return l10n_util::GetNSString(
          IDS_IOS_COMPROMISED_PASSWORD_ISSUES_PHISHED_DESCRIPTION);
    }
  }

  return nil;
}

@end

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/chrome/browser/credential_provider/credential_provider_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;

}  // namespace

@implementation ArchivableCredential (PasswordForm)

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                validationIdentifier:(NSString*)validationIdentifier {
  if (passwordForm.url.is_empty() || passwordForm.blocked_by_user ||
      password_manager::IsValidAndroidFacetURI(passwordForm.signon_realm)) {
    return nil;
  }
  std::string site_name =
      password_manager::GetShownOrigin(url::Origin::Create(passwordForm.url));
  NSString* keychainIdentifier =
      SysUTF8ToNSString(passwordForm.encrypted_password);
  return [self initWithFavicon:favicon
            keychainIdentifier:keychainIdentifier
                          rank:passwordForm.times_used
              recordIdentifier:RecordIdentifierForPasswordForm(passwordForm)
             serviceIdentifier:SysUTF8ToNSString(passwordForm.url.spec())
                   serviceName:SysUTF8ToNSString(site_name)
                          user:SysUTF16ToNSString(passwordForm.username_value)
          validationIdentifier:validationIdentifier];
}

@end

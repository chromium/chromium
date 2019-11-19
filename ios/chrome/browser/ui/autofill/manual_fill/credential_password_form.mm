// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/credential_password_form.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManualFillCredential (PasswordForm)

- (instancetype)initWithPasswordForm:
    (const autofill::PasswordForm&)passwordForm {
  std::string host = passwordForm.origin.host();
  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  NSString* siteName = base::SysUTF8ToNSString(site_name);
  NSString* username = base::SysUTF16ToNSString(passwordForm.username_value);
  NSString* password = base::SysUTF16ToNSString(passwordForm.password_value);
  NSString* credentialHost = base::SysUTF8ToNSString(host);
  if ([credentialHost hasPrefix:@"www."] && credentialHost.length > 4) {
    credentialHost = [credentialHost substringFromIndex:4];
  }
  return [self initWithUsername:username
                       password:password
                       siteName:siteName.length ? siteName : credentialHost
                           host:credentialHost
                            URL:passwordForm.origin];
}

@end

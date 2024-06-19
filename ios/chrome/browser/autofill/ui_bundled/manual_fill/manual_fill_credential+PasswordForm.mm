// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential+PasswordForm.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "url/gurl.h"

@implementation ManualFillCredential (PasswordForm)

- (instancetype)initWithPasswordForm:
    (const password_manager::PasswordForm&)passwordForm {
  std::string host = passwordForm.url.host();
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
                            URL:passwordForm.url];
}

@end

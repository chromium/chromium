// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#import "base/strings/sys_string_conversions.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"

@implementation CWVPassword {
  password_manager::PasswordForm _passwordForm;
}

- (instancetype)initWithPasswordForm:
    (const password_manager::PasswordForm&)passwordForm {
  return [self initWithPasswordForm:passwordForm isAffiliationEnabled:false];
}

- (instancetype)initWithPasswordForm:
                    (const password_manager::PasswordForm&)passwordForm
                isAffiliationEnabled:(BOOL)isAffiliationEnabled {
  self = [super init];
  if (self) {
    _passwordForm = passwordForm;
    _title = base::SysUTF8ToNSString(password_manager::GetShownOrigin(
        password_manager::CredentialUIEntry(_passwordForm)));

    // If valid android password check the affiliation.
    if (isAffiliationEnabled &&
        affiliations::IsValidAndroidFacetURI(_passwordForm.signon_realm)) {
      std::string siteName = password_manager::GetShownOrigin(
          url::Origin::Create(passwordForm.url));

      NSString* serviceName = base::SysUTF8ToNSString(siteName);
      NSString* serviceIdentifier = @"";
      NSString* webRealm =
          base::SysUTF8ToNSString(passwordForm.affiliated_web_realm);
      url::Origin origin =
          url::Origin::Create(GURL(passwordForm.affiliated_web_realm));
      std::string shownOrigin = password_manager::GetShownOrigin(origin);

      // Set serviceIdentifier:
      if (webRealm.length) {
        // Prefer webRealm.
        serviceIdentifier = webRealm;
      } else if (!serviceIdentifier.length) {
        // Fallback to signon_realm.
        serviceIdentifier = base::SysUTF8ToNSString(passwordForm.signon_realm);
      }

      // Set serviceName:
      if (!shownOrigin.empty()) {
        // Prefer shownOrigin to match non Android credentials.
        serviceName = base::SysUTF8ToNSString(shownOrigin);
      } else if (!passwordForm.app_display_name.empty()) {
        serviceName = base::SysUTF8ToNSString(passwordForm.app_display_name);
      } else if (!serviceName.length) {
        // Fallback to serviceIdentifier.
        serviceName = serviceIdentifier;
      }

      _title = serviceName;
      _site = serviceIdentifier;

    } else {
      _site = base::SysUTF8ToNSString(
          password_manager::GetShownUrl(
              password_manager::CredentialUIEntry(_passwordForm))
              .spec());
    }
  }
  return self;
}

#pragma mark - Public

- (BOOL)isBlocked {
  return _passwordForm.blocked_by_user;
}

- (NSString*)username {
  if (self.blocked) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.username_value);
}

- (NSString*)password {
  if (self.blocked) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.password_value);
}

- (NSString*)keychainIdentifier {
  if (self.blocked) {
    return nil;
  }
  // On iOS, the LoginDatabase uses Keychain API to store passwords. The
  // "encrypted" version of the password is a unique ID (UUID) that is
  // stored as an attribute along with the password in the keychain.
  // See login_database_ios.cc for more info.
  return base::SysUTF8ToNSString(_passwordForm.keychain_identifier);
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

#pragma mark - Internal

- (password_manager::PasswordForm*)internalPasswordForm {
  return &_passwordForm;
}

@end

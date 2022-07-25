// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordDetails

- (instancetype)initWithCredential:
    (const password_manager::CredentialUIEntry&)credential {
  self = [super init];
  if (self) {
    auto facetUri = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        credential.signon_realm);
    if (facetUri.IsValidAndroidFacetURI()) {
      if (!credential.app_display_name.empty()) {
        _changePasswordURL = password_manager::CreateChangePasswordUrl(
            GURL(credential.affiliated_web_realm));
        _origin = base::SysUTF8ToNSString(credential.app_display_name);
        _website = base::SysUTF8ToNSString(credential.app_display_name);
      } else {
        _origin = base::SysUTF8ToNSString(facetUri.android_package_name());
        _website = base::SysUTF8ToNSString(facetUri.android_package_name());
      }
    } else {
      _origin =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(credential));
      _website = base::SysUTF8ToNSString(
          password_manager::GetShownUrl(credential).spec());
      _changePasswordURL =
          password_manager::CreateChangePasswordUrl(credential.url);
    }

    if (!credential.blocked_by_user) {
      _username = base::SysUTF16ToNSString(credential.username);
    }

    if (credential.federation_origin.opaque()) {
      _password = base::SysUTF16ToNSString(credential.password);
    } else {
      _federation =
          base::SysUTF8ToNSString(credential.federation_origin.host());
    }
  }
  return self;
}

@end

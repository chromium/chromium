// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#import "url/gurl.h"

namespace {

using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;

}  // namespace

password_manager::PasswordForm PasswordFormFromCredential(
    id<Credential> credential) {
  password_manager::PasswordForm form;

  GURL url(SysNSStringToUTF8(credential.serviceIdentifier));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = SysNSStringToUTF16(credential.username);
  form.password_value = SysNSStringToUTF16(credential.password);
  form.times_used_in_html_form = credential.rank;
  form.SetNoteWithEmptyUniqueDisplayName(SysNSStringToUTF16(credential.note));

  return form;
}

@implementation ArchivableCredential (PasswordForm)

- (instancetype)initWithPasswordForm:
                    (const password_manager::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                                gaia:(NSString*)gaia {
  if (passwordForm.blocked_by_user) {
    return nil;
  }
  std::string site_name =
      password_manager::GetShownOrigin(url::Origin::Create(passwordForm.url));

  NSString* serviceName = SysUTF8ToNSString(site_name);
  NSString* note =
      SysUTF16ToNSString(passwordForm.GetNoteWithEmptyUniqueDisplayName());

  NSString* serviceIdentifier = @"";
  if (affiliations::IsValidAndroidFacetURI(passwordForm.signon_realm)) {
    NSString* webRealm = SysUTF8ToNSString(passwordForm.affiliated_web_realm);
    url::Origin origin =
        url::Origin::Create(GURL(passwordForm.affiliated_web_realm));
    std::string shownOrigin = password_manager::GetShownOrigin(origin);

    // Set serviceIdentifier:
    if (webRealm.length) {
      // Prefer webRealm.
      serviceIdentifier = webRealm;
    } else if (!serviceIdentifier.length) {
      // Fallback to signon_realm.
      serviceIdentifier = SysUTF8ToNSString(passwordForm.signon_realm);
    }

    // Set serviceName:
    if (!shownOrigin.empty()) {
      // Prefer shownOrigin to match non Android credentials.
      serviceName = SysUTF8ToNSString(shownOrigin);
    } else if (!passwordForm.app_display_name.empty()) {
      serviceName = SysUTF8ToNSString(passwordForm.app_display_name);
    } else if (!serviceName.length) {
      // Fallback to serviceIdentifier.
      serviceName = serviceIdentifier;
    }
  } else {
    serviceIdentifier = SysUTF8ToNSString(passwordForm.url.spec());
  }

  DCHECK(serviceIdentifier.length);

  return [self initWithFavicon:favicon
                          gaia:gaia
                      password:SysUTF16ToNSString(passwordForm.password_value)
                          rank:passwordForm.times_used_in_html_form
              recordIdentifier:RecordIdentifierForPasswordForm(passwordForm)
             serviceIdentifier:serviceIdentifier
                   serviceName:serviceName
                      username:SysUTF16ToNSString(passwordForm.username_value)
                          note:note];
}

@end

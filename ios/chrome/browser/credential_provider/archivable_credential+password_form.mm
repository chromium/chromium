// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/credential_provider/credential_provider_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;

}  // namespace

password_manager::PasswordForm PasswordFormFromCredential(
    id<Credential> credential) {
  password_manager::PasswordForm form;

  GURL url(SysNSStringToUTF8(credential.serviceIdentifier));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = SysNSStringToUTF16(credential.user);
  form.encrypted_password = SysNSStringToUTF8(credential.keychainIdentifier);
  form.times_used_in_html_form = credential.rank;
  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    form.SetNoteWithEmptyUniqueDisplayName(SysNSStringToUTF16(credential.note));
  }

  return form;
}

@implementation ArchivableCredential (PasswordForm)

- (instancetype)initWithPasswordForm:
                    (const password_manager::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                validationIdentifier:(NSString*)validationIdentifier {
  if (passwordForm.blocked_by_user) {
    return nil;
  }
  std::string site_name =
      password_manager::GetShownOrigin(url::Origin::Create(passwordForm.url));
  NSString* keychainIdentifier =
      SysUTF8ToNSString(passwordForm.encrypted_password);

  NSString* serviceIdentifier = SysUTF8ToNSString(passwordForm.url.spec());
  NSString* serviceName = SysUTF8ToNSString(site_name);

  NSString* note = @"";
  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    note = SysUTF16ToNSString(passwordForm.GetNoteWithEmptyUniqueDisplayName());
  }

  if (password_manager::IsValidAndroidFacetURI(passwordForm.signon_realm)) {
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
  }

  DCHECK(serviceIdentifier.length);

  return [self initWithFavicon:favicon
            keychainIdentifier:keychainIdentifier
                          rank:passwordForm.times_used_in_html_form
              recordIdentifier:RecordIdentifierForPasswordForm(passwordForm)
             serviceIdentifier:serviceIdentifier
                   serviceName:serviceName
                          user:SysUTF16ToNSString(passwordForm.username_value)
          validationIdentifier:validationIdentifier
                          note:note];
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/credential_exporter.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/webauthn/model/credential_exchange_password.h"
#import "ios/chrome/browser/webauthn/model/credential_export_manager_swift.h"
#import "net/base/apple/url_conversions.h"

@implementation CredentialExporter {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Exports credentials through the OS ASCredentialExportManager API.
  CredentialExportManager* _credentialExportManager;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Secrets for passkey security domain needed for decryption.
  // TODO(crbug.com/444112223): Ensure this is in memory only for decryption.
  NSArray<NSData*>* _securityDomainSecrets;
}

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
         securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets {
  CHECK(window);
  CHECK(savedPasswordsPresenter);

  self = [super init];
  if (self) {
    _window = window;
    _credentialExportManager = [[CredentialExportManager alloc] init];
    _savedPasswordsPresenter = savedPasswordsPresenter;
    _securityDomainSecrets = securityDomainSecrets;
  }
  return self;
}

- (void)startExport API_AVAILABLE(ios(26.0)) {
  std::vector<password_manager::CredentialUIEntry> credentials =
      _savedPasswordsPresenter->GetSavedPasswords();

  NSMutableArray<CredentialExchangePassword*>* exportedPasswords =
      [NSMutableArray arrayWithCapacity:credentials.size()];

  for (const password_manager::CredentialUIEntry& credential : credentials) {
    NSString* username = base::SysUTF16ToNSString(credential.username) ?: @"";
    NSString* password = base::SysUTF16ToNSString(credential.password) ?: @"";
    NSString* note = base::SysUTF16ToNSString(credential.note) ?: @"";
    NSURL* URL =
        net::NSURLWithGURL(credential.GetURL()) ?: [NSURL URLWithString:@""];

    CredentialExchangePassword* exportedPassword =
        [[CredentialExchangePassword alloc] initWithURL:URL
                                               username:username
                                               password:password
                                                   note:note];
    [exportedPasswords addObject:exportedPassword];
  }

  [_credentialExportManager startExportWithCredentials:exportedPasswords
                                                window:_window];
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/credential_export_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/webauthn/model/credential_exporter.h"

@implementation CredentialExportMediator {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Responsible for interaction with the credential export OS libraries.
  CredentialExporter* _credentialExporter;
}

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter {
  self = [super init];
  if (self) {
    _window = window;
    _savedPasswordsPresenter = savedPasswordsPresenter;
  }
  return self;
}

#pragma mark - Public

// Called when the user confirms the export flow.
- (void)startExportWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets {
  _credentialExporter = [[CredentialExporter alloc]
               initWithWindow:_window
      savedPasswordsPresenter:_savedPasswordsPresenter
        securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets];
  [_credentialExporter startExport];
}

@end

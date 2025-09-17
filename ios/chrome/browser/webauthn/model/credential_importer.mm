// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/credential_importer.h"

#import "ios/chrome/browser/webauthn/model/credential_import_manager_swift.h"

@implementation CredentialImporter {
  // Imports credentials through the OS ASCredentialImportManager API.
  CredentialImportManager* _credentialImportManager;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _credentialImportManager = [[CredentialImportManager alloc] init];
  }
  return self;
}

- (void)startImport:(NSUUID*)UUID {
  if (@available(iOS 26, *)) {
    [_credentialImportManager startImport:UUID];
  }
}

@end

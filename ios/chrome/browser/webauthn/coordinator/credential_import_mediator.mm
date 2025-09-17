// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/credential_import_mediator.h"

#import "ios/chrome/browser/webauthn/model/credential_importer.h"

@implementation CredentialImportMediator {
  CredentialImporter* _credentialImporter;
}

- (instancetype)initWithUUID:(NSUUID*)UUID {
  self = [super init];
  if (self) {
    _credentialImporter = [[CredentialImporter alloc] init];
    [_credentialImporter startImport:UUID];
  }
  return self;
}

@end

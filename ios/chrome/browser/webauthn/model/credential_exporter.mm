// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/credential_exporter.h"

#import "base/check.h"
#import "ios/chrome/browser/webauthn/model/credential_export_manager_swift.h"

@implementation CredentialExporter {
  // Used as a presentation anchor for OS views. Must not be nil.
  UIWindow* _window;

  // Exports credentials through the OS ASCredentialExportManager API.
  CredentialExportManager* _credentialExportManager;
}

- (instancetype)initWithWindow:(UIWindow*)window {
  CHECK(window);

  self = [super init];
  if (self) {
    _window = window;
    _credentialExportManager = [[CredentialExportManager alloc] init];
  }
  return self;
}

- (void)startExport {
  [_credentialExportManager startExport:_window];
}

@end

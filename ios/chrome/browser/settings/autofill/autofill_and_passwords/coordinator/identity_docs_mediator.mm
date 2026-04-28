// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"

@implementation IdentityDocsMediator

- (void)disconnect {
  self.consumer = nil;
}

@end

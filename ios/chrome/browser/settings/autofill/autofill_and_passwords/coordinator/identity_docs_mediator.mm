// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"

namespace {

// Entity types go into the "Identity docs" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kIdentityDocs = {
    autofill::EntityTypeName::kDriversLicense,
    autofill::EntityTypeName::kNationalIdCard,
    autofill::EntityTypeName::kPassport};

}  // namespace

@implementation IdentityDocsMediator

- (void)setConsumer:(id<IdentityDocsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer) {
    // Trigger initial push.
    [self pushEntitiesToConsumer];
  }
}

- (void)disconnect {
  [super disconnect];
  _consumer = nil;
}

#pragma mark - AutofillAIBaseMediator

- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes {
  return kIdentityDocs;
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  [self.consumer setIdentityDocsItems:items];
}

@end

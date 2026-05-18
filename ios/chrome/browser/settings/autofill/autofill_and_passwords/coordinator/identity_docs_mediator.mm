// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"

namespace {

// Entity types go into the "Identity docs" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kIdentityDocs = {
    autofill::EntityTypeName::kDriversLicense,
    autofill::EntityTypeName::kNationalIdCard,
    autofill::EntityTypeName::kPassport};

}  // namespace

// Mediator implementation for Identity Docs.
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
  NSMutableArray<TableViewItem*>* driversLicenses = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* nationalIdCards = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* passports = [NSMutableArray array];

  for (TableViewItem* item in items) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCast<AutofillAIEntityItem>(item);
    if (!aiItem) {
      continue;
    }
    switch (aiItem.entityTypeName) {
      case autofill::EntityTypeName::kDriversLicense:
        [driversLicenses addObject:item];
        break;
      case autofill::EntityTypeName::kNationalIdCard:
        [nationalIdCards addObject:item];
        break;
      case autofill::EntityTypeName::kPassport:
        [passports addObject:item];
        break;
      case autofill::EntityTypeName::kFlightReservation:
      case autofill::EntityTypeName::kKnownTravelerNumber:
      case autofill::EntityTypeName::kRedressNumber:
      case autofill::EntityTypeName::kVehicle:
      case autofill::EntityTypeName::kOrder:
      case autofill::EntityTypeName::kShipment:
        NOTREACHED();
    }
  }

  [self.consumer setIdentityDocsWithDriversLicenses:driversLicenses
                                    nationalIdCards:nationalIdCards
                                          passports:passports];
}

@end

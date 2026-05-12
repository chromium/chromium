// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"

namespace {

// Entity types go into the "Travel Info" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kTravelInfo = {
    autofill::EntityTypeName::kKnownTravelerNumber,
    autofill::EntityTypeName::kRedressNumber,
    autofill::EntityTypeName::kVehicle,
    autofill::EntityTypeName::kFlightReservation};

}  // namespace

@implementation TravelInfoMediator

- (void)setConsumer:(id<TravelInfoConsumer>)consumer {
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
  return kTravelInfo;
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  [self.consumer setTravelInfoItems:items];
}

@end

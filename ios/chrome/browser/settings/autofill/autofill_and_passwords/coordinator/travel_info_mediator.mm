// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

namespace {

// Entity types go into the "Travel Info" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kTravelInfo = {
    autofill::EntityTypeName::kFlightReservation,
    autofill::EntityTypeName::kKnownTravelerNumber,
    autofill::EntityTypeName::kRedressNumber,
    autofill::EntityTypeName::kVehicle};

}  // namespace

@interface TravelInfoMediator () <BooleanObserver>
@end

// Mediator implementation for Travel Info.
@implementation TravelInfoMediator {
  PrefBackedBoolean* _travelInfoEnabled;
  PrefBackedBoolean* _autofillProfileEnabled;
}

- (instancetype)initWithEntityDataManager:
                    (autofill::EntityDataManager*)entityDataManager
                              prefService:(PrefService*)prefService {
  self = [super initWithEntityDataManager:entityDataManager
                              prefService:prefService];
  if (self) {
    if (prefService) {
      _travelInfoEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::
                                  kAutofillAiTravelEntitiesEnabled];
      _travelInfoEnabled.observer = self;
      _autofillProfileEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::kAutofillProfileEnabled];
      _autofillProfileEnabled.observer = self;
    }
  }
  return self;
}

- (void)setConsumer:(id<TravelInfoConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer) {
    // Trigger initial push.
    [self pushEntitiesToConsumer];

    [self updateConsumerToggleState];
  }
}

- (void)disconnect {
  [super disconnect];
  _travelInfoEnabled.observer = nil;
  [_travelInfoEnabled stop];
  _travelInfoEnabled = nil;
  _autofillProfileEnabled.observer = nil;
  [_autofillProfileEnabled stop];
  _autofillProfileEnabled = nil;
  _consumer = nil;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _travelInfoEnabled ||
      observableBoolean == _autofillProfileEnabled) {
    [self updateConsumerToggleState];
  }
}

#pragma mark - Protected

- (void)updateConsumerToggleState {
  if (!self.consumer) {
    return;
  }
  BOOL profileEnabled =
      _autofillProfileEnabled ? _autofillProfileEnabled.value : YES;
  BOOL travelInfoEnabled = _travelInfoEnabled ? _travelInfoEnabled.value : YES;
  BOOL managed = [self isAutofillAiDisabledByEnterprisePolicy];

  if (base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    [self.consumer setTravelInfoToggleState:travelInfoEnabled
                                    enabled:YES
                                    managed:managed];
  } else {
    [self.consumer setTravelInfoToggleState:travelInfoEnabled && profileEnabled
                                    enabled:profileEnabled
                                    managed:managed];
  }
}

#pragma mark - TravelInfoMutator

- (void)didToggleTravelInfo:(BOOL)enabled {
  _travelInfoEnabled.value = enabled;
}

#pragma mark - AutofillAIBaseMediator

- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes {
  return kTravelInfo;
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  NSMutableArray<TableViewItem*>* flightReservations = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* knownTravelerNumbers = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* redressNumbers = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* vehicles = [NSMutableArray array];

  for (TableViewItem* item in items) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCast<AutofillAIEntityItem>(item);
    if (!aiItem) {
      continue;
    }
    switch (aiItem.entityTypeName) {
      case autofill::EntityTypeName::kFlightReservation:
        [flightReservations addObject:item];
        break;
      case autofill::EntityTypeName::kKnownTravelerNumber:
        [knownTravelerNumbers addObject:item];
        break;
      case autofill::EntityTypeName::kRedressNumber:
        [redressNumbers addObject:item];
        break;
      case autofill::EntityTypeName::kVehicle:
        [vehicles addObject:item];
        break;
      case autofill::EntityTypeName::kDriversLicense:
      case autofill::EntityTypeName::kNationalIdCard:
      case autofill::EntityTypeName::kPassport:
      case autofill::EntityTypeName::kOrder:
      case autofill::EntityTypeName::kShipment:
        NOTREACHED();
    }
  }

  [self.consumer setTravelInfoWithFlightReservations:flightReservations
                                knownTravelerNumbers:knownTravelerNumbers
                                      redressNumbers:redressNumbers
                                            vehicles:vehicles];

  [self.consumer setWritableEntityTypes:[self writableEntityTypes]];
}

@end

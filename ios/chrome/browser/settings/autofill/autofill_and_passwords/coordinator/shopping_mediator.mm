// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

namespace {

// Entity types go into the "Shopping" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kShopping = {
    autofill::EntityTypeName::kOrder, autofill::EntityTypeName::kShipment};

}  // namespace

@interface ShoppingMediator () <BooleanObserver>
@end

// Mediator implementation for Shopping.
@implementation ShoppingMediator {
  PrefBackedBoolean* _shoppingEnabled;
  PrefBackedBoolean* _autofillProfileEnabled;
}

- (instancetype)initWithEntityDataManager:
                    (autofill::EntityDataManager*)entityDataManager
                              prefService:(PrefService*)prefService {
  self = [super initWithEntityDataManager:entityDataManager
                              prefService:prefService];
  if (self) {
    if (prefService) {
      _shoppingEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::
                                  kAutofillAiShoppingEntitiesEnabled];
      _shoppingEnabled.observer = self;
      _autofillProfileEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::kAutofillProfileEnabled];
      _autofillProfileEnabled.observer = self;
      self.personalContextEnabled.observer = self;
    }
  }
  return self;
}

- (void)setConsumer:(id<ShoppingConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer) {
    // Trigger initial push.
    [self pushEntitiesToConsumer];

    [self updateConsumerToggleState];

    [self updateSuggestionsFromGeminiForConsumer:_consumer];
  }
}

- (void)disconnect {
  self.personalContextEnabled.observer = nil;
  _shoppingEnabled.observer = nil;
  [_shoppingEnabled stop];
  _shoppingEnabled = nil;
  _autofillProfileEnabled.observer = nil;
  [_autofillProfileEnabled stop];
  _autofillProfileEnabled = nil;
  _consumer = nil;
  [super disconnect];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _shoppingEnabled ||
      observableBoolean == _autofillProfileEnabled) {
    [self updateConsumerToggleState];
  } else if (observableBoolean == self.personalContextEnabled) {
    [self updateSuggestionsFromGeminiForConsumer:_consumer];
  }
}

#pragma mark - Protected

- (void)updateConsumerToggleState {
  if (!self.consumer) {
    return;
  }
  BOOL profileEnabled =
      _autofillProfileEnabled ? _autofillProfileEnabled.value : YES;
  BOOL shoppingEnabled = _shoppingEnabled ? _shoppingEnabled.value : YES;
  BOOL managed = [self isAutofillAiDisabledByEnterprisePolicy];
  [self.consumer setShoppingToggleState:shoppingEnabled && profileEnabled
                                enabled:profileEnabled
                                managed:managed];
}

#pragma mark - ShoppingMutator

- (void)didToggleShopping:(BOOL)enabled {
  _shoppingEnabled.value = enabled;
}

#pragma mark - AutofillAIBaseMediator

- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes {
  return kShopping;
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  NSMutableArray<TableViewItem*>* orders = [NSMutableArray array];
  NSMutableArray<TableViewItem*>* shipments = [NSMutableArray array];

  for (TableViewItem* item in items) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCast<AutofillAIEntityItem>(item);
    if (!aiItem) {
      continue;
    }
    switch (aiItem.entityTypeName) {
      case autofill::EntityTypeName::kOrder:
        [orders addObject:item];
        break;
      case autofill::EntityTypeName::kShipment:
        [shipments addObject:item];
        break;
      case autofill::EntityTypeName::kFlightReservation:
      case autofill::EntityTypeName::kKnownTravelerNumber:
      case autofill::EntityTypeName::kRedressNumber:
      case autofill::EntityTypeName::kVehicle:
      case autofill::EntityTypeName::kDriversLicense:
      case autofill::EntityTypeName::kNationalIdCard:
      case autofill::EntityTypeName::kPassport:
        NOTREACHED();
    }
  }

  [self.consumer setShoppingWithOrders:orders shipments:shipments];
}

@end

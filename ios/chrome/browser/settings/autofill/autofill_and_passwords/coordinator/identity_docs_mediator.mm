// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

namespace {

// Entity types go into the "Identity docs" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kIdentityDocs = {
    autofill::EntityTypeName::kDriversLicense,
    autofill::EntityTypeName::kNationalIdCard,
    autofill::EntityTypeName::kPassport};

}  // namespace

@interface IdentityDocsMediator () <BooleanObserver>
@end

// Mediator implementation for Identity Docs.
@implementation IdentityDocsMediator {
  PrefBackedBoolean* _identityDocsEnabled;
  PrefBackedBoolean* _autofillProfileEnabled;
}

- (instancetype)initWithEntityDataManager:
                    (autofill::EntityDataManager*)entityDataManager
                              prefService:(PrefService*)prefService {
  self = [super initWithEntityDataManager:entityDataManager
                              prefService:prefService];
  if (self) {
    if (prefService) {
      _identityDocsEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::
                                  kAutofillAiIdentityEntitiesEnabled];
      _identityDocsEnabled.observer = self;
      _autofillProfileEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:autofill::prefs::kAutofillProfileEnabled];
      _autofillProfileEnabled.observer = self;
    }
  }
  return self;
}

- (void)setConsumer:(id<IdentityDocsConsumer>)consumer {
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
  _identityDocsEnabled.observer = nil;
  [_identityDocsEnabled stop];
  _identityDocsEnabled = nil;
  _autofillProfileEnabled.observer = nil;
  [_autofillProfileEnabled stop];
  _autofillProfileEnabled = nil;
  _consumer = nil;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _identityDocsEnabled ||
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
  BOOL identityDocsEnabled =
      _identityDocsEnabled ? _identityDocsEnabled.value : YES;
  BOOL managed = [self isAutofillAiDisabledByEnterprisePolicy];

  if (base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableAutofillSettingsEnterprisePolicy)) {
    [self.consumer setIdentityDocsToggleState:identityDocsEnabled
                                      enabled:YES
                                      managed:managed];
  } else {
    [self.consumer
        setIdentityDocsToggleState:identityDocsEnabled && profileEnabled
                           enabled:profileEnabled
                           managed:managed];
  }
}

#pragma mark - IdentityDocsMutator

- (void)didToggleIdentityDocs:(BOOL)enabled {
  _identityDocsEnabled.value = enabled;
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

  [self.consumer setWritableEntityTypes:[self writableEntityTypes]];
}

@end

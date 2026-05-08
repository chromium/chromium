// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Entity types go into the "Travel Info" section of Settings.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kTravelInfo = {
    autofill::EntityTypeName::kKnownTravelerNumber,
    autofill::EntityTypeName::kRedressNumber,
    autofill::EntityTypeName::kVehicle,
    autofill::EntityTypeName::kFlightReservation};

// Point size for AI entity icons.
const CGFloat kEntityIconPointSize = 20;

enum ItemType {
  ItemTypeTravelInfo = kItemTypeEnumZero,
};

}  // namespace

@interface TravelInfoMediator () <IOSAutofillEntityDataManagerObserver>
@end

@implementation TravelInfoMediator {
  // The data manager responsible for user entity instances.
  raw_ptr<autofill::EntityDataManager> _entityDataManager;

  // Bridge to observe changes to entity data from the manager.
  std::unique_ptr<autofill::IOSAutofillEntityDataManagerObserverBridge>
      _entityDataManagerObserver;
}

- (instancetype)initWithEntityDataManager:
    (autofill::EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    CHECK(entityDataManager);
    _entityDataManager = entityDataManager;
    _entityDataManagerObserver =
        std::make_unique<autofill::IOSAutofillEntityDataManagerObserverBridge>(
            _entityDataManager, self);
  }
  return self;
}

- (void)setConsumer:(id<TravelInfoConsumer>)consumer {
  _consumer = consumer;
  [self pushItemsToConsumer];
}

- (void)disconnect {
  _entityDataManagerObserver.reset();
  _entityDataManager = nullptr;
  _consumer = nil;
}

#pragma mark - TravelInfoMutator

- (void)didSelectTravelInfoItem:(TableViewItem*)item {
  AutofillAIEntityItem* aiEntityItem =
      base::apple::ObjCCast<AutofillAIEntityItem>(item);
  if (aiEntityItem) {
    [self.delegate travelInfoMediator:self
         didRequestToOpenEntityWithID:aiEntityItem.guid];
  }
}

#pragma mark - IOSAutofillEntityDataManagerObserver

- (void)onEntityInstancesChanged {
  [self pushItemsToConsumer];
}

#pragma mark - Private

// Fetches the relevant travel info entities, creates their corresponding
// items, and updates the consumer.
- (void)pushItemsToConsumer {
  CHECK(_entityDataManager);

  base::span<const autofill::EntityInstance> instances =
      _entityDataManager->GetEntityInstances();

  std::vector<const autofill::EntityInstance*> travelInfo;

  for (const auto& instance : instances) {
    if (kTravelInfo.contains(instance.type().name())) {
      travelInfo.push_back(&instance);
    }
  }

  NSMutableArray<TableViewItem*>* items =
      [[NSMutableArray alloc] initWithCapacity:travelInfo.size()];

  const std::string& locale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();

  std::vector<autofill::EntityLabel> labels = autofill::GetLabelsForEntities(
      travelInfo, /*attribute_types_to_ignore=*/{},
      /*only_disambiguating_types=*/true, /*obfuscate_sensitive_types=*/true,
      locale);

  for (size_t i = 0; i < travelInfo.size(); ++i) {
    TableViewItem* item = [self itemForEntityInstance:*travelInfo[i]
                                            withLabel:labels[i]
                                                 type:ItemTypeTravelInfo];
    [items addObject:item];
  }

  [self.consumer setTravelInfoItems:items];
}

// Creates and returns a `TableViewItem` for the given entity `instance` with
// the specified `label` and `type`.
- (TableViewItem*)itemForEntityInstance:
                      (const autofill::EntityInstance&)instance
                              withLabel:(const autofill::EntityLabel&)label
                                   type:(ItemType)type {
  AutofillAIEntityItem* item = [[AutofillAIEntityItem alloc] initWithType:type];
  item.name = base::SysUTF16ToNSString(
      base::JoinString(label, autofill::kLabelSeparator));
  item.typeDescription =
      base::SysUTF16ToNSString(instance.type().GetNameForI18n());
  item.guid = instance.guid();

  if (instance.record_type() ==
      autofill::EntityInstance::RecordType::kServerWallet) {
    item.isServerWalletItem = YES;
    item.trailingText = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_TEXT);
  }

  item.icon = autofill::DefaultIconForAutofillAiEntityType(
      instance.type().name(), kEntityIconPointSize, nil);
  return item;
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AutofillAIBaseMediator () <IOSAutofillEntityDataManagerObserver>
@end

@implementation AutofillAIBaseMediator {
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

- (void)disconnect {
  _entityDataManagerObserver.reset();
  _entityDataManager = nullptr;
}

#pragma mark - IOSAutofillEntityDataManagerObserver

- (void)onEntityInstancesChanged {
  [self pushEntitiesToConsumer];
}

#pragma mark - AutofillAIBaseMutator

- (void)didSelectEntityItem:(TableViewItem*)item {
  AutofillAIEntityItem* aiEntityItem =
      base::apple::ObjCCast<AutofillAIEntityItem>(item);
  if (aiEntityItem) {
    [self.delegate autofillAIBaseMediator:self
             didRequestToOpenEntityWithID:aiEntityItem.guid];
  }
}

#pragma mark - Public

+ (CGFloat)entityIconPointSize {
  return 20.0;
}

- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes {
  NOTREACHED();
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  NOTREACHED();
}

#pragma mark - Private

- (void)pushEntitiesToConsumer {
  if (!_entityDataManager) {
    return;
  }

  base::span<const autofill::EntityInstance> instances =
      _entityDataManager->GetEntityInstances();

  autofill::DenseSet<autofill::EntityTypeName> supportedTypes =
      [self supportedEntityTypes];

  std::vector<const autofill::EntityInstance*> filteredEntities;

  for (const auto& instance : instances) {
    if (supportedTypes.contains(instance.type().name())) {
      filteredEntities.push_back(&instance);
    }
  }

  NSMutableArray<TableViewItem*>* items =
      [[NSMutableArray alloc] initWithCapacity:filteredEntities.size()];

  const std::string& locale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();

  std::vector<autofill::EntityLabel> labels = autofill::GetLabelsForEntities(
      filteredEntities, /*attribute_types_to_ignore=*/{},
      /*only_disambiguating_types=*/true, /*obfuscate_sensitive_types=*/true,
      locale);

  for (size_t i = 0; i < filteredEntities.size(); ++i) {
    TableViewItem* item = [self itemForEntityInstance:*filteredEntities[i]
                                            withLabel:labels[i]];
    [items addObject:item];
  }

  [self pushItemsToConsumer:items];
}

- (TableViewItem*)itemForEntityInstance:
                      (const autofill::EntityInstance&)instance
                              withLabel:(const autofill::EntityLabel&)label {
  AutofillAIEntityItem* item =
      [[AutofillAIEntityItem alloc] initWithType:kItemTypeEnumZero];
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
      instance.type().name(), [[self class] entityIconPointSize], nil);
  return item;
}

@end

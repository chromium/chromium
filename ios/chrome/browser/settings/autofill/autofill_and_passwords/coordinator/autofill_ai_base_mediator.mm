// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator.h"

#import <algorithm>
#import <iterator>
#import <string>

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
#import "components/autofill/core/browser/integrators/autofill_ai/management_util.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator_protected.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/suggestions_from_gemini_entry_point_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AutofillAIBaseMediator () <IOSAutofillEntityDataManagerObserver,
                                      PrefObserverDelegate>
@end

@implementation AutofillAIBaseMediator {
  // The data manager responsible for user entity instances.
  raw_ptr<autofill::EntityDataManager> _entityDataManager;

  // Bridge to observe changes to entity data from the manager.
  std::unique_ptr<autofill::IOSAutofillEntityDataManagerObserverBridge>
      _entityDataManagerObserver;

  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;

  // Backing boolean to observe Suggestions from Gemini preference changes.
  PrefBackedBoolean* _personalContextEnabled;
}

- (instancetype)initWithEntityDataManager:
                    (autofill::EntityDataManager*)entityDataManager
                              prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(entityDataManager);
    _entityDataManager = entityDataManager;
    _entityDataManagerObserver =
        std::make_unique<autofill::IOSAutofillEntityDataManagerObserverBridge>(
            _entityDataManager, self);
    _prefService = prefService;
    if (prefService) {
      _prefChangeRegistrar.Init(prefService);
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      _prefObserverBridge->ObserveChangesForPreference(
          optimization_guide::prefs::
              kAutofillPredictionImprovementsEnterprisePolicyAllowed,
          &_prefChangeRegistrar);
      _personalContextEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:
                         personal_context::prefs::
                             kPersonalContextInAutofillSettingsToggleStatus];
    }
  }
  return self;
}

- (void)disconnect {
  _entityDataManagerObserver.reset();
  _entityDataManager = nullptr;
  [_personalContextEnabled stop];
  _personalContextEnabled = nil;
  if (_prefService) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
}

- (std::vector<autofill::EntityType>)writableEntityTypes {
  std::vector<autofill::EntityType> writableTypes;
  if (!_entityDataManager) {
    return writableTypes;
  }
  autofill::DenseSet<autofill::EntityType> allTypes =
      autofill::GetWritableEntityTypes(
          _entityDataManager->GetVariationCountryCode());

  autofill::DenseSet<autofill::EntityTypeName> supportedTypes =
      [self supportedEntityTypes];

  std::ranges::copy_if(allTypes, std::back_inserter(writableTypes),
                       [&supportedTypes](const autofill::EntityType& type) {
                         return supportedTypes.contains(type.name());
                       });
  return writableTypes;
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

- (void)didSelectAddEntityWithType:(autofill::EntityType)type {
  [self.delegate autofillAIBaseMediator:self
       didRequestToCreateEntityWithType:type];
}

- (void)didSelectDeleteEntityItems:(NSArray<TableViewItem*>*)items {
  if (!_entityDataManager) {
    return;
  }
  for (TableViewItem* item in items) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCast<AutofillAIEntityItem>(item);
    if (aiItem && !aiItem.isServerWalletItem) {
      // Entities with record type `kPersonalContext` are not displayed in the
      // settings, therefore, assigning `kLocal` here is okay.
      autofill::EntityInstance::RecordType recordType =
          autofill::EntityInstance::RecordType::kLocal;
      autofill::LogEntityDeletedFromSettings(
          autofill::EntityType(aiItem.entityTypeName), recordType);
      _entityDataManager->RemoveEntityInstance(aiItem.guid);
    }
  }
}

#pragma mark - Public

+ (CGFloat)entityIconPointSize {
  return 20.0;
}

#pragma mark - Protected

- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes {
  NOTREACHED();
}

- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items {
  NOTREACHED();
}

- (void)pushEntitiesToConsumer {
  if (!_entityDataManager) {
    return;
  }

  std::vector<autofill::EntityInstance> instances =
      autofill::GetEntityInstancesForSettings(
          _entityDataManager->GetEntityInstances());

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

- (void)updateConsumerToggleState {
  // Overridden by subclasses.
}

- (BOOL)isAutofillAiDisabledByEnterprisePolicy {
  return self.prefService &&
         autofill::IsAutofillAiDisabledByEnterprisePolicy(self.prefService);
}

- (id<ObservableBoolean>)personalContextEnabled {
  return _personalContextEnabled;
}

- (void)updateSuggestionsFromGeminiForConsumer:
    (id<SuggestionsFromGeminiEntryPointConsumer>)consumer {
  if (!consumer) {
    return;
  }

  BOOL enabled = _personalContextEnabled ? _personalContextEnabled.value : NO;
  [consumer
      setShouldShowSuggestionsFromGemini:self.shouldShowSuggestionsFromGemini
                                 enabled:enabled];
}

#pragma mark - Private

- (TableViewItem*)itemForEntityInstance:
                      (const autofill::EntityInstance&)instance
                              withLabel:(const autofill::EntityLabel&)label {
  AutofillAIEntityItem* item =
      [[AutofillAIEntityItem alloc] initWithType:kAutofillAIBaseItemTypeEntity];
  item.name = base::SysUTF16ToNSString(
      base::JoinString(label, autofill::kLabelSeparator));
  item.typeDescription =
      base::SysUTF16ToNSString(instance.type().GetNameForI18n());
  item.guid = instance.guid();
  item.entityTypeName = instance.type().name();

  if (instance.record_type() ==
      autofill::EntityInstance::RecordType::kServerWallet) {
    item.isServerWalletItem = YES;
    item.trailingText = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_TEXT);
  }

  const bool isPersonalContext =
      instance.record_type() ==
      autofill::EntityInstance::RecordType::kPersonalContext;

  item.icon = autofill::DefaultIconForAutofillAiEntityType(
      instance.type().name(), isPersonalContext,
      [[self class] entityIconPointSize], nil);
  return item;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName ==
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed) {
    [self updateConsumerToggleState];
  }
}

@end

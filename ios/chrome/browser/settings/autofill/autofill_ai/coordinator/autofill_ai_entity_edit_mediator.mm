// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"
#import "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#import "components/autofill/core/browser/proto/server.pb.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AttributeTypeName;
using autofill::EntityDataManager;
using autofill::EntityInstance;

namespace {

// Creates a date formatter for the given locale.
NSDateFormatter* CreateDateFormatterForLocale(const std::string& locale) {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  // The desired format is the following: "Jun 10, 2024".
  // This maps to NSDateFormatterMediumStyle.
  dateFormatter.dateStyle = NSDateFormatterMediumStyle;
  dateFormatter.timeStyle = NSDateFormatterNoStyle;
  dateFormatter.locale =
      [NSLocale localeWithLocaleIdentifier:base::SysUTF8ToNSString(locale)];
  return dateFormatter;
}

// Returns true if `attribute_type` is found in `required_fields`.
bool IsFieldRequired(const EntityInstance& entity_instance,
                     AttributeType attribute_type) {
  return std::ranges::any_of(entity_instance.type().required_fields(),
                             [&](const auto& required_set) {
                               return required_set.contains(attribute_type);
                             });
}

}  // namespace

@implementation AutofillAIEntityEditMediator {
  // The entity instance being viewed and edited.
  std::optional<EntityInstance> _entityInstance;

  // The entity data manager. It outlives the mediator.
  raw_ptr<EntityDataManager> _entityDataManager;

  // The Wallet pass manager. It outlives the mediator.
  raw_ptr<autofill::WalletPassAccessManager> _walletPassManager;

  // The locale used to get info from the entity instance.
  std::string _locale;

  // The date formatter used to display the date.
  NSDateFormatter* _dateFormatter;

  // Items to be displayed.
  NSMutableArray<TableViewItem*>* _editItems;

  // The fetched country list.
  NSArray<CountryItem*>* _allCountries;

  // The item factory.
  AutofillAIEntityEditItemFactory* _itemFactory;

  // The user's primary account email.
  NSString* _userEmail;
}

- (instancetype)initWithEntityInstance:(EntityInstance)entityInstance
                     entityDataManager:(EntityDataManager*)entityDataManager
                     walletPassManager:
                         (autofill::WalletPassAccessManager*)walletPassManager
                             userEmail:(NSString*)userEmail {
  self = [super init];
  if (self) {
    _entityInstance = std::move(entityInstance);
    _entityDataManager = entityDataManager;
    _walletPassManager = walletPassManager;
    _locale = GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _dateFormatter = CreateDateFormatterForLocale(_locale);
    _itemFactory =
        [[AutofillAIEntityEditItemFactory alloc] initWithLocale:_locale
                                                  dateFormatter:_dateFormatter];
    _userEmail = [userEmail copy];
  }
  return self;
}

// Sets the consumer of the mediator.
- (void)setConsumer:(id<AutofillAIEntityEditConsumer>)consumer {
  if (!consumer || !_entityInstance.has_value()) {
    return;
  }

  [consumer setTitle:base::SysUTF16ToNSString(
                         _entityInstance->type().GetNameForI18n())];

  [consumer setEditingAllowed:!_entityInstance->are_attributes_read_only()];
  [consumer setIsServerWalletItem:
                (_entityInstance->record_type() ==
                 autofill::EntityInstance::RecordType::kServerWallet)];
  [consumer setUserEmail:_userEmail];

  _editItems = [[NSMutableArray alloc] init];
  for (AttributeInstance attribute : _entityInstance->attributes()) {
    [_editItems addObject:[_itemFactory createItemForAttribute:attribute]];
  }

  [consumer setEditItems:_editItems];
  _consumer = consumer;
}

#pragma mark - AutofillAIEntityEditMutator

- (void)saveEntityInstance {
  CHECK(_entityInstance.has_value());

  base::flat_set<autofill::AttributeInstance,
                 autofill::AttributeInstance::CompareByType>
      updatedAttributes;

  for (TableViewItem* item in _editItems) {
    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
      autofill::AttributeType attrType(editItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           base::SysNSStringToUTF16(editItem.textFieldValue),
                           _locale, std::nullopt,
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
      autofill::AttributeType attrType(countryItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           base::SysNSStringToUTF16(countryItem.detailText),
                           _locale, std::nullopt,
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditDateItem>(item);
      autofill::AttributeType attrType(dateItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           AttributeValueFromNSDate(dateItem.dateValue),
                           _locale, GetAttributeFormatString(),
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    }
  }

  // If there are no attributes, we shouldn't save an empty entity.
  if (updatedAttributes.empty()) {
    return;
  }

  autofill::EntityInstanceBuilder builder(_entityInstance->type());
  builder.SetGUID(_entityInstance->guid())
      .SetNickname(_entityInstance->nickname())
      .SetDateModified(base::Time::Now())
      .SetUseCount(_entityInstance->use_count())
      .SetUseDate(_entityInstance->use_date())
      .SetRecordType(_entityInstance->record_type())
      .SetAreAttributesReadOnly(_entityInstance->are_attributes_read_only())
      .SetFrecencyOverride(std::string());

  for (const auto& attr : _entityInstance->attributes()) {
    builder.AddAttribute(attr);
  }
  for (const auto& attr : updatedAttributes) {
    builder.AddAttribute(attr);
  }

  _entityInstance = builder.Build();

  BOOL isSaveAsynchronous = autofill::IsMaskedStorageSupported(
      _entityInstance->type(), _entityInstance->record_type());
  if (isSaveAsynchronous && _walletPassManager) {
    [self.consumer showLoadingState];

    autofill::EntityInstance originalEntity = *_entityInstance;

    // TODO(crbug.com/496450943): Set appropriate sessionId.
    consent_auditor::ConsentAuditor::SessionId sessionId;
    __weak __typeof(self) weakSelf = self;
    auto callback = base::BindOnce(
        [](__typeof(self) weakSelf,
           autofill::EntityInstance fallbackOriginalEntity,
           std::optional<autofill::EntityInstance> savedEntity) {
          [weakSelf
              onSavePrivatePassToWalletFinished:std::move(savedEntity)
                                 originalEntity:std::move(
                                                    fallbackOriginalEntity)];
        },
        weakSelf, std::move(originalEntity));

    _walletPassManager->SaveWalletEntityInstance(*_entityInstance, sessionId,
                                                 std::move(callback));
  } else {
    // Standard local save.
    _entityDataManager->AddOrUpdateEntityInstance(*_entityInstance);
    [self.consumer didFinishSavingWithLocalFallback:NO];
  }
}

- (void)didChangeDate:(NSDate*)date
              forItem:(AutofillAIEntityEditDateItem*)item {
  item.dateValue = date;
  item.detailText = [_dateFormatter stringFromDate:date];
}

- (BOOL)isFieldRequired:(autofill::AttributeTypeName)attributeTypeName {
  return IsFieldRequired(*_entityInstance,
                         autofill::AttributeType(attributeTypeName));
}

#pragma mark - Public

- (NSArray<CountryItem*>*)allCountries {
  if (!_allCountries) {
    _allCountries = [AutofillProfileEditMediator loadCountries];
  }
  return _allCountries;
}

- (void)didSelectCountry:(CountryItem*)countryItem
                 forItem:(AutofillAIEntityCountryItem*)item {
  item.detailText = countryItem.text;
  [self.consumer updateItem:item];
}

- (GURL)walletManagementURL {
  CHECK(_entityInstance.has_value());
  return GURL(autofill::GetWalletManagementURL(*_entityInstance));
}

#pragma mark - Private

- (void)onSavePrivatePassToWalletFinished:
            (std::optional<autofill::EntityInstance>)savedEntity
                           originalEntity:
                               (autofill::EntityInstance)originalEntity {
  [self.consumer hideLoadingState];

  if (savedEntity.has_value()) {
    _entityDataManager->AddOrUpdateEntityInstance(std::move(*savedEntity));
    [self.consumer didFinishSavingWithLocalFallback:NO];
  } else {
    // Wallet save failed, fallback to Local.
    autofill::EntityInstance localEntity = originalEntity.CopyWithNewRecordType(
        autofill::EntityInstance::RecordType::kLocal);
    _entityDataManager->AddOrUpdateEntityInstance(std::move(localEntity));

    [self.consumer didFinishSavingWithLocalFallback:YES];
  }
}

@end

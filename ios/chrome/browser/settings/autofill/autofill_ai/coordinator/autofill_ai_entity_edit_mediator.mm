// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/proto/server.pb.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item_factory.h"
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

}  // namespace

@implementation AutofillAIEntityEditMediator {
  // The entity instance being viewed and edited.
  std::optional<EntityInstance> _entityInstance;

  // The entity data manager. It outlives the mediator.
  raw_ptr<EntityDataManager> _entityDataManager;

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
}

- (instancetype)initWithEntityInstance:(EntityInstance)entityInstance
                     entityDataManager:(EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    _entityInstance = std::move(entityInstance);
    _entityDataManager = entityDataManager;
    _locale = GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _dateFormatter = CreateDateFormatterForLocale(_locale);
    _itemFactory =
        [[AutofillAIEntityEditItemFactory alloc] initWithLocale:_locale
                                                  dateFormatter:_dateFormatter];
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
          base::apple::ObjCCast<AutofillAIEntityEditItem>(item);
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
          base::apple::ObjCCast<AutofillAIEntityCountryItem>(item);
      autofill::AttributeType attrType(countryItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           base::SysNSStringToUTF16(countryItem.detailText),
                           _locale, std::nullopt,
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
  _entityDataManager->AddOrUpdateEntityInstance(*_entityInstance);
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

@end

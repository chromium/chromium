// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/proto/server.pb.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AttributeTypeName;
using autofill::EntityDataManager;
using autofill::EntityInstance;

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAttribute = kItemTypeEnumZero,
};

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

// Creates a country item.
AutofillAIEntityCountryItem* CreateCountryItem(
    NSString* display_name,
    NSString* value,
    AttributeTypeName attr_type_name) {
  AutofillAIEntityCountryItem* countryItem =
      [[AutofillAIEntityCountryItem alloc] initWithType:ItemTypeAttribute];
  countryItem.text = display_name;
  countryItem.detailText = value;
  countryItem.attributeType = attr_type_name;
  countryItem.accessoryType = UITableViewCellAccessoryNone;
  countryItem.selectionStyle = UITableViewCellSelectionStyleNone;
  countryItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return countryItem;
}

// Creates an attribute item.
AutofillAIEntityEditItem* CreateAttributeItem(
    NSString* display_name,
    NSString* value,
    AttributeTypeName attr_type_name) {
  AutofillAIEntityEditItem* item =
      [[AutofillAIEntityEditItem alloc] initWithType:ItemTypeAttribute];
  item.fieldNameLabelText = display_name;
  item.textFieldPlaceholder = display_name;
  item.textFieldValue = value;
  item.attributeType = attr_type_name;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
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
}

- (instancetype)initWithEntityInstance:(EntityInstance)entityInstance
                     entityDataManager:(EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    _entityInstance = std::move(entityInstance);
    _entityDataManager = entityDataManager;
    _locale = GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _dateFormatter = CreateDateFormatterForLocale(_locale);
  }
  return self;
}

- (void)setConsumer:(id<AutofillAIEntityEditConsumer>)consumer {
  if (!consumer || !_entityInstance.has_value()) {
    return;
  }

  [consumer setTitle:base::SysUTF16ToNSString(
                         _entityInstance->type().GetNameForI18n())];

  [consumer setEditingAllowed:!_entityInstance->are_attributes_read_only()];

  NSMutableArray<TableViewItem*>* items = [[NSMutableArray alloc] init];
  for (AttributeInstance attribute : _entityInstance->attributes()) {
    const AttributeType attributeType = attribute.type();
    NSString* displayName =
        autofill::DisplayNameForAutofillAiAttributeType(attributeType);

    NSString* value = @"";
    if (attributeType.data_type() == AttributeType::DataType::kDate) {
      value = [_dateFormatter
          stringFromDate:NSDateFromAttributeInstance(attribute)];
    } else {
      value = base::SysUTF16ToNSString(attribute.GetInfo(
          attribute.type().field_type(), _locale, std::nullopt));
    }

    if (attributeType.data_type() == AttributeType::DataType::kCountry) {
      [items addObject:CreateCountryItem(displayName, value,
                                         attributeType.name())];
    } else {
      [items addObject:CreateAttributeItem(displayName, value,
                                           attributeType.name())];
    }
  }

  [consumer setEditItems:items];
  _consumer = consumer;
}

@end

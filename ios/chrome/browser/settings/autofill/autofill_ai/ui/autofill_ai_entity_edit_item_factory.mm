// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item_factory.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

using autofill::AttributeInstance;
using autofill::AttributeType;

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAttribute = kItemTypeEnumZero,
};
}  // namespace

@implementation AutofillAIEntityEditItemFactory {
  std::string _locale;
  NSDateFormatter* _dateFormatter;
}

- (instancetype)initWithLocale:(std::string)locale
                 dateFormatter:(NSDateFormatter*)dateFormatter {
  self = [super init];
  if (self) {
    _locale = std::move(locale);
    _dateFormatter = dateFormatter;
  }
  return self;
}

- (TableViewItem*)createItemForAttribute:(const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  if (attributeType.data_type() == AttributeType::DataType::kDate) {
    return [self createDateItemForAttribute:attribute];
  } else if (attributeType.data_type() == AttributeType::DataType::kCountry) {
    return [self createCountryItemForAttribute:attribute];
  } else {
    return [self createTextItemForAttribute:attribute];
  }
}

#pragma mark - Private

- (AutofillAIEntityCountryItem*)createCountryItemForAttribute:
    (const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  NSString* displayName =
      autofill::DisplayNameForAutofillAiAttributeType(attributeType);
  NSString* value = [self valueFromAttributeInstance:attribute];

  AutofillAIEntityCountryItem* countryItem =
      [[AutofillAIEntityCountryItem alloc] initWithType:ItemTypeAttribute];
  countryItem.text = displayName;
  countryItem.detailText = value;
  countryItem.attributeType = attributeType.name();
  countryItem.selectionStyle = UITableViewCellSelectionStyleNone;
  return countryItem;
}

- (AutofillAIEntityEditDateItem*)createDateItemForAttribute:
    (const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  NSString* displayName =
      autofill::DisplayNameForAutofillAiAttributeType(attributeType);
  NSDate* dateValue = NSDateFromAttributeInstance(attribute);
  NSString* value = [_dateFormatter stringFromDate:dateValue];

  AutofillAIEntityEditDateItem* item =
      [[AutofillAIEntityEditDateItem alloc] initWithType:ItemTypeAttribute];
  item.text = displayName;
  item.detailText = value;
  item.attributeType = attributeType.name();
  item.dateValue = dateValue;
  item.selectionStyle = UITableViewCellSelectionStyleNone;
  return item;
}

- (AutofillAIEntityEditItem*)createTextItemForAttribute:
    (const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  NSString* displayName =
      autofill::DisplayNameForAutofillAiAttributeType(attributeType);
  NSString* value = [self valueFromAttributeInstance:attribute];

  AutofillAIEntityEditItem* item =
      [[AutofillAIEntityEditItem alloc] initWithType:ItemTypeAttribute];
  item.fieldNameLabelText = displayName;
  item.textFieldPlaceholder = displayName;
  item.textFieldValue = value;
  item.attributeType = attributeType.name();
  item.selectionStyle = UITableViewCellSelectionStyleNone;
  item.customTextfieldAccessibilityIdentifier =
      [NSString stringWithFormat:@"%d", (int)attributeType.name()];
  return item;
}

// Extracts the standard string value for non-date attributes.
- (NSString*)valueFromAttributeInstance:(const AttributeInstance&)attribute {
  return base::SysUTF16ToNSString(
      attribute.GetInfo(attribute.type().field_type(), _locale, std::nullopt));
}
@end

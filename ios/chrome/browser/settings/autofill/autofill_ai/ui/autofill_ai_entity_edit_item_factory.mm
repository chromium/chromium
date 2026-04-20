// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item_factory.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/filling/field_filling_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::GetObfuscatedValue;

namespace {
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAttribute = kItemTypeEnumZero,
};
}  // namespace

@implementation AutofillAIEntityEditItemFactory {
  std::string _locale;
  NSDateFormatter* _dateFormatter;
  BOOL _userHasAuthenticated;
}

- (instancetype)initWithLocale:(std::string)locale
                 dateFormatter:(NSDateFormatter*)dateFormatter
          userHasAuthenticated:(BOOL)userHasAuthenticated {
  self = [super init];
  if (self) {
    _locale = std::move(locale);
    _dateFormatter = dateFormatter;
    _userHasAuthenticated = userHasAuthenticated;
  }
  return self;
}

- (void)setUserHasAuthenticated:(BOOL)userHasAuthenticated {
  _userHasAuthenticated = userHasAuthenticated;
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

// Returns true if the attribute should be obfuscated.
- (BOOL)shouldObfuscateAttribute:(const AttributeInstance&)attribute {
  return attribute.type().is_obfuscated() && !_userHasAuthenticated;
}

- (AutofillAIEntityCountryItem*)createCountryItemForAttribute:
    (const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  NSString* displayName =
      autofill::DisplayNameForAutofillAiAttributeType(attributeType);
  NSString* value = [self valueFromAttributeInstance:attribute];
  // If the value isn't a country ("_" is returned), prefer it to be an empty
  // string.
  if (value.length <= 1) {
    value = @"";
  }

  AutofillAIEntityCountryItem* countryItem =
      [[AutofillAIEntityCountryItem alloc] initWithType:ItemTypeAttribute];
  countryItem.text = displayName;
  countryItem.detailText = value;
  countryItem.attributeType = attributeType.name();
  countryItem.selectionStyle = UITableViewCellSelectionStyleNone;
  countryItem.hasValidValueStatus = YES;
  return countryItem;
}

- (AutofillAIEntityEditDateItem*)createDateItemForAttribute:
    (const AttributeInstance&)attribute {
  const AttributeType attributeType = attribute.type();
  NSString* displayName =
      autofill::DisplayNameForAutofillAiAttributeType(attributeType);
  NSDate* dateValue = NSDateFromAttributeInstance(attribute);
  NSString* value = [_dateFormatter stringFromDate:dateValue];
  if ([self shouldObfuscateAttribute:attribute]) {
    std::u16string value_u16 = base::SysNSStringToUTF16(value);
    value = base::SysUTF16ToNSString(
        GetObfuscatedValue(value_u16, /*visible_suffix_length=*/4));
  }

  AutofillAIEntityEditDateItem* item =
      [[AutofillAIEntityEditDateItem alloc] initWithType:ItemTypeAttribute];
  item.fieldNameLabelText = displayName;
  item.textFieldValue = value;
  item.attributeType = attributeType.name();
  item.dateValue = dateValue;
  item.selectionStyle = UITableViewCellSelectionStyleNone;
  item.hasValidValueStatus = YES;
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
  item.textFieldPlaceholder = @"";
  item.textFieldValue = value;
  item.attributeType = attributeType.name();
  item.selectionStyle = UITableViewCellSelectionStyleNone;
  item.customTextfieldAccessibilityIdentifier =
      base::SysUTF8ToNSString(attributeType.name_as_string());
  item.hasValidValueStatus = YES;
  return item;
}

// Extracts the standard string value for non-date attributes.
- (NSString*)valueFromAttributeInstance:(const AttributeInstance&)attribute {
  std::u16string value_u16 =
      attribute.GetInfo(attribute.type().field_type(), _locale, std::nullopt);
  if ([self shouldObfuscateAttribute:attribute] && !value_u16.empty()) {
    return base::SysUTF16ToNSString(
        GetObfuscatedValue(value_u16, /*visible_suffix_length=*/4));
  } else {
    return base::SysUTF16ToNSString(value_u16);
  }
}
@end

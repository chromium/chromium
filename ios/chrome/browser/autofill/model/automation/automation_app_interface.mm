// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/automation/automation_app_interface.h"

#import <map>
#import <string_view>

#import "base/containers/contains.h"
#import "base/json/json_reader.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/uuid.h"
#import "base/values.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

using autofill::PersonalDataManager;
using autofill::PersonalDataManagerFactory;

namespace {

// Converts a string (from the test recipe) to the autofill FieldType it
// represents.
autofill::FieldType FieldTypeFromString(std::string_view str, NSError** error) {
  static std::map<std::string_view, autofill::FieldType>
      string_to_field_type_map;

  // Only init the string to autofill field type map on the first call.
  // The test recipe can contain both server and html field types, as when
  // creating the recipe either type can be returned from predictions.
  // Therefore, store both in this map.
  if (string_to_field_type_map.empty()) {
    for (size_t i = autofill::NO_SERVER_DATA;
         i < autofill::MAX_VALID_FIELD_TYPE; ++i) {
      autofill::AutofillType autofill_type(static_cast<autofill::FieldType>(i));
      string_to_field_type_map[autofill_type.ToStringView()] =
          autofill_type.GetStorableType();
    }

    for (size_t i = static_cast<size_t>(autofill::HtmlFieldType::kUnspecified);
         i <= static_cast<size_t>(autofill::HtmlFieldType::kMaxValue); ++i) {
      autofill::AutofillType autofill_type(
          static_cast<autofill::HtmlFieldType>(i));
      string_to_field_type_map[autofill_type.ToStringView()] =
          autofill_type.GetStorableType();
    }
  }

  if (!base::Contains(string_to_field_type_map, str)) {
    NSString* error_description = [NSString
        stringWithFormat:@"Unable to recognize autofill field type %@!",
                         base::SysUTF8ToNSString(str)];
    *error = testing::NSErrorWithLocalizedDescription(error_description);
    return autofill::UNKNOWN_TYPE;
  }

  return string_to_field_type_map[str];
}

// Loads the defined autofill profile into the personal data manager, so that
// autofill actions will be suggested when tapping on an autofillable form.
// The autofill profile should be pulled from the test recipe, and consists of
// a list of dictionaries, each mapping one autofill type to one value, like so:
// "autofillProfile": [
//   { "type": "NAME_FIRST", "value": "Satsuki" },
//   { "type": "NAME_LAST", "value": "Yumizuka" },
//  ],
NSError* PrepareAutofillProfileWithValues(
    const base::Value::List* autofill_profile) {
  if (!autofill_profile) {
    return testing::NSErrorWithLocalizedDescription(
        @"Unable to find autofill profile in parsed JSON value.");
  }

  autofill::AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  autofill::CreditCard credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      "https://www.example.com/");

  // For each type-value dictionary in the autofill profile list, validate it,
  // then add it to the appropriate profile.
  for (const auto& profile_list_item : *autofill_profile) {
    const base::Value::Dict* entry = profile_list_item.GetIfDict();
    if (!entry) {
      return testing::NSErrorWithLocalizedDescription(
          @"Failed to extract an entry!");
    }

    const base::Value* type_container = entry->Find("type");
    if (!type_container->is_string()) {
      return testing::NSErrorWithLocalizedDescription(@"Type is not a string!");
    }
    const base::Value* value_container = entry->Find("value");
    if (!value_container->is_string()) {
      return testing::NSErrorWithLocalizedDescription(
          @"Value is not a string!");
    }

    const std::string field_type = type_container->GetString();
    NSError* error = nil;
    autofill::FieldType type = FieldTypeFromString(field_type, &error);
    if (error) {
      return error;
    }

    // TODO(crbug.com/40598404): Autofill profile and credit card info should be
    // loaded from separate fields in the recipe, instead of being grouped
    // together. However, need to make sure this change is also performed on
    // desktop automation.
    const std::string field_value = value_container->GetString();
    if (base::StartsWith(field_type, "HTML_TYPE_CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(field_type, "CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      credit_card.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    } else {
      profile.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    }
  }

  // Clear all existing local data and save the profile and credit card
  // generated to the personal data manager.
  ProfileIOS* profileIOS = chrome_test_util::GetOriginalProfile();
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profileIOS);
  for (const autofill::CreditCard* local_card :
       personal_data_manager->payments_data_manager().GetLocalCreditCards()) {
    personal_data_manager->RemoveByGUID(local_card->guid());
  }
  for (const autofill::AutofillProfile* local_profile :
       personal_data_manager->address_data_manager().GetProfilesByRecordType(
           autofill::AutofillProfile::RecordType::kLocalOrSyncable)) {
    personal_data_manager->RemoveByGUID(local_profile->guid());
  }
  personal_data_manager->payments_data_manager().AddCreditCard(credit_card);
  personal_data_manager->address_data_manager().AddProfile(profile);

  return nil;
}

}  // namespace

@implementation AutomationAppInterface

+ (NSError*)setAutofillAutomationProfile:(NSString*)profileJSON {
  std::optional<base::Value> readResult =
      base::JSONReader::Read(base::SysNSStringToUTF8(profileJSON));
  if (!readResult.has_value()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Unable to parse JSON string in app side.");
  }

  base::Value recipeRoot = std::move(readResult).value();

  const base::Value::List* autofillProfile =
      recipeRoot.GetDict().FindList("autofillProfile");
  return PrepareAutofillProfileWithValues(autofillProfile);
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/autofill_address_util.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/profile_requirement_utils.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "components/autofill/ios/common/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCountry = kItemTypeEnumZero,
};

// Field types that do not change with the country value.
constexpr std::array<autofill::FieldType, 5> kStaticFieldsTypes = {
    autofill::NAME_FULL, autofill::COMPANY_NAME, autofill::ADDRESS_HOME_COUNTRY,
    autofill::PHONE_HOME_WHOLE_NUMBER, autofill::EMAIL_ADDRESS};

}  // namespace

@interface AutofillProfileEditMediator ()

// Stores the address input fields.
@property(nonatomic, strong, readonly)
    NSArray<AutofillProfileAddressField*>* inputAddressFields;

@end

@implementation AutofillProfileEditMediator {
  raw_ptr<autofill::AutofillProfile> _autofillProfile;

  // Used for editing autofill profile.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // This property is for an interface which sends a response about saving the
  // edited profile.
  __weak id<AutofillProfileEditMediatorDelegate> _delegate;

  // The fetched country list.
  NSArray<CountryItem*>* _allCountries;

  // The country code that has been selected.
  NSString* _selectedCountryCode;

  // YES, when the mediator belongs to the migration prompt.
  BOOL _isMigrationPrompt;

  // If YES, denote that the particular field requires a value.
  BOOL _line1Required;
  BOOL _cityRequired;
  BOOL _stateRequired;
  BOOL _zipRequired;

  // Stores the required field names whose values are empty;
  NSMutableSet<NSString*>* _requiredFieldsWithEmptyValue;
}

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditMediatorDelegate>)delegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager
                 autofillProfile:(autofill::AutofillProfile*)autofillProfile
               isMigrationPrompt:(BOOL)isMigrationPrompt {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _autofillProfile = autofillProfile;
    _delegate = delegate;
    _isMigrationPrompt = isMigrationPrompt;
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];
    _selectedCountryCode =
        base::SysUTF8ToNSString(autofill::data_util::GetCountryCodeWithFallback(
            *autofillProfile, GetApplicationContext()->GetApplicationLocale()));

    [self loadCountries];
  }

  return self;
}

- (void)setConsumer:(id<AutofillProfileEditConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;

  [self fetchAndSetInputAddressFields];
  [self sendAutofillProfileDataToConsumer];
  [self fetchAndUpdateFieldRequirements];

  [_consumer setAccountProfile:[self isAccountProfile]];
}

#pragma mark - Public

- (void)didSelectCountry:(CountryItem*)countryItem {
  if ([_selectedCountryCode isEqualToString:countryItem.countryCode]) {
    return;
  }

  _selectedCountryCode = countryItem.countryCode;

  [self fetchAndSetInputAddressFields];
  [self fetchAndUpdateFieldRequirements];
  [self.consumer didSelectCountry:countryItem.text];
}

#pragma mark - AutofillSettingsProfileEditTableViewControllerDelegate

- (void)didEditAutofillProfileFromSettings {
  _personalDataManager->address_data_manager().UpdateProfile(*_autofillProfile);

  // Push the saved profile data to the consumer.
  [self sendAutofillProfileDataToConsumer];
}

- (BOOL)isMinimumAddress {
  return autofill::IsMinimumAddress(*_autofillProfile);
}

- (void)didTapMigrateToAccountButton {
  _personalDataManager->address_data_manager().MigrateProfileToAccount(
      *_autofillProfile);

  // Push the saved profile data to the consumer.
  [self sendAutofillProfileDataToConsumer];
}

#pragma mark - AutofillProfileEditTableViewControllerDelegate

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country {
  [_delegate willSelectCountryWithCurrentlySelectedCountry:country
                                               countryList:_allCountries];
}

- (void)didSaveProfileFromModal {
  [_delegate didSaveProfile];
}

- (void)updateProfileMetadataWithValue:(NSString*)value
                  forAutofillFieldType:(NSString*)autofillFieldType {
  autofill::FieldType serverFieldType =
      [self typeNameToFieldType:autofillFieldType];

  // Since the country field is a text field, we should use SetInfo() to
  // make sure they get converted to country codes.
  // Use SetInfo for fullname to propogate the change to the name_first,
  // name_middle and name_last subcomponents.
  if (serverFieldType == autofill::ADDRESS_HOME_COUNTRY ||
      serverFieldType == autofill::NAME_FULL) {
    _autofillProfile->SetInfoWithVerificationStatus(
        autofill::AutofillType(serverFieldType),
        base::SysNSStringToUTF16(value),
        GetApplicationContext()->GetApplicationLocale(),
        autofill::VerificationStatus::kUserVerified);
  } else {
    _autofillProfile->SetRawInfoWithVerificationStatus(
        serverFieldType, base::SysNSStringToUTF16(value),
        autofill::VerificationStatus::kUserVerified);
  }
}

- (BOOL)fieldContainsValidValue:(NSString*)autofillFieldType
                  hasEmptyValue:(BOOL)hasEmptyValue
      moveToAccountFromSettings:(BOOL)moveToAccountFromSettings {
  if (![self isAutofillFieldTypeRequiredField:autofillFieldType] ||
      [self requiredFieldWasEmptyOnProfileLoadForType:autofillFieldType
                            moveToAccountFromSettings:
                                moveToAccountFromSettings]) {
    // Early return if the text field is not a required field or contained an
    // empty value when the profile was loaded.
    return YES;
  }

  // If the required text field contains a value now, remove it from
  // `_requiredFieldsWithEmptyValue`.
  if ([_requiredFieldsWithEmptyValue containsObject:autofillFieldType] &&
      !hasEmptyValue) {
    [_requiredFieldsWithEmptyValue removeObject:autofillFieldType];
    return YES;
  }

  // If the required field is empty, add it to `_requiredFieldsWithEmptyValue`.
  if (hasEmptyValue) {
    [_requiredFieldsWithEmptyValue addObject:autofillFieldType];
    return NO;
  }

  return !hasEmptyValue;
}

- (void)viewDidDisappear {
  [_delegate autofillEditProfileMediatorDidFinish:self];
}

- (NSString*)fieldTypeToTypeName:(autofill::FieldType)autofillType {
  return base::SysUTF8ToNSString(autofill::FieldTypeToStringView(autofillType));
}

- (int)requiredFieldsWithEmptyValuesCount {
  return (int)[_requiredFieldsWithEmptyValue count];
}

- (void)resetRequiredFieldsWithEmptyValuesCount {
  [_requiredFieldsWithEmptyValue removeAllObjects];
}

#pragma mark - Private

// Returns true if the `autofillFieldType` belongs to a required field.
- (BOOL)isAutofillFieldTypeRequiredField:(NSString*)autofillFieldType {
  autofill::FieldType serverFieldType =
      [self typeNameToFieldType:autofillFieldType];
  return (serverFieldType == autofill::ADDRESS_HOME_LINE1 && _line1Required) ||
         (serverFieldType == autofill::ADDRESS_HOME_STREET_ADDRESS &&
          _line1Required) ||
         (serverFieldType == autofill::ADDRESS_HOME_CITY && _cityRequired) ||
         (serverFieldType == autofill::ADDRESS_HOME_STATE && _stateRequired) ||
         (serverFieldType == autofill::ADDRESS_HOME_ZIP && _zipRequired);
}

// Returns YES if the profile contained an empty value for the required
// `itemType`.
- (BOOL)requiredFieldWasEmptyOnProfileLoadForType:(NSString*)autofillFieldType
                        moveToAccountFromSettings:
                            (BOOL)moveToAccountFromSettings {
  if (moveToAccountFromSettings) {
    return NO;
  }
  autofill::FieldType serverFieldType =
      [self typeNameToFieldType:autofillFieldType];
  return _autofillProfile
      ->GetInfo(serverFieldType,
                GetApplicationContext()->GetApplicationLocale())
      .empty();
}

// Loads the country codes and names and sets the default selected country code.
- (void)loadCountries {
  autofill::CountryComboboxModel countryModel;
  countryModel.SetCountries(*_personalDataManager,
                            base::RepeatingCallback<bool(const std::string&)>(),
                            GetApplicationContext()->GetApplicationLocale());
  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      countryModel.countries();

  NSMutableArray<CountryItem*>* countryItems = [[NSMutableArray alloc]
      initWithCapacity:static_cast<NSUInteger>(countriesVector.size())];
  // Skip the first country as it appears twice in the
  // list. It was relevant to other platforms where the country does not have a
  // search option.
  for (size_t i = 1; i < countriesVector.size(); ++i) {
    if (countriesVector[i].get()) {
      if (([self isAccountProfile] || _isMigrationPrompt) &&
          !_personalDataManager->address_data_manager()
               .IsCountryEligibleForAccountStorage(
                   countriesVector[i]->country_code())) {
        continue;
      }
      CountryItem* countryItem =
          [[CountryItem alloc] initWithType:ItemTypeCountry];
      countryItem.text = base::SysUTF16ToNSString(countriesVector[i]->name());
      countryItem.countryCode =
          base::SysUTF8ToNSString(countriesVector[i]->country_code());
      countryItem.accessibilityIdentifier = countryItem.text;
      countryItem.accessibilityTraits |= UIAccessibilityTraitButton;
      [countryItems addObject:countryItem];
    }
  }
  _allCountries = countryItems;
}

// Fetches and computes the required fields based on `_selectedCountryCode`.
- (void)fetchAndUpdateFieldRequirements {
  for (CountryItem* countryItem in _allCountries) {
    if ([_selectedCountryCode isEqualToString:countryItem.countryCode]) {
      countryItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      countryItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  autofill::AutofillCountry country(
      base::SysNSStringToUTF8(_selectedCountryCode),
      GetApplicationContext()->GetApplicationLocale());
  _line1Required = country.requires_line1();
  _cityRequired = country.requires_city();
  _stateRequired = country.requires_state();
  _zipRequired = country.requires_zip();
}

// Fetches the address fields for input and sets them to inputAddressFields.
- (void)fetchAndSetInputAddressFields {
  NSMutableArray<AutofillProfileAddressField*>* addressFields =
      [[NSMutableArray alloc] init];

  if (base::FeatureList::IsEnabled(
          kAutofillDynamicallyLoadsFieldsForAddressInput)) {
    i18n::addressinput::Localization localization;
    localization.SetGetter(l10n_util::GetStringUTF8);
    std::string best_language_tag_unused;
    std::string country_code = base::SysNSStringToUTF8(_selectedCountryCode);
    autofill::AutofillCountry country(country_code);
    std::vector<autofill::AutofillAddressUIComponent> ui_components =
        ConvertAddressUiComponents(
            BuildComponents(country_code, localization,
                            GetApplicationContext()->GetApplicationLocale(),
                            &best_language_tag_unused),
            country);
    ExtendAddressComponents(ui_components, country, localization,
                            /*include_literals=*/false);
    for (const auto& item : ui_components) {
      if (GroupTypeOfFieldType(item.field) !=
          autofill::FieldTypeGroup::kAddress) {
        continue;
      }

      AutofillProfileAddressField* field =
          [[AutofillProfileAddressField alloc] init];
      field.fieldType = [self fieldTypeToTypeName:item.field];
      field.fieldLabel = base::SysUTF8ToNSString(item.name);

      [addressFields addObject:field];
    }
  } else {
    for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
      const AutofillProfileFieldDisplayInfo& fieldDisplayInfo =
          kProfileFieldsToDisplay[i];

      if (!FieldIsUsedInAddress(fieldDisplayInfo.autofillType,
                                _selectedCountryCode) ||
          GroupTypeOfFieldType(fieldDisplayInfo.autofillType) !=
              autofill::FieldTypeGroup::kAddress ||
          fieldDisplayInfo.autofillType == autofill::ADDRESS_HOME_COUNTRY) {
        // Country field is added separately in the VC.
        continue;
      }

      AutofillProfileAddressField* field =
          [[AutofillProfileAddressField alloc] init];
      field.fieldLabel =
          l10n_util::GetNSString(fieldDisplayInfo.displayStringID);
      field.fieldType =
          [self fieldTypeToTypeName:fieldDisplayInfo.autofillType];

      [addressFields addObject:field];
    }
  }

  _inputAddressFields = addressFields;
}

// Informs the consumer of the profile's data.
- (void)sendAutofillProfileDataToConsumer {
  int totalFieldCount =
      [self.inputAddressFields count] + kStaticFieldsTypes.size();
  NSMutableDictionary<NSString*, NSString*>* fieldValueMap =
      [[NSMutableDictionary alloc] initWithCapacity:totalFieldCount];
  for (AutofillProfileAddressField* field in self.inputAddressFields) {
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        [self typeNameToFieldType:field.fieldType],
        GetApplicationContext() -> GetApplicationLocale()));
    fieldValueMap[field.fieldType] = fieldValue;
  }

  for (const auto& field_type : kStaticFieldsTypes) {
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        field_type, GetApplicationContext()->GetApplicationLocale()));
    fieldValueMap[[self fieldTypeToTypeName:field_type]] = fieldValue;
  }

  [self.consumer setFieldValuesMap:fieldValueMap];
}

// Returns YES if `autofillProfile` is an account profile.
- (BOOL)isAccountProfile {
  return _autofillProfile->IsAccountProfile();
}

- (autofill::FieldType)typeNameToFieldType:(NSString*)autofillFieldType {
  return autofill::TypeNameToFieldType(
      base::SysNSStringToUTF8(autofillFieldType));
}

@end

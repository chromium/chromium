// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/profile_requirement_utils.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCountry = kItemTypeEnumZero,
};

}  // namespace

@implementation AutofillProfileEditMediator {
  raw_ptr<autofill::AutofillProfile> _autofillProfile;

  // Used for editing autofill profile.
  autofill::PersonalDataManager* _personalDataManager;

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
                     countryCode:(NSString*)countryCode
               isMigrationPrompt:(BOOL)isMigrationPrompt {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _autofillProfile = autofillProfile;
    _delegate = delegate;
    _selectedCountryCode = countryCode;
    _isMigrationPrompt = isMigrationPrompt;
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];

    [self loadCountries];
  }

  return self;
}

- (void)setConsumer:(id<AutofillProfileEditConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;

  [self sendAutofillProfileDataToConsumer];
  if (_selectedCountryCode) {
    [self updateRequirementsForCountryCode:_selectedCountryCode];
  } else {
    [self updateRequirementsForCountry:base::SysUTF16ToNSString(
                                           _autofillProfile->GetInfo(
                                               autofill::ADDRESS_HOME_COUNTRY,
                                               GetApplicationContext()
                                                   ->GetApplicationLocale()))];
  }

  [_consumer setAccountProfile:[self isAccountProfile]];
}

#pragma mark - Public

- (void)didSelectCountry:(CountryItem*)countryItem {
  if ([_selectedCountryCode isEqualToString:countryItem.countryCode]) {
    return;
  }

  [self updateRequirementsForCountryCode:countryItem.countryCode];
  [self.consumer didSelectCountry:countryItem.text];
}

#pragma mark - AutofillSettingsProfileEditTableViewControllerDelegate

- (void)didEditAutofillProfileFromSettings {
  _personalDataManager->UpdateProfile(*_autofillProfile);

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

  return hasEmptyValue;
}

- (void)viewDidDisappear {
  [_delegate autofillEditProfileMediatorDidFinish:self];
}

- (NSString*)selectedCountryCode {
  return _selectedCountryCode;
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

// Fetches and updates the required fields for the `countryCode`.
- (void)updateRequirementsForCountryCode:(NSString*)countryCode {
  _selectedCountryCode = countryCode;
  for (CountryItem* countryItem in _allCountries) {
    if ([_selectedCountryCode isEqualToString:countryItem.countryCode]) {
      countryItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      countryItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  [self setRequirementsForFields];
}

// Fetches and updates the required fields for the `country`.
- (void)updateRequirementsForCountry:(NSString*)country {
  for (CountryItem* countryItem in _allCountries) {
    if ([country isEqualToString:countryItem.text]) {
      _selectedCountryCode = countryItem.countryCode;
      countryItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      countryItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  [self setRequirementsForFields];
}

// Computes the required fields based on the country value
// `_selectedCountryCode`.
- (void)setRequirementsForFields {
  autofill::AutofillCountry country(
      base::SysNSStringToUTF8(_selectedCountryCode),
      GetApplicationContext()->GetApplicationLocale());
  _line1Required = country.requires_line1();
  _cityRequired = country.requires_city();
  _stateRequired = country.requires_state();
  _zipRequired = country.requires_zip();
}

// Informs the consumer of the profile's data.
- (void)sendAutofillProfileDataToConsumer {
  for (const AutofillProfileFieldDisplayInfo& field : kProfileFieldsToDisplay) {
    AutofillUIType autofillUIType =
        AutofillUITypeFromAutofillType(field.autofillType);
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        autofill::AutofillType(field.autofillType),
        GetApplicationContext()->GetApplicationLocale()));
    switch (autofillUIType) {
      case AutofillUITypeProfileCompanyName:
        [self.consumer setCompanyName:fieldValue];
        break;
      case AutofillUITypeProfileFullName:
        [self.consumer setFullName:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressLine1:
        [self.consumer setHomeAddressLine1:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressLine2:
        [self.consumer setHomeAddressLine2:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressDependentLocality:
        [self.consumer setHomeAddressDependentLocality:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressCity:
        [self.consumer setHomeAddressCity:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressAdminLevel2:
        [self.consumer setHomeAddressAdminLevel2:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressState:
        [self.consumer setHomeAddressState:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressZip:
        [self.consumer setHomeAddressZip:fieldValue];
        break;
      case AutofillUITypeProfileHomeAddressCountry:
        [self.consumer setHomeAddressCountry:fieldValue];
        break;
      case AutofillUITypeProfileHomePhoneWholeNumber:
        [self.consumer setHomePhoneWholeNumber:fieldValue];
        break;
      case AutofillUITypeProfileEmailAddress:
        [self.consumer setEmailAddress:fieldValue];
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

// Returns YES if `autofillProfile` is an account profile.
- (BOOL)isAccountProfile {
  return _autofillProfile->source() ==
         autofill::AutofillProfile::Source::kAccount;
}

- (autofill::FieldType)typeNameToFieldType:(NSString*)autofillFieldType {
  return autofill::TypeNameToFieldType(
      base::SysNSStringToUTF8(autofillFieldType));
}

@end

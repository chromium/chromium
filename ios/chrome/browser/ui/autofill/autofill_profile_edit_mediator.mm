// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator.h"

#import "base/strings/sys_string_conversions.h"
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

@interface AutofillProfileEditMediator ()

// Used for editing autofill profile.
@property(nonatomic, assign) autofill::PersonalDataManager* personalDataManager;

// This property is for an interface which sends a response about saving the
// edited profile.
@property(nonatomic, weak) id<AutofillProfileEditMediatorDelegate> delegate;

// The fetched country list.
@property(nonatomic, strong) NSArray<CountryItem*>* allCountries;

// The country code that has been selected.
@property(nonatomic, strong) NSString* selectedCountryCode;

// YES, when the mediator belongs to the migration prompt.
@property(nonatomic, assign, readonly) BOOL isMigrationPrompt;

// If YES, a migration button would be shown for the profile.
@property(nonatomic, assign, readonly) BOOL showMigrateToAccountButton;

@end

@implementation AutofillProfileEditMediator {
  autofill::AutofillProfile* _autofillProfile;
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
  if (self.selectedCountryCode) {
    [self updateRequirementsForCountryCode:self.selectedCountryCode];
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
  if ([self.selectedCountryCode isEqualToString:countryItem.countryCode]) {
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
  _personalDataManager->MigrateProfileToAccount(*_autofillProfile);

  // Push the saved profile data to the consumer.
  [self sendAutofillProfileDataToConsumer];
}

#pragma mark - AutofillProfileEditTableViewControllerDelegate

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country {
  [self.delegate
      willSelectCountryWithCurrentlySelectedCountry:country
                                        countryList:self.allCountries];
}

- (void)didSaveProfileFromModal {
  [self.delegate didSaveProfile];
}

- (BOOL)fieldValueEmptyOnProfileLoadForType:
    (autofill::ServerFieldType)serverFieldType {
  return _autofillProfile
      ->GetInfo(serverFieldType,
                GetApplicationContext()->GetApplicationLocale())
      .empty();
}

- (void)updateProfileMetadataWithValue:(NSString*)value
                     forAutofillUIType:(AutofillUIType)autofillUIType {
  autofill::ServerFieldType serverFieldType =
      AutofillTypeFromAutofillUIType(autofillUIType);

  // Since the country field is a text field, we should use SetInfo() to
  // make sure they get converted to country codes.
  // Use SetInfo for fullname to propogate the change to the name_first,
  // name_middle and name_last subcomponents.
  if (autofillUIType == AutofillUITypeProfileHomeAddressCountry ||
      autofillUIType == AutofillUITypeProfileFullName) {
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

- (void)viewDidDisappear {
  [self.delegate autofillEditProfileMediatorDidFinish:self];
}

#pragma mark - Private

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
      if (([self isAccountProfile] || self.isMigrationPrompt) &&
          !_personalDataManager->IsCountryEligibleForAccountStorage(
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
  self.allCountries = countryItems;
}

// Fetches and updates the required fields for the `countryCode`.
- (void)updateRequirementsForCountryCode:(NSString*)countryCode {
  self.selectedCountryCode = countryCode;
  for (CountryItem* countryItem in self.allCountries) {
    if ([self.selectedCountryCode isEqualToString:countryItem.countryCode]) {
      countryItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      countryItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  [self sendRequirementsToConsumer];
}

// Fetches and updates the required fields for the `country`.
- (void)updateRequirementsForCountry:(NSString*)country {
  for (CountryItem* countryItem in self.allCountries) {
    if ([country isEqualToString:countryItem.text]) {
      self.selectedCountryCode = countryItem.countryCode;
      countryItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      countryItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  [self sendRequirementsToConsumer];
}

// Informs the consumer about the required fields corresponding to the
// `self.selectedCountryCode`.
- (void)sendRequirementsToConsumer {
  autofill::AutofillCountry country(
      base::SysNSStringToUTF8(self.selectedCountryCode),
      GetApplicationContext()->GetApplicationLocale());
  [self.consumer setNameRequired:country.requires_full_name()];
  [self.consumer setLine1Required:country.requires_line1()];
  [self.consumer setCityRequired:country.requires_city()];
  [self.consumer setStateRequired:country.requires_state()];
  [self.consumer setZipRequired:country.requires_zip()];
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
      case AutofillUITypeProfileHonorificPrefix:
        [self.consumer setHonorificPrefix:fieldValue];
        break;
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
      case AutofillUITypeProfileHomeAddressCity:
        [self.consumer setHomeAddressCity:fieldValue];
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

@end

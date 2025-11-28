// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/country_type.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#import "components/autofill/core/browser/data_quality/autofill_data_util.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCountry = kItemTypeEnumZero,
};

// Field types that do not change with the country value.
constexpr std::array<autofill::FieldType, 3> kStaticFieldsTypes = {
    autofill::ADDRESS_HOME_COUNTRY, autofill::PHONE_HOME_WHOLE_NUMBER,
    autofill::EMAIL_ADDRESS};

}  // namespace

@interface AutofillProfileEditMediator ()

// Stores the non-address input fields.
@property(nonatomic, strong, readonly)
    NSArray<AutofillEditProfileField*>* inputNonAddressFields;

// Stores the address input fields.
@property(nonatomic, strong, readonly)
    NSArray<AutofillEditProfileField*>* inputAddressFields;

@end

@implementation AutofillProfileEditMediator {
  raw_ptr<autofill::AutofillProfile, DanglingUntriaged> _autofillProfile;

  // Used for editing autofill profile.
  raw_ptr<autofill::PersonalDataManager, DanglingUntriaged>
      _personalDataManager;

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

  // If YES, the new address is being added manually.
  BOOL _addManualAddress;

  // Indicates if error warnings should be ignored. Prevents displaying error
  // messages while adding a new address manually from settings, before the
  // user inputs data.
  BOOL _ignoreErrorMessage;

  // Stores the required field names whose values are empty;
  NSMutableSet<NSString*>* _requiredFieldsWithEmptyValue;

  // Stores the value displayed in the fields.
  NSMutableDictionary<NSString*, NSString*>* _currentValuesMap;

  // Stores the fields that were edited.
  NSMutableSet<NSString*>* _editedFields;

  // Yes, if the error section has been presented.
  BOOL _errorSectionPresented;
}

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditMediatorDelegate>)delegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager
                 autofillProfile:(autofill::AutofillProfile*)autofillProfile
               isMigrationPrompt:(BOOL)isMigrationPrompt
                addManualAddress:(BOOL)addManualAddress {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _autofillProfile = autofillProfile;
    _delegate = delegate;
    _isMigrationPrompt = isMigrationPrompt;
    _addManualAddress = addManualAddress;
    _requiredFieldsWithEmptyValue = [[NSMutableSet<NSString*> alloc] init];
    _selectedCountryCode =
        base::SysUTF8ToNSString(autofill::data_util::GetCountryCodeWithFallback(
            *autofillProfile,
            GetApplicationContext()->GetApplicationLocaleStorage()->Get()));
    _editedFields = [[NSMutableSet<NSString*> alloc] init];

    // Initially ignore the error warnings when adding an address manually
    // through settings.
    _ignoreErrorMessage = _addManualAddress;

    [self loadCountries];
  }

  return self;
}

- (void)setConsumer:(id<AutofillProfileEditConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;

  [self fetchAndSetFieldsForInput];
  [self populateCurrentValuesMap];
  [self fetchAndUpdateFieldRequirements];
  [self initializeRequiredEmptyFieldsForManualAddition];
  [_consumer setProfileRecordType:[self accountRecordType]];
}

#pragma mark - Public

- (void)didSelectCountry:(CountryItem*)countryItem {
  if ([_selectedCountryCode isEqualToString:countryItem.countryCode]) {
    return;
  }

  _selectedCountryCode = countryItem.countryCode;

  [self fetchAndSetFieldsForInput];
  [self fetchAndUpdateFieldRequirements];
  [self
      computeFieldWasEdited:base::SysUTF8ToNSString(autofill::FieldTypeToString(
                                autofill::ADDRESS_HOME_COUNTRY))
                      value:countryItem.text];
  [self.consumer didSelectCountry:countryItem.text];
}

- (BOOL)canDismissImmediately {
  return !_errorSectionPresented && ![_editedFields count];
}

- (BOOL)shouldShowConfirmationDialogOnDismissBySwiping {
  return !_errorSectionPresented && [_editedFields count] > 0;
}

- (void)saveChangesForDismiss {
  [self didSaveProfileFromModal];
}

#pragma mark - AutofillSettingsProfileEditTableViewControllerDelegate

- (void)didEditAutofillProfileFromSettings {
  _personalDataManager->address_data_manager().UpdateProfile(*_autofillProfile);

  // Populate the current values map so that the newly saved values are
  // displayed.
  [self populateCurrentValuesMap];
}

- (BOOL)isMinimumAddress {
  return autofill::IsMinimumAddress(*_autofillProfile);
}

- (autofill::AutofillProfile::RecordType)accountRecordType {
  return _autofillProfile->record_type();
}

- (void)didTapMigrateToAccountButton {
  _personalDataManager->address_data_manager().MigrateProfileToAccount(
      *_autofillProfile);

  // Populate the current values map so that the newly saved values are
  // displayed.
  [self populateCurrentValuesMap];
}

#pragma mark - AutofillProfileEditTableViewHelperDelegate

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country {
  [_delegate willSelectCountryWithCurrentlySelectedCountry:country
                                               countryList:_allCountries];
}

- (void)didSaveProfileFromModal {
  if (_errorSectionPresented) {
    return;
  }

  [_consumer updateProfileData];
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
        GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
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
  BOOL isRequired = [self isAutofillFieldTypeRequiredField:autofillFieldType];

  // Only required fields need further checks. If not required, it's considered
  // valid.
  if (!isRequired) {
    return YES;
  }

  // Early return if adding an address through infobar and the text field
  // contained an empty value when the profile was loaded. An empty value isn't
  // considered a valid value when adding an address manually through settings.
  if (!_addManualAddress &&
      [self requiredFieldWasEmptyOnProfileLoadForType:autofillFieldType
                            moveToAccountFromSettings:
                                moveToAccountFromSettings]) {
    return YES;
  }

  // If the required text field contains a value now, remove it from
  // `_requiredFieldsWithEmptyValue`.
  if ([_requiredFieldsWithEmptyValue containsObject:autofillFieldType] &&
      !hasEmptyValue) {
    [_requiredFieldsWithEmptyValue removeObject:autofillFieldType];

    // If `_requiredFieldsWithEmptyValue` is empty, error warnings should not be
    // ignored.
    if ([self requiredFieldsWithEmptyValuesCount] == 0) {
      _ignoreErrorMessage = NO;
    }
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

- (void)setCurrentValueForType:(NSString*)autofillFieldType
                     withValue:(NSString*)value {
  _currentValuesMap[autofillFieldType] = value;
}

- (NSString*)currentValueForType:(NSString*)autofillFieldType {
  return _currentValuesMap[autofillFieldType];
}

- (void)validateFieldsAndUpdateButtonStatus {
  if (_ignoreErrorMessage) {
    return;
  }

  BOOL shouldShowError = ([self requiredFieldsWithEmptyValuesCount] > 0);

  if (shouldShowError != _errorSectionPresented) {
    [_consumer updateErrorStatus:shouldShowError];
    _errorSectionPresented = shouldShowError;
  } else if (shouldShowError) {
    [_consumer updateErrorMessageIfRequired];
  }

  [_consumer updateButtonStatus:!shouldShowError];
}

- (void)computeFieldWasEdited:(NSString*)editedFieldType
                        value:(NSString*)value {
  BOOL contains = [_editedFields containsObject:editedFieldType];
  autofill::FieldType serverFieldType =
      [self typeNameToFieldType:editedFieldType];
  NSString* fieldOriginalValue =
      base::SysUTF16ToNSString(_autofillProfile->GetInfo(
          serverFieldType,
          GetApplicationContext()->GetApplicationLocaleStorage()->Get()));
  if (contains && [fieldOriginalValue isEqualToString:value]) {
    [_editedFields removeObject:editedFieldType];
  } else if (!contains && ![fieldOriginalValue isEqualToString:value]) {
    [_editedFields addObject:editedFieldType];
  }
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
                GetApplicationContext()->GetApplicationLocaleStorage()->Get())
      .empty();
}

// Loads the country codes and names and sets the default selected country code.
- (void)loadCountries {
  autofill::CountryComboboxModel countryModel;
  const variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  countryModel.SetCountries(
      autofill::GeoIpCountryCode(variations_service
                                     ? variations_service->GetLatestCountry()
                                     : std::string()),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      countryModel.countries();

  NSMutableArray<CountryItem*>* countryItems = [[NSMutableArray alloc]
      initWithCapacity:static_cast<NSUInteger>(countriesVector.size())];
  // Skip the first country as it appears twice in the
  // list. It was relevant to other platforms where the country does not have a
  // search option.
  for (size_t i = 1; i < countriesVector.size(); ++i) {
    if (countriesVector[i].get()) {
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
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
  _line1Required = country.requires_line1();
  _cityRequired = country.requires_city();
  _stateRequired = country.requires_state();
  _zipRequired = country.requires_zip();
}

// Fetches the fields for input and sets them to
// `inputAddressFields`/`inputNonAddressFields`.
- (void)fetchAndSetFieldsForInput {
  NSMutableArray<AutofillEditProfileField*>* addressFields =
      [[NSMutableArray alloc] init];
  NSMutableArray<AutofillEditProfileField*>* nonAddressFields =
      [[NSMutableArray alloc] init];

  i18n::addressinput::Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  std::string best_language_tag_unused;
  std::string country_code = base::SysNSStringToUTF8(_selectedCountryCode);
  autofill::AutofillCountry country(country_code);
  std::vector<autofill::AutofillAddressUIComponent> ui_components =
      ConvertAddressUiComponents(
          BuildComponents(
              country_code, localization,
              GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
              &best_language_tag_unused),
          country);
  ExtendAddressComponents(ui_components, country, localization,
                          /*include_literals=*/false);
  for (const auto& item : ui_components) {
    AutofillEditProfileField* field = [[AutofillEditProfileField alloc] init];
    field.fieldType = [self fieldTypeToTypeName:item.field];
    field.fieldLabel = base::SysUTF8ToNSString(item.name);

    if (GroupTypeOfFieldType(item.field) ==
        autofill::FieldTypeGroup::kAddress) {
      [addressFields addObject:field];
    } else {
      [nonAddressFields addObject:field];
    }
  }

  _inputNonAddressFields = nonAddressFields;
  _inputAddressFields = addressFields;
}

// Populates `_currentValuesMap` on the basis of values in `_autofillProfile`.
- (void)populateCurrentValuesMap {
  CHECK(!_errorSectionPresented);
  int totalFieldCount = [self.inputNonAddressFields count] +
                        [self.inputAddressFields count] +
                        kStaticFieldsTypes.size();
  NSMutableDictionary<NSString*, NSString*>* fieldValuesMap =
      [[NSMutableDictionary alloc] initWithCapacity:totalFieldCount];
  for (AutofillEditProfileField* field in self.inputNonAddressFields) {
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        [self typeNameToFieldType:field.fieldType],
        GetApplicationContext() -> GetApplicationLocaleStorage() -> Get()));
    fieldValuesMap[field.fieldType] = fieldValue;
  }
  for (AutofillEditProfileField* field in self.inputAddressFields) {
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        [self typeNameToFieldType:field.fieldType],
        GetApplicationContext() -> GetApplicationLocaleStorage() -> Get()));
    fieldValuesMap[field.fieldType] = fieldValue;
  }

  for (const auto& field_type : kStaticFieldsTypes) {
    NSString* fieldValue = base::SysUTF16ToNSString(_autofillProfile->GetInfo(
        field_type,
        GetApplicationContext()->GetApplicationLocaleStorage()->Get()));
    fieldValuesMap[[self fieldTypeToTypeName:field_type]] = fieldValue;
  }

  _currentValuesMap = fieldValuesMap;
  [_editedFields removeAllObjects];
}

// Returns YES if `autofillProfile` is an account profile.
- (BOOL)isAccountProfile {
  return _autofillProfile->IsAccountProfile();
}

- (autofill::FieldType)typeNameToFieldType:(NSString*)autofillFieldType {
  return autofill::TypeNameToFieldType(
      base::SysNSStringToUTF8(autofillFieldType));
}

// Populates `_requiredFieldsWithEmptyValue` with required address field types
// if adding a new address from settings.
- (void)initializeRequiredEmptyFieldsForManualAddition {
  // Early return if we are adding an address through infobar or adding a new
  // local address manually.
  if (!_addManualAddress || ![self isAccountProfile]) {
    return;
  }
  if (_line1Required) {
    [_requiredFieldsWithEmptyValue
        addObject:
            [self fieldTypeToTypeName:autofill::ADDRESS_HOME_STREET_ADDRESS]];
  }
  if (_cityRequired) {
    [_requiredFieldsWithEmptyValue
        addObject:[self fieldTypeToTypeName:autofill::ADDRESS_HOME_CITY]];
  }
  if (_stateRequired) {
    [_requiredFieldsWithEmptyValue
        addObject:[self fieldTypeToTypeName:autofill::ADDRESS_HOME_STATE]];
  }
  if (_zipRequired) {
    [_requiredFieldsWithEmptyValue
        addObject:[self fieldTypeToTypeName:autofill::ADDRESS_HOME_ZIP]];
  }
}

@end

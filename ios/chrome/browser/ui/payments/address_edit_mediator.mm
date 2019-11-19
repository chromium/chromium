// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ios/chrome/grit/ios_strings.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// If |region| is either a valid region code or name in |regions|, then the
// region's name is returned. Otherwise, returns |nil|.
NSString* NormalizeRegionName(NSString* region, NSArray<RegionData*>* regions) {
  // See if |region| is a region code, and return the region's name if that's
  // the case.
  NSIndexSet* matchingRegions =
      [regions indexesOfObjectsPassingTest:^BOOL(RegionData* testRegion,
                                                 NSUInteger index, BOOL* stop) {
        return [testRegion.regionCode isEqualToString:region];
      }];
  if ([matchingRegions count]) {
    DCHECK_EQ(1U, [matchingRegions count]);
    return [regions objectAtIndex:[matchingRegions firstIndex]].regionName;
  }

  // See if |region| is a valid region name, and return that if it is.
  matchingRegions =
      [regions indexesOfObjectsPassingTest:^BOOL(RegionData* testRegion,
                                                 NSUInteger index, BOOL* stop) {
        return [testRegion.regionName isEqualToString:region];
      }];
  if ([matchingRegions count]) {
    DCHECK_EQ(1U, [matchingRegions count]);
    return region;
  }

  // |region| was neither a valid region code or region name, so return |nil|.
  return nil;
}

}  // namespace

@interface AddressEditMediator () {
  std::unique_ptr<RegionDataLoader> _regionDataLoader;
}

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The address to be edited, if any. This pointer is not owned by this class and
// should outlive it.
@property(nonatomic, assign) autofill::AutofillProfile* address;

// The map of autofill types to the cached editor fields. Helps reuse the editor
// fields and therefore maintain their existing values when the selected country
// changes and the editor fields get updated.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The list of current editor fields.
@property(nonatomic, strong) NSMutableArray<EditorField*>* fields;

// The reference to the autofill::ADDRESS_HOME_STATE field, if any.
@property(nonatomic, strong) EditorField* regionField;

@end

@implementation AddressEditMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize countries = _countries;
@synthesize selectedCountryCode = _selectedCountryCode;
@synthesize paymentRequest = _paymentRequest;
@synthesize address = _address;
@synthesize fieldsMap = _fieldsMap;
@synthesize fields = _fields;
@synthesize regionField = _regionField;

- (instancetype)initWithPaymentRequest:(payments::PaymentRequest*)paymentRequest
                               address:(autofill::AutofillProfile*)address {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _address = address;
    _state =
        _address ? EditViewControllerStateEdit : EditViewControllerStateCreate;
    _fieldsMap = [[NSMutableDictionary alloc] init];
    [self loadCountries];
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.regionField)
    [self loadRegions];
}

- (void)setSelectedCountryCode:(NSString*)selectedCountryCode {
  if (_selectedCountryCode == selectedCountryCode)
    return;
  _selectedCountryCode = selectedCountryCode;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.regionField)
    [self loadRegions];
}

#pragma mark - CreditCardEditViewControllerDataSource

- (NSString*)title {
  if (!self.address)
    return l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS);

  if (self.paymentRequest->profile_comparator()->IsShippingComplete(
          self.address)) {
    return l10n_util::GetNSString(IDS_PAYMENTS_EDIT_ADDRESS);
  }

  return base::SysUTF16ToNSString(
      self.paymentRequest->profile_comparator()
          ->GetTitleForMissingShippingFields(*self.address));
}

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (BOOL)shouldFormatValueForAutofillUIType:(AutofillUIType)type {
  return (type == AutofillUITypeProfileHomePhoneWholeNumber);
}

- (NSString*)formatValue:(NSString*)value autofillUIType:(AutofillUIType)type {
  if (type == AutofillUITypeProfileHomePhoneWholeNumber) {
    return base::SysUTF8ToNSString(autofill::i18n::FormatPhoneForDisplay(
        base::SysNSStringToUTF8(value),
        base::SysNSStringToUTF8(self.selectedCountryCode)));
  }
  return nil;
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  return nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    switch (field.autofillUIType) {
      case AutofillUITypeProfileHomePhoneWholeNumber: {
        const std::string selectedCountryCode =
            base::SysNSStringToUTF8(self.selectedCountryCode);
        if (!autofill::IsPossiblePhoneNumber(
                base::SysNSStringToUTF16(field.value), selectedCountryCode)) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }
      default:
        break;
    }
  } else if (field.isRequired) {
    return l10n_util::GetNSString(
        IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return nil;
}

#pragma mark - RegionDataLoaderConsumer

- (void)regionDataLoaderDidSucceedWithRegions:(NSArray<RegionData*>*)regions {
  // Enable the previously disabled field.
  self.regionField.enabled = YES;

  // An autofill profile may have a region code or a region name stored as the
  // autofill::ADDRESS_HOME_STATE. If an address is being edited whose value for
  // that field type is a valid region code or a valid region name, the editor
  // field value is set to the respective region code. Otherwise, it is set to
  // nil.
  self.regionField.value = nil;
  if (self.address) {
    self.regionField.value = NormalizeRegionName(
        [self fieldValueFromProfile:self.address
                          fieldType:autofill::ADDRESS_HOME_STATE],
        regions);
  }

  // Notify the view controller asynchronously to allow for the view to update.
  __weak AddressEditMediator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    NSMutableArray<NSString*>* values = [[NSMutableArray alloc] init];

    // If the field contains no value, insert an empty value at the beginning
    // of the list of options, as the first option is selected when the UI
    // opens. Otherwise, it would be impossible for the user to select the first
    // option without selecting another option first.
    if (!weakSelf.regionField.value)
      [values addObject:@""];

    for (RegionData* region in regions)
      [values addObject:region.regionName];

    [weakSelf.consumer setOptions:@[ values ]
                   forEditorField:weakSelf.regionField];
  });
}

#pragma mark - Helper methods

// Loads the country codes and names and sets the default selected country code.
- (void)loadCountries {
  autofill::CountryComboboxModel countryModel;
  countryModel.SetCountries(*_paymentRequest->GetPersonalDataManager(),
                            base::Callback<bool(const std::string&)>(),
                            _paymentRequest->GetApplicationLocale());
  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      countryModel.countries();

  NSMutableDictionary<NSString*, NSString*>* countries =
      [[NSMutableDictionary alloc]
          initWithCapacity:static_cast<NSUInteger>(countriesVector.size())];
  for (size_t i = 0; i < countriesVector.size(); ++i) {
    if (countriesVector[i].get()) {
      [countries setObject:base::SysUTF16ToNSString(countriesVector[i]->name())
                    forKey:base::SysUTF8ToNSString(
                               countriesVector[i]->country_code())];
    }
  }
  _countries = countries;

  // If an address is being edited and it has a valid country code or a valid
  // country name for the autofill::ADDRESS_HOME_COUNTRY field, the selected
  // country code is set to the respective country code. Otherwise, the selected
  // country code is set to the default country code.
  NSString* country =
      [self fieldValueFromProfile:_address
                        fieldType:autofill::ADDRESS_HOME_COUNTRY];

  if ([countries objectForKey:country]) {
    _selectedCountryCode = country;
  } else if ([[countries allKeysForObject:country] count]) {
    DCHECK_EQ(1U, [[countries allKeysForObject:country] count]);
    _selectedCountryCode = [countries allKeysForObject:country][0];
  } else {
    _selectedCountryCode =
        base::SysUTF8ToNSString(countryModel.GetDefaultCountryCode());
  }
}

// Queries the region names based on the selected country code.
- (void)loadRegions {
  _regionDataLoader = std::make_unique<RegionDataLoader>(self);
  _regionDataLoader->LoadRegionData(
      base::SysNSStringToUTF8(self.selectedCountryCode),
      _paymentRequest->GetRegionDataLoader());
}

// Returns an array of editor fields based on the selected country code. Caches
// the fields to be reused when the selected country code changes.
- (NSArray<EditorField*>*)createEditorFields {
  self.fields = [[NSMutableArray alloc] init];

  self.regionField = nil;

  base::ListValue addressComponents;
  std::string unused;
  autofill::GetAddressComponents(
      base::SysNSStringToUTF8(self.selectedCountryCode),
      _paymentRequest->GetApplicationLocale(), &addressComponents, &unused);

  for (size_t lineIndex = 0; lineIndex < addressComponents.GetSize();
       ++lineIndex) {
    const base::ListValue* line = nullptr;
    if (!addressComponents.GetList(lineIndex, &line)) {
      NOTREACHED();
      return @[];
    }
    for (size_t componentIndex = 0; componentIndex < line->GetSize();
         ++componentIndex) {
      const base::DictionaryValue* component = nullptr;
      if (!line->GetDictionary(componentIndex, &component)) {
        NOTREACHED();
        return @[];
      }

      std::string autofillType;
      if (!component->GetString(autofill::kFieldTypeKey, &autofillType)) {
        NOTREACHED();
        return @[];
      }
      autofill::ServerFieldType serverFieldType =
          autofill::GetFieldTypeFromString(autofillType);
      AutofillUIType autofillUIType =
          AutofillUITypeFromAutofillType(serverFieldType);

      NSNumber* fieldKey = [NSNumber numberWithInt:autofillUIType];
      EditorField* field = self.fieldsMap[fieldKey];
      if (!field) {
        NSString* value =
            [self fieldValueFromProfile:self.address fieldType:serverFieldType];

        BOOL required = autofill::i18n::IsFieldRequired(
            serverFieldType, base::SysNSStringToUTF8(self.selectedCountryCode));
        field =
            [[EditorField alloc] initWithAutofillUIType:autofillUIType
                                              fieldType:EditorFieldTypeTextField
                                                  label:nil
                                                  value:value
                                               required:required];
        // Set the keyboardType and autoCapitalizationType as appropriate.
        if (autofillUIType == AutofillUITypeProfileEmailAddress) {
          field.keyboardType = UIKeyboardTypeEmailAddress;
          field.autoCapitalizationType = UITextAutocapitalizationTypeNone;
        } else if (autofillUIType == AutofillUITypeProfileHomeAddressZip) {
          field.autoCapitalizationType =
              UITextAutocapitalizationTypeAllCharacters;
        }

        [self.fieldsMap setObject:field forKey:fieldKey];
      }

      std::string fieldLabel;
      if (!component->GetString(autofill::kFieldNameKey, &fieldLabel)) {
        NOTREACHED();
        return @[];
      }
      field.label = base::SysUTF8ToNSString(fieldLabel);

      // Keep a reference to the field for the autofill::ADDRESS_HOME_STATE. Set
      // its value to "Loading..." and disable it until the regions are loaded.
      if (autofillUIType == AutofillUITypeProfileHomeAddressState) {
        self.regionField = field;
        field.value = l10n_util::GetNSString(IDS_AUTOFILL_LOADING_REGIONS);
        field.enabled = NO;
      }

      [self.fields addObject:field];

      // Insert the country field right after the full name field.
      if (autofillUIType == AutofillUITypeProfileFullName) {
        NSNumber* countryFieldKey =
            [NSNumber numberWithInt:AutofillUITypeProfileHomeAddressCountry];
        EditorField* field = self.fieldsMap[countryFieldKey];
        if (!field) {
          NSString* label = l10n_util::GetNSString(
              IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL);
          field = [[EditorField alloc]
              initWithAutofillUIType:AutofillUITypeProfileHomeAddressCountry
                           fieldType:EditorFieldTypeSelector
                               label:label
                               value:nil
                            required:YES];
          [self.fieldsMap setObject:field forKey:countryFieldKey];
        }
        field.value = self.selectedCountryCode;
        field.displayValue = self.countries[self.selectedCountryCode];
        [self.fields addObject:field];
      }
    }
  }

  // Always add phone number field at the end.
  NSNumber* phoneNumberFieldKey =
      [NSNumber numberWithInt:AutofillUITypeProfileHomePhoneWholeNumber];
  EditorField* field = self.fieldsMap[phoneNumberFieldKey];
  if (!field) {
    NSString* value =
        self.address
            ? base::SysUTF16ToNSString(
                  autofill::i18n::GetFormattedPhoneNumberForDisplay(
                      *self.address, _paymentRequest->GetApplicationLocale()))
            : nil;
    field = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_IOS_AUTOFILL_PHONE)
                         value:value
                      required:YES];
    field.keyboardType = UIKeyboardTypePhonePad;
    field.returnKeyType = UIReturnKeyDone;
    [self.fieldsMap setObject:field forKey:phoneNumberFieldKey];
  }
  [self.fields addObject:field];

  return self.fields;
}

// Takes in an autofill profile and an autofill field type and returns the
// corresponding field value. Returns nil if |profile| is nullptr.
- (NSString*)fieldValueFromProfile:(autofill::AutofillProfile*)profile
                         fieldType:(autofill::ServerFieldType)fieldType {
  return profile ? base::SysUTF16ToNSString(profile->GetInfo(
                       autofill::AutofillType(fieldType),
                       _paymentRequest->GetApplicationLocale()))
                 : nil;
}

@end

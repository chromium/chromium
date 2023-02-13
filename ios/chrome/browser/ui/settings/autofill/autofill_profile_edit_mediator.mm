// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/country_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@end

@implementation AutofillProfileEditMediator

- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditMediatorDelegate>)delegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _delegate = delegate;

    [self loadCountries];
  }

  return self;
}

#pragma mark - AutofillProfileEditTableViewControllerDelegate

- (void)autofillProfileEditViewController:
            (AutofillProfileEditTableViewController*)viewController
    willSelectCountryWithCurrentlySelectedCountry:(NSString*)country {
  [self.delegate
      willSelectCountryWithCurrentlySelectedCountry:country
                                        countryList:self.allCountries];
}

- (void)autofillProfileEditViewController:
            (AutofillProfileEditTableViewController*)viewController
                   didEditAutofillProfile:(autofill::AutofillProfile*)profile {
  _personalDataManager->UpdateProfile(*profile);
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
  // TODO(crbug.com/1407666): Skip the first country as it appears twice in the
  // list.
  for (size_t i = 0; i < countriesVector.size(); ++i) {
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
  self.allCountries = countryItems;
}

@end

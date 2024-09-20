// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCountry = kItemTypeEnumZero,
};

}  // namespace

@interface FakeAutofillProfileEditConsumer
    : NSObject <AutofillProfileEditConsumer>
// Stores the value displayed in the fields.

// YES, if the profile's record type is
// autofill::AutofillProfile::RecordType::kAccount.
@property(nonatomic, assign) BOOL accountProfile;

@property(nonatomic, assign) NSString* countrySelected;
@end

@implementation FakeAutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  self.countrySelected = country;
}

- (void)setFieldValuesMap:
    (NSMutableDictionary<NSString*, NSString*>*)fieldValueMap {
}

- (void)setAddressInputFields:
    (NSArray<AutofillProfileAddressField*>*)addressInputFields {
}

@end

@interface FakeAutofillProfileEditMediatorDelegate
    : NSObject <AutofillProfileEditMediatorDelegate>
@property(nonatomic, copy) NSArray<CountryItem*>* allCountries;
@end

@implementation FakeAutofillProfileEditMediatorDelegate

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country
                                          countryList:(NSArray<CountryItem*>*)
                                                          allCountries {
  self.allCountries = allCountries;
}

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
}

- (void)didSaveProfile {
}

@end

class AutofillProfileEditMediatorTest : public PlatformTest {
 protected:
  AutofillProfileEditMediatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    // Profile edit requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    test_profile_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    profileIOS_ = std::move(test_profile_builder).Build();
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForProfile(profileIOS_.get());
    personal_data_manager_->SetSyncServiceForTest(nullptr);

    personal_data_manager_->address_data_manager()
        .get_alternative_state_name_map_updater_for_testing()
        ->set_local_state_for_testing(local_state());

    profile_ = std::make_unique<autofill::AutofillProfile>(
        autofill::test::GetFullProfile());

    fake_autofill_profile_edit_mediator_delegate_ =
        [[FakeAutofillProfileEditMediatorDelegate alloc] init];
    fake_consumer_ = [[FakeAutofillProfileEditConsumer alloc] init];
  }

  void InitializeMediator(bool is_migration_prompt) {
    autofill_profile_edit_mediator_ = [[AutofillProfileEditMediator alloc]
           initWithDelegate:fake_autofill_profile_edit_mediator_delegate_
        personalDataManager:personal_data_manager_
            autofillProfile:profile_.get()
          isMigrationPrompt:is_migration_prompt];
    autofill_profile_edit_mediator_.consumer = fake_consumer_;
  }

  autofill::PersonalDataManager* personal_data_manager() const {
    return personal_data_manager_;
  }

  const autofill::CountryComboboxModel::CountryVector& CountriesList() {
    country_model_.SetCountries(
        *personal_data_manager(),
        base::RepeatingCallback<bool(const std::string&)>(),
        GetApplicationContext()->GetApplicationLocale());
    return country_model_.countries();
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  AutofillProfileEditMediator* autofill_profile_edit_mediator_;
  FakeAutofillProfileEditConsumer* fake_consumer_;
  FakeAutofillProfileEditMediatorDelegate*
      fake_autofill_profile_edit_mediator_delegate_;

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profileIOS_;
  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  autofill::CountryComboboxModel country_model_;
  std::unique_ptr<autofill::AutofillProfile> profile_;
};

// Tests that the consumer is initialised and informed of the required fields on
// initialisation.
TEST_F(AutofillProfileEditMediatorTest, TestRequiredFieldsOnInitialisation) {
  InitializeMediator(NO);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_LINE1"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_CITY"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_STATE"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_ZIP"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
}

// Tests that the consumer is informed of the required fields on country
// selection.
TEST_F(AutofillProfileEditMediatorTest, TestRequiredFieldsOnCountrySelection) {
  InitializeMediator(NO);
  ASSERT_EQ(
      [autofill_profile_edit_mediator_ requiredFieldsWithEmptyValuesCount], 0);
  CountryItem* countryItem = [[CountryItem alloc] initWithType:ItemTypeCountry];
  countryItem.text = @"Germany";
  countryItem.countryCode = @"DE";
  [autofill_profile_edit_mediator_ didSelectCountry:countryItem];
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_LINE1"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_CITY"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_TRUE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_STATE"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_FALSE([autofill_profile_edit_mediator_
        fieldContainsValidValue:@"ADDRESS_HOME_ZIP"
                  hasEmptyValue:YES
      moveToAccountFromSettings:NO]);
  EXPECT_NSEQ([fake_consumer_ countrySelected], @"Germany");
  ASSERT_GT(
      [autofill_profile_edit_mediator_ requiredFieldsWithEmptyValuesCount], 0);
}

// Tests that the country list used for selecting countries is correctly
// initialized.
TEST_F(AutofillProfileEditMediatorTest, TestCountriesList) {
  InitializeMediator(NO);
  [autofill_profile_edit_mediator_
      willSelectCountryWithCurrentlySelectedCountry:@"US"];
  size_t countryCount =
      [fake_autofill_profile_edit_mediator_delegate_.allCountries count];

  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      CountriesList();
  size_t country_counter_in_mediator = 0;
  for (size_t i = 1; i < countriesVector.size() - 1; i++) {
    if (!countriesVector[i].get()) {
      continue;
    }

    EXPECT_EQ(
        base::SysNSStringToUTF8(fake_autofill_profile_edit_mediator_delegate_
                                    .allCountries[country_counter_in_mediator]
                                    .countryCode),
        countriesVector[i]->country_code());
    country_counter_in_mediator++;
  }

  EXPECT_EQ(country_counter_in_mediator + 1, countryCount);
}

// Tests that the country list used for selecting countries does not contain
// sanctioned countries for the migration prompt.
TEST_F(AutofillProfileEditMediatorTest,
       TestCountriesListExcludesSanctionedOnes) {
  InitializeMediator(YES);
  [autofill_profile_edit_mediator_
      willSelectCountryWithCurrentlySelectedCountry:@"US"];
  size_t countryCount =
      [fake_autofill_profile_edit_mediator_delegate_.allCountries count];

  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      CountriesList();
  size_t country_counter_in_mediator = 0;
  for (size_t i = 1; i < countriesVector.size() - 1; i++) {
    if (!countriesVector[i].get() ||
        !personal_data_manager()
             ->address_data_manager()
             .IsCountryEligibleForAccountStorage(
                 countriesVector[i]->country_code())) {
      continue;
    }

    EXPECT_EQ(
        base::SysNSStringToUTF8(fake_autofill_profile_edit_mediator_delegate_
                                    .allCountries[country_counter_in_mediator]
                                    .countryCode),
        countriesVector[i]->country_code());
    country_counter_in_mediator++;
  }

  EXPECT_EQ(country_counter_in_mediator + 1, countryCount);
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/geo/autofill_country.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/ui/country_combobox_model.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_consumer.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
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
// If YES, denote that the particular field requires a value.
@property(nonatomic, assign) BOOL nameRequired;
@property(nonatomic, assign) BOOL line1Required;
@property(nonatomic, assign) BOOL cityRequired;
@property(nonatomic, assign) BOOL stateRequired;
@property(nonatomic, assign) BOOL zipRequired;

// Stores the value displayed in the fields.
@property(nonatomic, assign) NSString* honorificPrefix;
@property(nonatomic, assign) NSString* companyName;
@property(nonatomic, assign) NSString* fullName;
@property(nonatomic, assign) NSString* homeAddressLine1;
@property(nonatomic, assign) NSString* homeAddressLine2;
@property(nonatomic, assign) NSString* homeAddressDependentLocality;
@property(nonatomic, assign) NSString* homeAddressCity;
@property(nonatomic, assign) NSString* homeAddressAdminLevel2;
@property(nonatomic, assign) NSString* homeAddressState;
@property(nonatomic, assign) NSString* homeAddressZip;
@property(nonatomic, assign) NSString* homeAddressCountry;
@property(nonatomic, assign) NSString* homePhoneWholeNumber;
@property(nonatomic, assign) NSString* emailAddress;

// YES, if the profile's source is autofill::AutofillProfile::Source::kAccount.
@property(nonatomic, assign) BOOL accountProfile;

@property(nonatomic, assign) NSString* countrySelected;
@end

@implementation FakeAutofillProfileEditConsumer

- (void)didSelectCountry:(NSString*)country {
  self.countrySelected = country;
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
    TestChromeBrowserState::Builder test_cbs_builder;
    // Profile edit requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    test_cbs_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    personal_data_manager_->SetSyncServiceForTest(nullptr);

    personal_data_manager_->personal_data_manager_cleaner_for_testing()
        ->alternative_state_name_map_updater_for_testing()
        ->set_local_state_for_testing(local_state_.Get());

    fake_autofill_profile_edit_mediator_delegate_ =
        [[FakeAutofillProfileEditMediatorDelegate alloc] init];
    fake_consumer_ = [[FakeAutofillProfileEditConsumer alloc] init];
  }

  void InitializeMediator(bool is_migration_prompt) {
    autofill::AutofillProfile autofill_profile;
    autofill_profile_edit_mediator_ = [[AutofillProfileEditMediator alloc]
           initWithDelegate:fake_autofill_profile_edit_mediator_delegate_
        personalDataManager:personal_data_manager_
            autofillProfile:&autofill_profile
                countryCode:@"US"
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

  AutofillProfileEditMediator* autofill_profile_edit_mediator_;
  FakeAutofillProfileEditConsumer* fake_consumer_;
  FakeAutofillProfileEditMediatorDelegate*
      fake_autofill_profile_edit_mediator_delegate_;

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  autofill::PersonalDataManager* personal_data_manager_;
  autofill::CountryComboboxModel country_model_;
};

// Tests that the consumer is initialised and informed of the required fields on
// initialisation.
TEST_F(AutofillProfileEditMediatorTest, TestRequiredFieldsOnInitialisation) {
  InitializeMediator(NO);
  EXPECT_TRUE([fake_consumer_ line1Required]);
  EXPECT_TRUE([fake_consumer_ cityRequired]);
  EXPECT_TRUE([fake_consumer_ stateRequired]);
  EXPECT_TRUE([fake_consumer_ zipRequired]);
}

// Tests that the consumer is informed of the required fields on country
// selection.
TEST_F(AutofillProfileEditMediatorTest, TestRequiredFieldsOnCountrySelection) {
  InitializeMediator(NO);
  CountryItem* countryItem = [[CountryItem alloc] initWithType:ItemTypeCountry];
  countryItem.text = @"Germany";
  countryItem.countryCode = @"DE";
  [autofill_profile_edit_mediator_ didSelectCountry:countryItem];
  EXPECT_TRUE([fake_consumer_ line1Required]);
  EXPECT_TRUE([fake_consumer_ cityRequired]);
  EXPECT_FALSE([fake_consumer_ stateRequired]);
  EXPECT_TRUE([fake_consumer_ zipRequired]);
  EXPECT_NSEQ([fake_consumer_ countrySelected], @"Germany");
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
        !personal_data_manager()->IsCountryEligibleForAccountStorage(
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

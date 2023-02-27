// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_country_selection_table_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class AutofilllCountrySelectionTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    autofill_country_selection_table_view_controller_delegate_mock_ =
        OCMProtocolMock(
            @protocol(AutofillCountrySelectionTableViewControllerDelegate));
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillCountrySelectionTableViewController alloc]
        initWithDelegate:
            autofill_country_selection_table_view_controller_delegate_mock_
         selectedCountry:base::SysUTF8ToNSString(selected_country_)
            allCountries:GetFakeCountries()];
  }

  NSArray<CountryItem*>* GetFakeCountries() {
    NSMutableArray<CountryItem*>* countries = [[NSMutableArray alloc] init];
    for (const auto& [country_code, country_name] : fake_countries_) {
      CountryItem* countryItem =
          [[CountryItem alloc] initWithType:kItemTypeEnumZero];
      countryItem.text = base::SysUTF8ToNSString(country_name);
      countryItem.countryCode = base::SysUTF8ToNSString(country_code);
      [countries addObject:countryItem];
    }
    return countries;
  }

  std::string selected_country_;
  std::vector<std::pair<std::string, std::string>> fake_countries_;
  id autofill_country_selection_table_view_controller_delegate_mock_;
};

// Tests that the AutofilllCountrySelectionTableViewController is initialised
// and contains sections and items in those sections.
TEST_F(AutofilllCountrySelectionTableViewControllerTest, TestInitialisation) {
  selected_country_ = "Germany";
  fake_countries_ = {
      {"US", "United States"}, {"DE", "Germany"}, {"IN", "India"}};
  CreateController();
  CheckController();
  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ((int)fake_countries_.size(), NumberOfItemsInSection(0));
}

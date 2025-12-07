// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engines_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class SearchEngineUtilTest : public PlatformTest {
 public:
  SearchEngineUtilTest(const SearchEngineUtilTest&) = delete;
  SearchEngineUtilTest& operator=(const SearchEngineUtilTest&) = delete;

 protected:
  SearchEngineUtilTest() {
    feature_list_.InitWithFeatures({switches::kDynamicProfileCountry}, {});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    pref_service_ = profile_.get()->GetPrefs();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> pref_service_;
};

// Tests that UpdateSearchEngineCountryCodeIfNeeded doesn't set the country
// code pref if it doesn't exist and dynamic profile country is off.
TEST_F(
    SearchEngineUtilTest,
    UpdateSearchEngineCountryCodeIfNeededNoCountryCodeSet_DynamicProfileCountryIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {switches::kDynamicProfileCountry});

  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_FALSE(pref_service_->HasPrefPath(
      regional_capabilities::prefs::kCountryIDAtInstall));
}

// Tests that UpdateSearchEngineCountryCodeIfNeeded doesn't update the country
// code pref if the pref has the same value than the current country code and
// dynamic profile country is off.
TEST_F(
    SearchEngineUtilTest,
    UpdateSearchEngineCountryCodeIfNeededCountryCodeDidnotChange_DynamicProfileCountryIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {switches::kDynamicProfileCountry});

  country_codes::CountryId expected_country_code =
      country_codes::GetCurrentCountryID();
  pref_service_->SetInteger(regional_capabilities::prefs::kCountryIDAtInstall,
                            expected_country_code.Serialize());
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_EQ(expected_country_code.Serialize(),
            pref_service_->GetInteger(
                regional_capabilities::prefs::kCountryIDAtInstall));
}

// Tests that UpdateSearchEngineCountryCodeIfNeeded update the country code pref
// if the current country code is different and dynamic profile country is off.
TEST_F(
    SearchEngineUtilTest,
    UpdateSearchEngineCountryCodeIfNeededNoCountryCodeChagned_DynamicProfileCountryIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {switches::kDynamicProfileCountry});

  country_codes::CountryId expected_country_code =
      country_codes::GetCurrentCountryID();
  pref_service_->SetInteger(regional_capabilities::prefs::kCountryIDAtInstall,
                            expected_country_code.Serialize() + 1);
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_EQ(expected_country_code.Serialize(),
            pref_service_->GetInteger(
                regional_capabilities::prefs::kCountryIDAtInstall));
}

// Tests that UpdateSearchEngineCountryCodeIfNeeded doesn't update the country
// code pref if the current country code is different.
TEST_F(SearchEngineUtilTest,
       UpdateSearchEngineCountryCodeIfNeededNoCountryCodeChagned) {
  country_codes::CountryId expected_country_code =
      country_codes::GetCurrentCountryID();
  pref_service_->SetInteger(regional_capabilities::prefs::kCountryIDAtInstall,
                            expected_country_code.Serialize() + 1);
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_EQ(expected_country_code.Serialize() + 1,
            pref_service_->GetInteger(
                regional_capabilities::prefs::kCountryIDAtInstall));
}

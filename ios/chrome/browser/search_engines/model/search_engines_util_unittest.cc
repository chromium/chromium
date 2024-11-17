// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engines_util.h"

#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class SearchEngineUtilTest : public PlatformTest {
 public:
  SearchEngineUtilTest(const SearchEngineUtilTest&) = delete;
  SearchEngineUtilTest& operator=(const SearchEngineUtilTest&) = delete;

 protected:
  SearchEngineUtilTest() {}

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    pref_service_ = profile_.get()->GetPrefs();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> pref_service_;
};

// Tests that UpdateSearchEngineCountryCodeIfNeeded doesn't set the country
// code pref if it doesn't exist.
TEST_F(SearchEngineUtilTest,
       UpdateSearchEngineCountryCodeIfNeededNoCountryCodeSet) {
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_FALSE(pref_service_->HasPrefPath(country_codes::kCountryIDAtInstall));
}

// Tests that UpdateSearchEngineCountryCodeIfNeeded doesn't update the country
// code pref if the pref has the same value than the current country code.
TEST_F(SearchEngineUtilTest,
       UpdateSearchEngineCountryCodeIfNeededCountryCodeDidnotChange) {
  int expected_country_code = country_codes::GetCurrentCountryID();
  pref_service_->SetInteger(country_codes::kCountryIDAtInstall,
                            expected_country_code);
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_EQ(expected_country_code,
            pref_service_->GetInteger(country_codes::kCountryIDAtInstall));
}

// Tests that UpdateSearchEngineCountryCodeIfNeeded update the country code pref
// if the current country code is different.
TEST_F(SearchEngineUtilTest,
       UpdateSearchEngineCountryCodeIfNeededNoCountryCodeChagned) {
  int expected_country_code = country_codes::GetCurrentCountryID();
  pref_service_->SetInteger(country_codes::kCountryIDAtInstall,
                            expected_country_code + 1);
  search_engines::UpdateSearchEngineCountryCodeIfNeeded(pref_service_);
  EXPECT_EQ(expected_country_code,
            pref_service_->GetInteger(country_codes::kCountryIDAtInstall));
}

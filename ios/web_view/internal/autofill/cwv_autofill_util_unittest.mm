// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_util.h"

#import <Foundation/Foundation.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace ios_web_view {

using CWVAutofillUtilTest = PlatformTest;

// Tests that the country code comes from the locale's region.
TEST_F(CWVAutofillUtilTest, CountryCodeFromLocaleUsesRegion) {
  EXPECT_EQ("US", CWVCountryCodeFromLocale(
                      [NSLocale localeWithLocaleIdentifier:@"es_US"]));
  EXPECT_EQ("CA", CWVCountryCodeFromLocale(
                      [NSLocale localeWithLocaleIdentifier:@"fr_CA"]));
}

// Tests that a region override takes precedence over the locale's region.
TEST_F(CWVAutofillUtilTest, CountryCodeFromLocaleUsesRegionOverride) {
  EXPECT_EQ("US", CWVCountryCodeFromLocale([NSLocale
                      localeWithLocaleIdentifier:@"es_419@rg=uszzzz"]));
}

// Tests that the country code is empty when the locale has no region.
TEST_F(CWVAutofillUtilTest, CountryCodeFromLocaleIsEmptyWithoutRegion) {
  EXPECT_EQ("", CWVCountryCodeFromLocale(
                    [NSLocale localeWithLocaleIdentifier:@"es"]));
}

// Tests that the country code is empty when the region is not a country.
TEST_F(CWVAutofillUtilTest, CountryCodeFromLocaleIsEmptyForNonCountryRegion) {
  EXPECT_EQ("", CWVCountryCodeFromLocale(
                    [NSLocale localeWithLocaleIdentifier:@"es_419"]));
  EXPECT_EQ("", CWVCountryCodeFromLocale(
                    [NSLocale localeWithLocaleIdentifier:@"en_001"]));
}

}  // namespace ios_web_view

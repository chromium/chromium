// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/metrics/private_metrics/private_metrics_features.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface PumaTestCase : ChromeTestCase
@end

@implementation PumaTestCase

// TODO(crbug.com/461922774): This test currently checks the country ID
// via a test-only AppInterface method. This should be converted to a browser
// test that inspects the generated UMA proto directly.

// Enable the PUMA features and set the country code for the test.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      metrics::private_metrics::kPrivateMetricsPuma);
  config.features_enabled.push_back(
      metrics::private_metrics::kPrivateMetricsPumaRc);
  config.features_disabled.push_back(
      metrics::private_metrics::kPrivateMetricsFeature);
  config.additional_args.push_back(base::SysNSStringToUTF8([NSString
      stringWithFormat:@"--%s=BE", switches::kSearchEngineChoiceCountry]));
  return config;
}

#pragma mark - Tests

// Tests that the PumaService can correctly retrieve the profile country ID.
- (void)testGetCountryId {
  GREYAssertEqualObjects(@"BE", [MetricsAppInterface pumaCountryIdForTesting],
                         @"Failed to get correct country ID.");
}

@end

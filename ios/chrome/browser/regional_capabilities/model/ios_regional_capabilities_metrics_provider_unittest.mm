// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/regional_capabilities/model/ios_regional_capabilities_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/country_codes/country_codes.h"
#import "components/regional_capabilities/program_settings.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace regional_capabilities {

namespace {

std::unique_ptr<KeyedService> BuildServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS* profile) {
  return CreateServiceWithFakeClient(*profile->GetPrefs(), country_id);
}

}  // namespace

class IOSRegionalCapabilitiesMetricsProviderTest : public PlatformTest {
 protected:
  // Creates a personal testing profile and sets its active regional program.
  void CreatePersonalTestingProfile(country_codes::CountryId country_id) {
    CHECK(profile_manager_.GetLoadedProfiles().empty());
    profile_manager_.AddProfileWithBuilder(CreateProfileBuilder(country_id));
  }

  // Creates a managed testing profile and sets its active regional program.
  void CreateManagedTestingProfile(country_codes::CountryId country_id) {
    AddManagedProfileWithBuilder(profile_manager_,
                                 [FakeSystemIdentity fakeManagedIdentity],
                                 CreateProfileBuilder(country_id));
  }

  TestProfileIOS::Builder CreateProfileBuilder(
      country_codes::CountryId country_id) {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::RegionalCapabilitiesServiceFactory::GetInstance(),
        base::BindOnce(&BuildServiceWithFakeClient, country_id));
    return test_profile_builder;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::HistogramTester histogram_tester_;
  IOSRegionalCapabilitiesMetricsProvider metrics_provider_;
};

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, NoProfiles_Default) {
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.Debug.HasActiveRegionalProgram", false, 1);
  histogram_tester_.ExpectTotalCount(
      "RegionalCapabilities.ActiveRegionalProgram2", 0);
}

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, SingleWaffle_Waffle) {
  CreatePersonalTestingProfile(country_codes::CountryId("FR"));
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);
}

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, SingleDefault_Default) {
  CreatePersonalTestingProfile(country_codes::CountryId("US"));
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kDefault, 1);
}

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, MultipleWaffle_Waffle) {
  CreatePersonalTestingProfile(country_codes::CountryId("FR"));
  CreateManagedTestingProfile(country_codes::CountryId("FR"));
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);
}

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, DefaultAndWaffle_Waffle) {
  CreatePersonalTestingProfile(country_codes::CountryId("US"));
  CreateManagedTestingProfile(country_codes::CountryId("FR"));
  metrics_provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kWaffle, 1);
}

TEST_F(IOSRegionalCapabilitiesMetricsProviderTest, WaffleAndTaiyaki_Mixed) {
  if (!IsClientCompatibleWithProgram(Program::kTaiyaki)) {
    GTEST_SKIP() << "Device type does not support the Taiyaki program.";
  }
  CreatePersonalTestingProfile(country_codes::CountryId("FR"));
  CreateManagedTestingProfile(country_codes::CountryId("JP"));

  metrics_provider_.ProvideCurrentSessionData(nullptr);

  histogram_tester_.ExpectUniqueSample(
      "RegionalCapabilities.ActiveRegionalProgram2",
      ActiveRegionalProgram::kMixed, 1);
}

}  // namespace regional_capabilities
